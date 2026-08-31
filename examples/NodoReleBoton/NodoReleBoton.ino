/*
 * Nodo de campo minimo: un rele en el canal 1 y un pulsador local.
 *
 * El pulsador difunde su evento al bus y conmuta el rele local; tras conmutar,
 * el nodo difunde el estado resultante para que pantallas y gateways se
 * sincronicen sin preguntar (lazo comando -> ejecucion -> difusion).
 */

#include <PCD_CAN.h>

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus bus(5 /* TX */, 4 /* RX */);
#else
pcd::Mcp2515Bus bus(10 /* CS */, pcd::MCP_CLOCK_16MHZ, 2 /* INT */);
#endif

pcd::CanNode node(bus, 0x0016);

const uint8_t kRelayPin = 7;
const uint8_t kButtonPin = 8;

bool relay_state = false;
bool last_button = true;

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
    pinMode(kButtonPin, INPUT_PULLUP);

    node.begin(500000);
    node.registerResource(pcd::RES_RELAY, 0x01, relayHandler);
}

void loop() {
    node.poll(millis());

    const bool button = digitalRead(kButtonPin);
    if (last_button && !button) {
        node.publishInputEvent(0x01, pcd::EVT_SHORT_CLICK);
        node.applyLocal(pcd::RES_RELAY, 0x01, pcd::ACT_TOGGLE);
        delay(30);
    }
    last_button = button;
}
