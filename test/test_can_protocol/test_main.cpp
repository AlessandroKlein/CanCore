#include <unity.h>

#include "can_node.h"
#include "can_protocol.h"
#include "hal/hal_can_native.h"

using namespace pcd;

/* ------------------------------------------------------------------ */
/* Identificador de 29 bits                                            */
/* ------------------------------------------------------------------ */

void test_id_roundtrip(void) {
    const uint32_t raw = encodeId(PRIO_REALTIME, MSG_EVENT, 0x16, 0x0005);
    const CanId id = decodeId(raw);
    TEST_ASSERT_EQUAL_UINT8(PRIO_REALTIME, id.priority);
    TEST_ASSERT_EQUAL_UINT8(MSG_EVENT, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(0x16, id.target);
    TEST_ASSERT_EQUAL_UINT16(0x0005, id.source);
}

void test_id_field_layout(void) {
    /* prioridad 5, tipo 0x03, destino 0x7E, origen 0x3FFF */
    const uint32_t raw = encodeId(5, 0x03, 0x7E, 0x3FFF);
    const uint32_t expected = (5UL << 26) | (0x03UL << 21) | (0x7EUL << 14) | 0x3FFFUL;
    TEST_ASSERT_EQUAL_HEX32(expected, raw);
    TEST_ASSERT_EQUAL_HEX32(0, raw & ~kExtendedIdMask);
}

void test_id_fields_do_not_overflow(void) {
    /* Los valores fuera de rango se recortan al ancho de su campo. */
    const uint32_t raw = encodeId(0xFF, 0xFF, 0xFF, 0xFFFF);
    const CanId id = decodeId(raw);
    TEST_ASSERT_EQUAL_UINT8(0x07, id.priority);
    TEST_ASSERT_EQUAL_UINT8(0x1F, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(0x7F, id.target);
    TEST_ASSERT_EQUAL_UINT16(0x3FFF, id.source);
}

void test_priority_dominates_arbitration(void) {
    /* Menor identificador numerico gana el arbitraje del bus. */
    const uint32_t critical = encodeId(PRIO_CRITICAL, MSG_EVENT, 0x10, 0x3FFF);
    const uint32_t background = encodeId(PRIO_BACKGROUND, MSG_EVENT, 0x01, 0x0001);
    TEST_ASSERT_TRUE(critical < background);
}

void test_address_validation(void) {
    TEST_ASSERT_TRUE(isValidTarget(kBroadcastTarget));
    TEST_ASSERT_TRUE(isValidTarget(0x7E));
    TEST_ASSERT_FALSE(isValidTarget(0x7F));
    TEST_ASSERT_FALSE(isValidSource(0x0000));
    TEST_ASSERT_TRUE(isValidSource(0x3FFF));
}

/* ------------------------------------------------------------------ */
/* Payload                                                             */
/* ------------------------------------------------------------------ */

void test_float_big_endian_roundtrip(void) {
    uint8_t buffer[4];
    writeFloatBE(buffer, 23.5f);
    TEST_ASSERT_EQUAL_HEX8(0x41, buffer[0]); /* 23.5f = 0x41BC0000 */
    TEST_ASSERT_EQUAL_HEX8(0xBC, buffer[1]);
    TEST_ASSERT_EQUAL_FLOAT(23.5f, readFloatBE(buffer));
}

void test_command_frame(void) {
    const CanFrame frame = makeCommand(0x0005, 0x16, RES_RELAY, 0x01, ACT_ON, 1500);
    const CanId id = frame.fields();
    TEST_ASSERT_EQUAL_UINT8(MSG_EVENT, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(PRIO_REALTIME, id.priority);
    TEST_ASSERT_EQUAL_UINT8(0x16, id.target);
    TEST_ASSERT_EQUAL_UINT16(0x0005, id.source);
    TEST_ASSERT_EQUAL_UINT8(RES_RELAY, frame.resource());
    TEST_ASSERT_EQUAL_UINT8(0x01, frame.channel());
    TEST_ASSERT_EQUAL_UINT8(ACT_ON, frame.data[2]);
    TEST_ASSERT_EQUAL_UINT32(1500, frameParam(frame));
    TEST_ASSERT_EQUAL_UINT8(8, frame.dlc);
}

void test_state_broadcast_frame(void) {
    const CanFrame frame =
        makeStateBroadcast(0x0016, RES_ENV_SENSOR, 0x01, 21.75f, DIAG_SENSOR_ERROR);
    const CanId id = frame.fields();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(kBroadcastTarget, id.target);
    TEST_ASSERT_EQUAL_FLOAT(21.75f, frameValue(frame));
    TEST_ASSERT_EQUAL_HEX16(DIAG_SENSOR_ERROR, frameFlags(frame));
}

void test_telemetry_uses_low_priority(void) {
    const CanFrame frame = makeTelemetry(0x0016, RES_GAS_SENSOR, 0x01, 412.0f);
    TEST_ASSERT_EQUAL_UINT8(PRIO_TELEMETRY, frame.fields().priority);
}

void test_ota_data_frame(void) {
    const uint8_t chunk[7] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03};
    const CanFrame frame = makeOtaData(0x0001, 0x16, 0x2A, chunk, sizeof(chunk));
    const CanId id = frame.fields();
    TEST_ASSERT_EQUAL_UINT8(MSG_OTA, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(PRIO_BACKGROUND, id.priority);
    TEST_ASSERT_EQUAL_UINT8(0x2A, frame.data[0]);
    TEST_ASSERT_EQUAL_UINT8(8, frame.dlc);
    TEST_ASSERT_EQUAL_HEX8(0xDE, frame.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, frame.data[7]);
}

void test_ota_data_truncates_oversized_chunk(void) {
    const uint8_t chunk[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const CanFrame frame = makeOtaData(0x0001, 0x16, 0x00, chunk, sizeof(chunk));
    TEST_ASSERT_EQUAL_UINT8(8, frame.dlc);
    TEST_ASSERT_EQUAL_UINT8(7, frame.data[7]);
}

void test_config_frame(void) {
    const uint8_t rule[6] = {0x05, 0x30, 0x01, 0x10, 0x02, ACT_TOGGLE};
    const CanFrame frame = makeConfig(0x0001, 0x16, 0x01, rule, sizeof(rule));
    const CanId id = frame.fields();
    TEST_ASSERT_EQUAL_UINT8(MSG_CONFIG, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(PRIO_CONFIG, id.priority);
    TEST_ASSERT_EQUAL_UINT8(RES_CONFIG, frame.resource());
    TEST_ASSERT_EQUAL_UINT8(0x01, frame.channel());
    TEST_ASSERT_EQUAL_UINT8(ACT_TOGGLE, frame.data[7]);
}

/* ------------------------------------------------------------------ */
/* Filtros                                                             */
/* ------------------------------------------------------------------ */

void test_filter_by_target(void) {
    const CanFilter filter = CanFilter::forTarget(0x16);
    TEST_ASSERT_TRUE(filter.accepts(encodeId(PRIO_REALTIME, MSG_EVENT, 0x16, 0x0005)));
    TEST_ASSERT_FALSE(filter.accepts(encodeId(PRIO_REALTIME, MSG_EVENT, 0x17, 0x0005)));
}

void test_filter_by_msg_type(void) {
    const CanFilter filter = CanFilter::forMsgType(MSG_OTA);
    TEST_ASSERT_TRUE(filter.accepts(encodeId(PRIO_BACKGROUND, MSG_OTA, 0x16, 0x0001)));
    TEST_ASSERT_FALSE(filter.accepts(encodeId(PRIO_REALTIME, MSG_EVENT, 0x16, 0x0001)));
}

/* ------------------------------------------------------------------ */
/* Nodo: lazo comando -> ejecucion -> difusion                          */
/* ------------------------------------------------------------------ */

namespace {

struct RelayState {
    bool on;
    uint8_t commands;
};

RelayState g_relay = {false, 0};

bool relayHandler(uint8_t channel, uint8_t action, uint32_t param, float &out_value, void *ctx) {
    (void)channel;
    (void)param;
    RelayState *state = static_cast<RelayState *>(ctx);
    ++state->commands;
    switch (action) {
        case ACT_OFF: state->on = false; break;
        case ACT_ON: state->on = true; break;
        case ACT_TOGGLE: state->on = !state->on; break;
        default: return false;
    }
    out_value = state->on ? 1.0f : 0.0f;
    return true;
}

struct ScreenState {
    uint8_t updates;
    float last_value;
};

ScreenState g_screen = {0, -1.0f};

void screenListener(const CanFrame &frame, float value, void *ctx) {
    (void)frame;
    ScreenState *state = static_cast<ScreenState *>(ctx);
    ++state->updates;
    state->last_value = value;
}

}  // namespace

void test_feedback_loop_between_nodes(void) {
    g_relay.on = false;
    g_relay.commands = 0;
    g_screen.updates = 0;
    g_screen.last_value = -1.0f;

    VirtualBus wire;
    NativeCanBus field_phy(&wire);
    NativeCanBus screen_phy(&wire);

    CanNode field(field_phy, 0x0016);
    CanNode screen(screen_phy, 0x0005);
    TEST_ASSERT_EQUAL(CAN_OK, field.begin());
    TEST_ASSERT_EQUAL(CAN_OK, screen.begin());

    TEST_ASSERT_TRUE(field.registerResource(RES_RELAY, 0x01, relayHandler, &g_relay));
    TEST_ASSERT_TRUE(screen.subscribe(0x0016, RES_RELAY, 0x01, screenListener, &g_screen));

    /* La pantalla emite el comando; el nodo lo ejecuta y difunde su estado. */
    TEST_ASSERT_EQUAL(CAN_OK, screen.sendCommand(0x16, RES_RELAY, 0x01, ACT_ON));
    field.poll(1000);
    screen.poll(1000);

    TEST_ASSERT_TRUE(g_relay.on);
    TEST_ASSERT_EQUAL_UINT8(1, g_relay.commands);
    TEST_ASSERT_EQUAL_UINT8(1, g_screen.updates);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, g_screen.last_value);
}

void test_local_action_syncs_the_bus(void) {
    g_relay.on = false;
    g_screen.updates = 0;

    VirtualBus wire;
    NativeCanBus field_phy(&wire);
    NativeCanBus screen_phy(&wire);

    CanNode field(field_phy, 0x0016);
    CanNode screen(screen_phy, 0x0005);
    field.begin();
    screen.begin();
    field.registerResource(RES_RELAY, 0x01, relayHandler, &g_relay);
    screen.subscribe(kAnySource, RES_RELAY, kAnyChannel, screenListener, &g_screen);

    /* Pulsacion de la tecla fisica cableada al nodo de campo. */
    TEST_ASSERT_TRUE(field.applyLocal(RES_RELAY, 0x01, ACT_TOGGLE));
    screen.poll(1000);

    TEST_ASSERT_TRUE(g_relay.on);
    TEST_ASSERT_EQUAL_UINT8(1, g_screen.updates);
}

void test_node_ignores_frames_for_other_targets(void) {
    g_relay.on = false;
    g_relay.commands = 0;

    VirtualBus wire;
    NativeCanBus field_phy(&wire);
    NativeCanBus other_phy(&wire);
    CanNode field(field_phy, 0x0016);
    CanNode other(other_phy, 0x0005);
    field.begin();
    other.begin();
    field.registerResource(RES_RELAY, 0x01, relayHandler, &g_relay);

    other.sendCommand(0x17, RES_RELAY, 0x01, ACT_ON);
    field.poll(1000);

    TEST_ASSERT_FALSE(g_relay.on);
    TEST_ASSERT_EQUAL_UINT8(0, g_relay.commands);
}

void test_group_scene_reaches_every_node(void) {
    g_relay.on = false;

    VirtualBus wire;
    NativeCanBus field_phy(&wire);
    NativeCanBus gateway_phy(&wire);
    CanNode field(field_phy, 0x0016);
    field.begin();
    field.registerResource(RES_RELAY, 0x01, relayHandler, &g_relay);

    NativeCanBus &gateway = gateway_phy;
    gateway.begin();
    CanFrame scene;
    scene.id = encodeId(PRIO_REALTIME, MSG_GROUP, kBroadcastTarget, 0x0001);
    scene.dlc = kPayloadSize;
    scene.data[0] = RES_RELAY;
    scene.data[1] = 0x01;
    scene.data[2] = ACT_ON;
    gateway.send(scene);

    field.poll(1000);
    TEST_ASSERT_TRUE(g_relay.on);
}

void test_heartbeat_is_periodic(void) {
    VirtualBus wire;
    NativeCanBus phy(&wire);
    CanNode node(phy, 0x0016);
    node.begin();

    node.poll(0);
    const size_t after_first = phy.sentCount();
    TEST_ASSERT_EQUAL_UINT32(1, after_first);

    node.poll(CAN_HEARTBEAT_PERIOD_MS - 1);
    TEST_ASSERT_EQUAL_UINT32(after_first, phy.sentCount());

    node.poll(CAN_HEARTBEAT_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(after_first + 1, phy.sentCount());

    const CanFrame hb = phy.lastSent();
    TEST_ASSERT_EQUAL_UINT8(MSG_HEARTBEAT, hb.fields().msg_type);
    TEST_ASSERT_EQUAL_UINT32(CAN_HEARTBEAT_PERIOD_MS / 1000UL, readUint32BE(&hb.data[3]));
}

void test_config_listener_receives_sdo(void) {
    static uint8_t received = 0;
    struct Local {
        static void onConfig(const CanFrame &frame, void *ctx) {
            (void)frame;
            ++(*static_cast<uint8_t *>(ctx));
        }
    };

    VirtualBus wire;
    NativeCanBus node_phy(&wire);
    NativeCanBus gateway_phy(&wire);
    CanNode node(node_phy, 0x0016);
    node.begin();
    node.onConfig(&Local::onConfig, &received);

    gateway_phy.begin();
    const uint8_t rule[3] = {0x05, 0x30, 0x01};
    gateway_phy.send(makeConfig(0x0001, 0x16, 0x01, rule, sizeof(rule)));

    node.poll(1000);
    TEST_ASSERT_EQUAL_UINT8(1, received);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_id_roundtrip);
    RUN_TEST(test_id_field_layout);
    RUN_TEST(test_id_fields_do_not_overflow);
    RUN_TEST(test_priority_dominates_arbitration);
    RUN_TEST(test_address_validation);
    RUN_TEST(test_float_big_endian_roundtrip);
    RUN_TEST(test_command_frame);
    RUN_TEST(test_state_broadcast_frame);
    RUN_TEST(test_telemetry_uses_low_priority);
    RUN_TEST(test_ota_data_frame);
    RUN_TEST(test_ota_data_truncates_oversized_chunk);
    RUN_TEST(test_config_frame);
    RUN_TEST(test_filter_by_target);
    RUN_TEST(test_filter_by_msg_type);
    RUN_TEST(test_feedback_loop_between_nodes);
    RUN_TEST(test_local_action_syncs_the_bus);
    RUN_TEST(test_node_ignores_frames_for_other_targets);
    RUN_TEST(test_group_scene_reaches_every_node);
    RUN_TEST(test_heartbeat_is_periodic);
    RUN_TEST(test_config_listener_receives_sdo);
    return UNITY_END();
}
