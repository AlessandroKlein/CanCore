/*
 * Punto de entrada universal.
 *
 * Ejemplo minimo de nodo de campo multi-funcion: un rele en el canal 1 y una
 * entrada digital en el canal 1 que conmuta ese rele localmente. El nodo
 * responde a comandos remotos y difunde el estado resultante (feedback loop).
 *
 * Los perfiles de gateway (MQTT, tunel UDP, Modbus, OTA, servidor web) se
 * activan con las banderas FEATURE_* de system_config.h.
 */

#if defined(ARDUINO)

#include <Arduino.h>

#include "can_node.h"
#include "system_config.h"

namespace {

#if defined(ARDUINO_ARCH_ESP32)
const int kCanTxPin = 5;
const int kCanRxPin = 4;
pcd::Esp32TwaiBus g_bus(kCanTxPin, kCanRxPin);
const uint16_t kNodeId = 0x0001; /* gateway */
#else
const uint8_t kMcpCsPin = 10;
const int8_t kMcpIntPin = 2;
pcd::Mcp2515Bus g_bus(kMcpCsPin, pcd::MCP_CLOCK_16MHZ, kMcpIntPin);
const uint16_t kNodeId = 0x0016; /* nodo de campo */
#endif

pcd::CanNode g_node(g_bus, kNodeId);

const uint8_t kRelayPin = 7;
const uint8_t kButtonPin = 8;

bool g_relay_state = false;
bool g_last_button = true;

bool relayHandler(uint8_t channel, uint8_t action, uint32_t param, float &out_value, void *ctx) {
    (void)channel;
    (void)param;
    (void)ctx;
    switch (action) {
        case pcd::ACT_OFF: g_relay_state = false; break;
        case pcd::ACT_ON: g_relay_state = true; break;
        case pcd::ACT_TOGGLE: g_relay_state = !g_relay_state; break;
        default: return false;
    }
    digitalWrite(kRelayPin, g_relay_state ? HIGH : LOW);
    out_value = g_relay_state ? 1.0f : 0.0f;
    return true;
}

}  // namespace

void setup() {
#if CAN_LOG_LEVEL > 0
    Serial.begin(115200);
#endif
    pinMode(kRelayPin, OUTPUT);
    pinMode(kButtonPin, INPUT_PULLUP);

    if (g_node.begin(CAN_BUS_BITRATE) != pcd::CAN_OK) {
        LOG_ERROR("No se pudo inicializar el bus CAN");
    }
    g_node.registerResource(pcd::RES_RELAY, 0x01, relayHandler);
}

void loop() {
    g_node.poll(millis());

    const bool button = digitalRead(kButtonPin);
    if (g_last_button && !button) {
        g_node.publishInputEvent(0x01, pcd::EVT_SHORT_CLICK);
        g_node.applyLocal(pcd::RES_RELAY, 0x01, pcd::ACT_TOGGLE);
        delay(30); /* anti-rebote */
    }
    g_last_button = button;
}

#endif  /* ARDUINO */
