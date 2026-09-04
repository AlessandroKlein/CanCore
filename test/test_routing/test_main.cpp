/*
 * Pruebas del motor de enrutamiento multi-protocolo.
 *
 * Valida CanonicalFrame, RouteTable (serialize/deserialize + CRC) y el
 * RoutingEngine con un CanNode real sobre el bus virtual.
 */

#include <unity.h>

#include "PCD_CAN.h"

using namespace pcd;

namespace {

bool g_sink_called = false;
CanonicalFrame g_sink_frame;

bool relayHandler(uint8_t, uint8_t, uint32_t, float &value, void *ctx) {
    if (ctx == 0) {
        return false;
    }
    struct Ctx { bool *state; };
    Ctx *state_ctx = static_cast<Ctx *>(ctx);
    value = *(state_ctx->state) ? 1.0f : 0.0f;
    *(state_ctx->state) = true;
    return true;
}

#define ACTUATOR_REGISTER(node, ctx) \
    do { \
        (node).registerResource(RES_RELAY, 0x01, relayHandler, (ctx)); \
    } while (0)

void sink(const CanonicalFrame &frame, void *ctx) {
    (void)ctx;
    g_sink_called = true;
    g_sink_frame = frame;
}

/* ---- MockTransport para tests de TunnelEngine (reutilizado) ---- */
struct MockTransp : public ITunnelTransport {
    bool send(uint16_t, const uint8_t *, size_t) override { return true; }
    bool receive(uint16_t &, uint8_t *, size_t, size_t &) override { return false; }
    bool available() const override { return false; }
};

}  // namespace

void test_canonical_from_can_state() {
    const CanFrame frame = makeStateBroadcast(0x0016, RES_RELAY, 0x01, 1.0f);
    const CanonicalFrame c = CanonicalFrame::fromCanFrame(frame);
    TEST_ASSERT_EQUAL_UINT8(PROTO_CAN, c.protocol);
    TEST_ASSERT_EQUAL_UINT16(0x0016, c.source_id);
    TEST_ASSERT_EQUAL_UINT8(RES_RELAY, c.resource);
    TEST_ASSERT_EQUAL_UINT8(0x01, c.channel);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, c.value);
    TEST_ASSERT_EQUAL_UINT8(0, c.is_command);
}

void test_canonical_from_can_command() {
    const CanFrame frame = makeCommand(0x0005, 0x16, RES_DIMMER, 0x02, ACT_SET_VALUE, 80);
    const CanonicalFrame c = CanonicalFrame::fromCanFrame(frame);
    TEST_ASSERT_EQUAL_UINT8(1, c.is_command);
    TEST_ASSERT_EQUAL_UINT8(ACT_SET_VALUE, c.action);
    TEST_ASSERT_EQUAL_UINT32(80, c.param);
}

void test_route_table_match_and_serialize() {
    RouteTable table;
    MappingRule rule;
    rule.src_protocol = PROTO_CAN;
    rule.src_id = 0x0005;
    rule.src_resource = RES_DIGITAL_INPUT;
    rule.src_channel = 0x01;
    rule.dst_protocol = PROTO_CAN;
    rule.dst_id = 0x0016;
    rule.dst_resource = RES_RELAY;
    rule.dst_channel = 0x02;
    rule.dst_action = ACT_TOGGLE;
    TEST_ASSERT_TRUE(table.addRule(rule));

    /* Match */
    CanonicalFrame incoming;
    incoming.protocol = PROTO_CAN;
    incoming.source_id = 0x0005;
    incoming.resource = RES_DIGITAL_INPUT;
    incoming.channel = 0x01;
    MappingRule out;
    TEST_ASSERT_TRUE(table.match(incoming, out));
    TEST_ASSERT_EQUAL_UINT8(PROTO_CAN, out.dst_protocol);
    TEST_ASSERT_EQUAL_UINT16(0x0016, out.dst_id);
    TEST_ASSERT_EQUAL_UINT8(ACT_TOGGLE, out.dst_action);

    /* Serialize/deserialize roundtrip */
    uint8_t buffer[512];
    const uint16_t len = 4 + 20 + 2; /* header + 1 rule + crc */
    TEST_ASSERT_TRUE(table.serialize(buffer, sizeof(buffer)));
    RouteTable reloaded;
    TEST_ASSERT_TRUE(reloaded.deserialize(buffer, len));
    TEST_ASSERT_EQUAL_UINT8(1, reloaded.count());
    TEST_ASSERT_EQUAL_UINT8(PROTO_CAN, reloaded.rule(0).src_protocol);
    TEST_ASSERT_EQUAL_UINT16(0x0005, reloaded.rule(0).src_id);
    TEST_ASSERT_EQUAL_UINT8(ACT_TOGGLE, reloaded.rule(0).dst_action);
}

void test_route_table_rejects_bad_crc() {
    RouteTable table;
    MappingRule rule;
    rule.src_protocol = PROTO_MODBUS;
    rule.dst_protocol = PROTO_CAN;
    TEST_ASSERT_TRUE(table.addRule(rule));

    uint8_t buffer[512];
    const uint16_t len = 4 + 20 + 2;
    TEST_ASSERT_TRUE(table.serialize(buffer, sizeof(buffer)));
    buffer[len - 1] ^= 0xFF; /* corromper CRC */

    RouteTable reloaded;
    TEST_ASSERT_FALSE(reloaded.deserialize(buffer, len));
}

void test_routing_engine_routes_can_to_can() {
    VirtualBus wire;
    NativeCanBus bus_a(&wire);
    NativeCanBus bus_b(&wire);

    CanNode gateway(bus_a, 0x0001);
    CanNode actuator(bus_b, 0x0016);
    gateway.begin();
    actuator.begin();

    RouteTable table;
    MappingRule rule;
    rule.src_protocol = PROTO_CAN;
    rule.src_id = 0x0005;
    rule.src_resource = RES_DIGITAL_INPUT;
    rule.src_channel = 0x01;
    rule.dst_protocol = PROTO_CAN;
    rule.dst_id = 0x0016;
    rule.dst_resource = RES_RELAY;
    rule.dst_channel = 0x01;
    rule.dst_action = ACT_TOGGLE;
    table.addRule(rule);

    RoutingEngine engine(table);
    engine.attach(gateway, 0x0001);
    gateway.onAnyFrame([](const CanFrame &frame, void *ctx) {
                           static_cast<RoutingEngine *>(ctx)->onCanFrame(frame);
                       },
                       &engine);

    bool relay_state = false;
    struct Ctx { bool *state; };
    Ctx ctx = {&relay_state};
    ACTUATOR_REGISTER(actuator, &ctx);

    actuator.applyLocal(RES_RELAY, 0x01, ACT_OFF, 0);

    /* Simular una pulsacion del nodo 0x0005, entrada 1. */
    const CanFrame event = makeInputEvent(0x0005, 0x01, EVT_SHORT_CLICK);
    bus_a.send(event);
    gateway.poll(1000);
    actuator.poll(1000);

    TEST_ASSERT_TRUE(relay_state);
}

void test_routing_engine_routes_to_sink_non_can() {
    RouteTable table;
    MappingRule rule;
    rule.src_protocol = PROTO_CAN;
    rule.src_id = 0x0005;
    rule.src_resource = RES_DIGITAL_INPUT;
    rule.src_channel = 0x01;
    rule.dst_protocol = PROTO_KNX;
    rule.dst_id = 0x0001; /* direccion de grupo */
    rule.dst_resource = RES_RELAY;
    rule.dst_channel = 0x00;
    rule.dst_action = ACT_ON;
    table.addRule(rule);

    RoutingEngine engine(table);
    engine.setSink(&sink, 0);

    CanonicalFrame incoming;
    incoming.protocol = PROTO_CAN;
    incoming.source_id = 0x0005;
    incoming.resource = RES_DIGITAL_INPUT;
    incoming.channel = 0x01;

    g_sink_called = false;
    TEST_ASSERT_TRUE(engine.process(incoming));
    TEST_ASSERT_TRUE(g_sink_called);
    TEST_ASSERT_EQUAL_UINT8(PROTO_KNX, g_sink_frame.protocol);
    TEST_ASSERT_EQUAL_UINT8(RES_RELAY, g_sink_frame.resource);
    TEST_ASSERT_EQUAL_UINT8(ACT_ON, g_sink_frame.action);
}

void test_gateway_attach_bridge() {
    VirtualBus wire;
    NativeCanBus bus(&wire);
    CanNode node(bus, 0x0001);
    node.begin();

    RouteTable table;
    Gateway gateway(node, table);
    GatewayConfig config;
    gateway.begin(config);

    /* Un bridge ficticio que cuenta procesamientos. */
    static int process_count = 0;
    struct DummyBridge : public IBridge {
        bool process(const CanonicalFrame &) override { ++process_count; return true; }
        bool buildCanonical(CanonicalFrame &) override { return false; }
        bool available() const override { return false; }
    };
    DummyBridge bridge;
    TEST_ASSERT_TRUE(gateway.attachBridge(bridge));

    /* Las tramas MSG_STATE llegan al bridge. */
    const CanFrame state = makeStateBroadcast(0x0016, RES_RELAY, 0x01, 1.0f);
    process_count = 0;
    gateway.onCanFrame(state);
    TEST_ASSERT_EQUAL_INT(1, process_count);
}

void test_mqtt_bridge_parses_incoming_command() {
    struct MockMqttTransport : public IMqttTransport {
        static bool publishFn(const char *, const uint8_t *, size_t, bool) { return true; }
        static bool subscribeFn(const char *) { return true; }
        static void onMessageFn(const char *, const uint8_t *, size_t, void *) {}
        MockMqttTransport() {
            publish = &publishFn;
            subscribe = &subscribeFn;
            onMessage = &onMessageFn;
            ctx = 0;
        }
    };

    MockMqttTransport transport;
    MqttBridge bridge(transport);
    bridge.onIncomingMessage("pcd/5/16/1/set", reinterpret_cast<const uint8_t *>("1"), 1);

    CanonicalFrame out;
    TEST_ASSERT_TRUE(bridge.buildCanonical(out));
    TEST_ASSERT_EQUAL_UINT8(PROTO_MQTT, out.protocol);
    TEST_ASSERT_EQUAL_UINT16(5, out.source_id);
    TEST_ASSERT_EQUAL_UINT8(RES_RELAY, out.resource);
    TEST_ASSERT_EQUAL_UINT8(1, out.channel);
    TEST_ASSERT_TRUE(out.is_command);
    TEST_ASSERT_EQUAL_UINT8(ACT_ON, out.action);
}

void test_modbus_bridge_translates_can_to_registers() {
    struct MockModbusMap : public IModbusRegisterMap {
        uint16_t holding[16];
        bool coils[16];

        MockModbusMap() {
            for (size_t i = 0; i < 16; ++i) {
                holding[i] = 0;
                coils[i] = false;
            }
            readRegister = 0;
            readCoil = 0;
            writeRegister = &writeRegisterFn;
            writeCoil = &writeCoilFn;
            ctx = this;
        }

        static bool writeRegisterFn(uint16_t address, uint16_t value, void *ctx) {
            MockModbusMap *self = static_cast<MockModbusMap *>(ctx);
            if (address >= 16) return false;
            self->holding[address] = value;
            return true;
        }

        static bool writeCoilFn(uint16_t address, bool value, void *ctx) {
            MockModbusMap *self = static_cast<MockModbusMap *>(ctx);
            if (address >= 16) return false;
            self->coils[address] = value;
            return true;
        }
    };

    MockModbusMap map;
    ModbusBridge bridge(map, 0x0100);

    CanonicalFrame frame;
    frame.protocol = PROTO_CAN;
    frame.source_id = 0x0016;
    frame.resource = RES_RELAY;
    frame.channel = 0x01;
    frame.action = ACT_ON;
    frame.is_command = 1;

    TEST_ASSERT_TRUE(bridge.process(frame));
    TEST_ASSERT_TRUE(map.coils[0]);

    bridge.queueIncoming(0x0005, RES_DIMMER, 0x02, ACT_SET_VALUE, 75);
    CanonicalFrame out;
    TEST_ASSERT_TRUE(bridge.buildCanonical(out));
    TEST_ASSERT_EQUAL_UINT8(PROTO_MODBUS, out.protocol);
    TEST_ASSERT_EQUAL_UINT16(0x0005, out.source_id);
    TEST_ASSERT_EQUAL_UINT8(RES_DIMMER, out.resource);
    TEST_ASSERT_EQUAL_UINT8(0x02, out.channel);
    TEST_ASSERT_EQUAL_UINT8(ACT_SET_VALUE, out.action);
    TEST_ASSERT_EQUAL_UINT32(75, out.param);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_canonical_from_can_state);
    RUN_TEST(test_canonical_from_can_command);
    RUN_TEST(test_route_table_match_and_serialize);
    RUN_TEST(test_route_table_rejects_bad_crc);
    RUN_TEST(test_routing_engine_routes_can_to_can);
    RUN_TEST(test_routing_engine_routes_to_sink_non_can);
    RUN_TEST(test_gateway_attach_bridge);
    RUN_TEST(test_mqtt_bridge_parses_incoming_command);
    RUN_TEST(test_modbus_bridge_translates_can_to_registers);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
