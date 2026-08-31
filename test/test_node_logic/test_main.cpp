/*
 * Pruebas del gestor de recursos, la persistencia y el motor de reglas sobre el
 * bus virtual: cubren temporizadores, rampas, CRC, recuperacion ante corrupcion
 * y la programacion de reglas por SDO segmentado.
 */

#include <unity.h>

#include "PCD_CAN.h"

using namespace pcd;

namespace {

struct FakeRelay {
    bool state;
    uint8_t calls;
};

bool relayHandler(uint8_t channel, uint8_t action, uint32_t param, float &out_value, void *ctx) {
    (void)channel;
    (void)param;
    FakeRelay *relay = static_cast<FakeRelay *>(ctx);
    ++relay->calls;
    switch (action) {
        case ACT_OFF: relay->state = false; break;
        case ACT_ON: relay->state = true; break;
        case ACT_TOGGLE: relay->state = !relay->state; break;
        default: return false;
    }
    out_value = relay->state ? 1.0f : 0.0f;
    return true;
}

bool dimmerHandler(uint8_t channel, uint8_t action, uint32_t param, float &out_value, void *ctx) {
    (void)channel;
    float *level = static_cast<float *>(ctx);
    switch (action) {
        case ACT_OFF: *level = 0.0f; break;
        case ACT_ON: *level = 100.0f; break;
        case ACT_SET_VALUE: *level = static_cast<float>(unpackSetValueTarget(param)); break;
        default: return false;
    }
    out_value = *level;
    return true;
}

BindingRule makeShortClickRule() {
    BindingRule rule;
    rule.source_node = 0x0005;
    rule.source_resource = RES_DIGITAL_INPUT;
    rule.source_channel = 0x01;
    rule.trigger = TRIG_EVENT;
    rule.event = EVT_SHORT_CLICK;
    rule.threshold = 0.0f;
    rule.target_resource = RES_RELAY;
    rule.target_channel = 0x02;
    rule.action = ACT_TOGGLE;
    rule.param = 0;
    return rule;
}

}  // namespace

void test_device_manager_registers_and_applies() {
    DeviceManager devices;
    FakeRelay relay = {false, 0};
    TEST_ASSERT_TRUE(devices.registerChannel(RES_RELAY, 0x01, relayHandler, &relay));

    float value = 0.0f;
    TEST_ASSERT_TRUE(devices.apply(RES_RELAY, 0x01, ACT_ON, 0, value));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, value);
    TEST_ASSERT_TRUE(relay.state);

    TEST_ASSERT_FALSE(devices.apply(RES_DIMMER, 0x01, ACT_ON, 0, value));
}

void test_device_manager_auto_off_timer() {
    DeviceManager devices;
    FakeRelay relay = {false, 0};
    devices.registerChannel(RES_RELAY, 0x01, relayHandler, &relay);

    float value = 0.0f;
    TEST_ASSERT_TRUE(devices.apply(RES_RELAY, 0x01, ACT_ON, 1000, value));

    devices.update(10000);
    TEST_ASSERT_TRUE(relay.state);
    devices.update(10500);
    TEST_ASSERT_TRUE(relay.state);
    devices.update(11000);
    TEST_ASSERT_FALSE(relay.state);
}

void test_device_manager_ramp_is_non_blocking() {
    DeviceManager devices;
    float level = 0.0f;
    devices.registerChannel(RES_DIMMER, 0x01, dimmerHandler, &level);

    float value = 0.0f;
    TEST_ASSERT_TRUE(devices.apply(RES_DIMMER, 0x01, ACT_SET_VALUE, packSetValue(100, 1000), value));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, level);

    devices.update(0);
    devices.update(500);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, level);
    devices.update(1000);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, level);
}

void test_config_store_roundtrip_with_crc() {
    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);
    TEST_ASSERT_TRUE(config.addRule(makeShortClickRule()));
    TEST_ASSERT_TRUE(config.save());

    ConfigStore reloaded(storage);
    TEST_ASSERT_TRUE(reloaded.begin(0x0001));
    TEST_ASSERT_EQUAL_UINT16(0x0016, reloaded.nodeId());
    TEST_ASSERT_EQUAL_UINT8(1, reloaded.ruleCount());
    TEST_ASSERT_EQUAL_UINT16(0x0005, reloaded.rule(0).source_node);
    TEST_ASSERT_EQUAL_UINT8(EVT_SHORT_CLICK, reloaded.rule(0).event);
    TEST_ASSERT_EQUAL_UINT8(RES_RELAY, reloaded.rule(0).target_resource);
}

void test_config_store_rejects_corrupted_block() {
    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);
    config.addRule(makeShortClickRule());
    config.save();

    storage.corrupt(kConfigHeaderBytes + 3);

    ConfigStore reloaded(storage);
    TEST_ASSERT_FALSE(reloaded.begin(0x0007));
    TEST_ASSERT_EQUAL_UINT16(0x0007, reloaded.nodeId());
    TEST_ASSERT_EQUAL_UINT8(0, reloaded.ruleCount());
}

void test_config_store_empty_medium_uses_defaults() {
    MemoryStorage storage;
    ConfigStore config(storage);
    TEST_ASSERT_FALSE(config.begin(0x0020));
    TEST_ASSERT_EQUAL_UINT16(0x0020, config.nodeId());
    TEST_ASSERT_EQUAL_UINT8(0, config.ruleCount());
}

void test_rule_engine_executes_remote_event_locally() {
    VirtualBus wire;
    NativeCanBus bus_a(&wire);
    NativeCanBus bus_b(&wire);

    CanNode sensor(bus_a, 0x0005);
    CanNode actuator(bus_b, 0x0016);
    sensor.begin();
    actuator.begin();

    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);
    config.addRule(makeShortClickRule());

    RuleEngine rules(actuator, config);
    rules.begin();

    FakeRelay relay = {false, 0};
    actuator.registerResource(RES_RELAY, 0x02, relayHandler, &relay);

    sensor.publishInputEvent(0x01, EVT_SHORT_CLICK);
    actuator.poll(1000);

    TEST_ASSERT_TRUE(relay.state);
    TEST_ASSERT_EQUAL_UINT8(1, relay.calls);
}

void test_rule_engine_ignores_other_sources_and_events() {
    VirtualBus wire;
    NativeCanBus bus_a(&wire);
    NativeCanBus bus_b(&wire);

    CanNode other(bus_a, 0x0009);
    CanNode actuator(bus_b, 0x0016);
    other.begin();
    actuator.begin();

    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);
    config.addRule(makeShortClickRule());

    RuleEngine rules(actuator, config);
    rules.begin();

    FakeRelay relay = {false, 0};
    actuator.registerResource(RES_RELAY, 0x02, relayHandler, &relay);

    /* Nodo equivocado. */
    other.publishInputEvent(0x01, EVT_SHORT_CLICK);
    actuator.poll(1000);
    TEST_ASSERT_FALSE(relay.state);
}

void test_rule_engine_threshold_trigger() {
    VirtualBus wire;
    NativeCanBus bus_a(&wire);
    NativeCanBus bus_b(&wire);

    CanNode sensor(bus_a, 0x0005);
    CanNode actuator(bus_b, 0x0016);
    sensor.begin();
    actuator.begin();

    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);

    BindingRule rule = makeShortClickRule();
    rule.source_resource = RES_ENV_SENSOR;
    rule.trigger = TRIG_VALUE_GREATER;
    rule.threshold = 28.0f;
    rule.action = ACT_ON;
    config.addRule(rule);

    RuleEngine rules(actuator, config);
    rules.begin();

    FakeRelay relay = {false, 0};
    actuator.registerResource(RES_RELAY, 0x02, relayHandler, &relay);

    sensor.publishTelemetry(RES_ENV_SENSOR, 0x01, 24.0f);
    actuator.poll(1000);
    TEST_ASSERT_FALSE(relay.state);

    sensor.publishTelemetry(RES_ENV_SENSOR, 0x01, 31.5f);
    actuator.poll(2000);
    TEST_ASSERT_TRUE(relay.state);
}

void test_rule_engine_sdo_programs_and_persists_rule() {
    VirtualBus wire;
    NativeCanBus bus_gw(&wire);
    NativeCanBus bus_node(&wire);

    CanNode gateway(bus_gw, 0x0001);
    CanNode actuator(bus_node, 0x0016);
    gateway.begin();
    actuator.begin();

    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);

    RuleEngine rules(actuator, config);
    rules.begin();

    FakeRelay relay = {false, 0};
    actuator.registerResource(RES_RELAY, 0x02, relayHandler, &relay);

    uint8_t raw[kBindingRuleBytes];
    ConfigStore::serializeRule(makeShortClickRule(), raw);

    const uint8_t target = 0x16;
    uint8_t payload[6];
    payload[0] = 0xFF; /* anexar al final de la tabla */
    gateway.sendConfig(target, CFG_RULE_BEGIN, payload, 1);

    for (uint8_t segment = 0; segment < 4; ++segment) {
        payload[0] = segment;
        for (uint8_t i = 0; i < 5; ++i) {
            payload[1 + i] = raw[segment * 5 + i];
        }
        gateway.sendConfig(target, CFG_RULE_CHUNK, payload, 6);
    }

    writeUint16BE(payload, crc16(raw, kBindingRuleBytes));
    gateway.sendConfig(target, CFG_RULE_COMMIT, payload, 2);

    actuator.poll(1000);
    TEST_ASSERT_EQUAL_UINT8(1, config.ruleCount());

    /* La regla quedo persistida: otra instancia la recupera del mismo medio. */
    ConfigStore reloaded(storage);
    TEST_ASSERT_TRUE(reloaded.begin(0x0000));
    TEST_ASSERT_EQUAL_UINT8(1, reloaded.ruleCount());
    TEST_ASSERT_EQUAL_UINT8(EVT_SHORT_CLICK, reloaded.rule(0).event);
}

void test_rule_engine_sdo_rejects_bad_crc() {
    VirtualBus wire;
    NativeCanBus bus_gw(&wire);
    NativeCanBus bus_node(&wire);

    CanNode gateway(bus_gw, 0x0001);
    CanNode actuator(bus_node, 0x0016);
    gateway.begin();
    actuator.begin();

    MemoryStorage storage;
    ConfigStore config(storage);
    config.begin(0x0016);
    RuleEngine rules(actuator, config);
    rules.begin();

    uint8_t raw[kBindingRuleBytes];
    ConfigStore::serializeRule(makeShortClickRule(), raw);

    const uint8_t target = 0x16;
    uint8_t payload[6];
    payload[0] = 0xFF;
    gateway.sendConfig(target, CFG_RULE_BEGIN, payload, 1);
    for (uint8_t segment = 0; segment < 4; ++segment) {
        payload[0] = segment;
        for (uint8_t i = 0; i < 5; ++i) {
            payload[1 + i] = raw[segment * 5 + i];
        }
        gateway.sendConfig(target, CFG_RULE_CHUNK, payload, 6);
    }
    writeUint16BE(payload, static_cast<uint16_t>(crc16(raw, kBindingRuleBytes) ^ 0x00FF));
    gateway.sendConfig(target, CFG_RULE_COMMIT, payload, 2);

    actuator.poll(1000);
    TEST_ASSERT_EQUAL_UINT8(0, config.ruleCount());
}

void test_node_publishes_state_when_timer_expires() {
    VirtualBus wire;
    NativeCanBus bus_node(&wire);
    NativeCanBus bus_monitor(&wire);

    CanNode node(bus_node, 0x0016);
    CanNode monitor(bus_monitor, 0x0001);
    node.begin();
    monitor.begin();

    FakeRelay relay = {false, 0};
    node.registerResource(RES_RELAY, 0x01, relayHandler, &relay);

    node.applyLocal(RES_RELAY, 0x01, ACT_ON, 500);
    node.poll(1000);
    TEST_ASSERT_TRUE(relay.state);

    node.poll(1600);
    TEST_ASSERT_FALSE(relay.state);

    /* El apagado autonomo se difunde al bus. */
    CanFrame frame;
    bool state_seen = false;
    while (bus_monitor.receive(frame) == CAN_OK) {
        const CanId id = frame.fields();
        if (id.msg_type == MSG_STATE && id.source == 0x0016 && frame.resource() == RES_RELAY) {
            state_seen = (frameValue(frame) == 0.0f);
        }
    }
    TEST_ASSERT_TRUE(state_seen);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_device_manager_registers_and_applies);
    RUN_TEST(test_device_manager_auto_off_timer);
    RUN_TEST(test_device_manager_ramp_is_non_blocking);
    RUN_TEST(test_config_store_roundtrip_with_crc);
    RUN_TEST(test_config_store_rejects_corrupted_block);
    RUN_TEST(test_config_store_empty_medium_uses_defaults);
    RUN_TEST(test_rule_engine_executes_remote_event_locally);
    RUN_TEST(test_rule_engine_ignores_other_sources_and_events);
    RUN_TEST(test_rule_engine_threshold_trigger);
    RUN_TEST(test_rule_engine_sdo_programs_and_persists_rule);
    RUN_TEST(test_rule_engine_sdo_rejects_bad_crc);
    RUN_TEST(test_node_publishes_state_when_timer_expires);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
