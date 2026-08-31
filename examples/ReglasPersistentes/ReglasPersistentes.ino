/*
 * Reglas de vinculacion persistentes.
 *
 * El nodo guarda en su memoria no volatil la regla "si el nodo 0x0005 informa
 * una pulsacion corta en su entrada 1, conmutar mi rele del canal 2" y la
 * ejecuta por si mismo: si el gateway esta apagado, la tecla sigue funcionando.
 *
 * La misma regla puede programarse en caliente por SDO (MSG_CONFIG) desde
 * cualquier punto del bus.
 */

#include <PCD_CAN.h>

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus bus(5 /* TX */, 4 /* RX */);
pcd::NvsStorage storage;
#else
pcd::Mcp2515Bus bus(10 /* CS */, pcd::MCP_CLOCK_16MHZ, 2 /* INT */);
pcd::EepromStorage storage;
#endif

pcd::ConfigStore config(storage);
pcd::CanNode node(bus, 0x0016);
pcd::RuleEngine rules(node, config);

const uint8_t kRelayPin = 7;
bool relay_state = false;

bool relayHandler(uint8_t channel, uint8_t action, uint32_t param, float &out_value, void *ctx) {
    (void)channel;
    (void)param;
    (void)ctx;
    switch (action) {
        case pcd::ACT_OFF: relay_state = false; break;
        case pcd::ACT_ON: relay_state = true; break;
        case pcd::ACT_TOGGLE: relay_state = !relay_state; break;
        default: return false;
    }
    digitalWrite(kRelayPin, relay_state ? HIGH : LOW);
    out_value = relay_state ? 1.0f : 0.0f;
    return true;
}

void setup() {
    pinMode(kRelayPin, OUTPUT);

    /* Carga la configuracion; si el CRC falla se parte de valores por defecto. */
    config.begin(0x0016);

    if (config.ruleCount() == 0) {
        pcd::BindingRule rule;
        rule.source_node = 0x0005;
        rule.source_resource = pcd::RES_DIGITAL_INPUT;
        rule.source_channel = 0x01;
        rule.trigger = pcd::TRIG_EVENT;
        rule.event = pcd::EVT_SHORT_CLICK;
        rule.threshold = 0.0f;
        rule.target_resource = pcd::RES_RELAY;
        rule.target_channel = 0x02;
        rule.action = pcd::ACT_TOGGLE;
        rule.param = 0;

        config.addRule(rule);
        config.save();
    }

    node.begin(500000);
    node.registerResource(pcd::RES_RELAY, 0x02, relayHandler);
    rules.begin();
}

void loop() {
    node.poll(millis());
}
