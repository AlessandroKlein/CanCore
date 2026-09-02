/*
 * Firmware de referencia del nodo de campo.
 *
 * Un rele en el canal 1, una entrada digital en el canal 1 y una regla de
 * vinculacion persistente: la pulsacion corta de la entrada conmuta el rele
 * aunque no haya gateway en el bus. El nodo responde a comandos remotos y
 * difunde el estado resultante (feedback loop).
 *
 * Este archivo se compila solo cuando PlatformIO construye un firmware
 * (-DPCD_BUILD_FIRMWARE); al instalar el repositorio como libreria de Arduino
 * queda fuera del build para no colisionar con el sketch del usuario.
 *
 * Los perfiles de gateway (MQTT, tunel UDP, Modbus, OTA, servidor web) se
 * activan con las banderas FEATURE_* de system_config.h.
 */

#if defined(ARDUINO) && defined(PCD_BUILD_FIRMWARE)

#include <Arduino.h>
#include <stdarg.h>

#include "PCD_CAN.h"

namespace {

#if defined(ARDUINO_ARCH_ESP32)
const int kCanTxPin = 5;
const int kCanRxPin = 4;
pcd::Esp32TwaiBus g_bus(kCanTxPin, kCanRxPin);
pcd::NvsStorage g_storage;
const uint16_t kNodeId = 0x0001; /* gateway */
#else
const uint8_t kMcpCsPin = 10;
const int8_t kMcpIntPin = 2;
pcd::Mcp2515Bus g_bus(kMcpCsPin, pcd::MCP_CLOCK_16MHZ, kMcpIntPin);
pcd::EepromStorage g_storage;
const uint16_t kNodeId = 0x0016; /* nodo de campo */
#endif

pcd::ConfigStore g_config(g_storage);
pcd::CanNode g_node(g_bus, kNodeId);
pcd::RuleEngine g_rules(g_node, g_config);

const uint8_t kRelayPin = 7;
const uint8_t kButtonPin = 8;

bool g_relay_state = false;
bool g_last_button = true;

pcd::SystemApi g_system_api;

#if CAN_LOG_LEVEL > 0
void logSink(uint8_t level, const char *format, va_list args) {
    char buf[128];
    vsnprintf(buf, sizeof(buf), format, args);
    Serial.printf("[%u] %s\n", static_cast<unsigned>(level), buf);
}
#endif

void setupSystemApi() {
    g_system_api.millis = millis;
    g_system_api.delay_ms = [](uint32_t ms) { delay(ms); };
#if CAN_LOG_LEVEL > 0
    g_system_api.log = logSink;
#endif
    pcd::setSystemApi(&g_system_api);
}

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
    setupSystemApi();

#if CAN_LOG_LEVEL > 0
    Serial.begin(115200);
#endif
    pinMode(kRelayPin, OUTPUT);
    pinMode(kButtonPin, INPUT_PULLUP);

    if (!g_config.begin(kNodeId)) {
        LOG_INFO("Configuracion ausente o corrupta: se aplican valores por defecto");
    }

    if (g_node.begin(CAN_BUS_BITRATE) != pcd::CAN_OK) {
        LOG_ERROR("No se pudo inicializar el bus CAN");
    }
    g_node.registerResource(pcd::RES_RELAY, 0x01, relayHandler);
    g_rules.begin();
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

#endif  /* ARDUINO && PCD_BUILD_FIRMWARE */
