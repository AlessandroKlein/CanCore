/*
 * Anuncio de capacidades y filtros de escucha.
 *
 * Al primer poll() el nodo anuncia su identidad y cada recurso registrado.
 * Los filtros limitan los estados que llegan a las suscripciones locales.
 * Tambien pueden configurarse remotamente con CFG_SUBSCRIBE por CAN.
 */
#include <PCD_CAN.h>

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus bus(5, 4);
#else
pcd::Mcp2515Bus bus(10, pcd::MCP_CLOCK_16MHZ, 2);
#endif

pcd::CanNode node(bus, 0x0016);
float relayValue = 0.0f;

bool relay(uint8_t, uint8_t action, uint32_t, float &value, void *) {
    relayValue = action == pcd::ACT_OFF ? 0.0f : 1.0f;
    value = relayValue;
    return action <= pcd::ACT_TOGGLE;
}

void onRemoteState(const pcd::CanFrame &, float value, void *) {
    Serial.print("Estado remoto recibido: ");
    Serial.println(value);
}

void setup() {
    Serial.begin(115200);
    node.registerResource(pcd::RES_RELAY, 1, relay);
    node.addListenFilter(pcd::kAnySource, pcd::RES_ENV_SENSOR, pcd::kAnyChannel);
    node.subscribe(pcd::kAnySource, pcd::RES_ENV_SENSOR, pcd::kAnyChannel,
                   onRemoteState);
    node.begin(500000);
    node.publishDiscovery();
}

void loop() {
    node.poll(millis());
}
