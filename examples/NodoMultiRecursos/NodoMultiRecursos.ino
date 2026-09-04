/*
 * Nodo con multiples elementos: dos reles, dos dimmers y cuatro entradas.
 * Cada canal se anuncia por MSG_DISCOVERY_RESOURCE y se configura por CAN.
 */
#include <PCD_CAN.h>

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus bus(5, 4);
#else
pcd::Mcp2515Bus bus(10, pcd::MCP_CLOCK_16MHZ, 2);
#endif
pcd::CanNode node(bus, 0x0020);
float values[8] = {0};

bool multiHandler(uint8_t channel, uint8_t action, uint32_t param, float &value, void *) {
    (void)param;
    if (channel >= 8 || action > pcd::ACT_SET_VALUE) return false;
    if (action == pcd::ACT_OFF) values[channel] = 0;
    else if (action == pcd::ACT_ON) values[channel] = 1;
    else if (action == pcd::ACT_TOGGLE) values[channel] = values[channel] > 0 ? 0 : 1;
    else values[channel] = pcd::unpackSetValueTarget(param);
    value = values[channel]; return true;
}
void setup() {
    for (uint8_t channel = 1; channel <= 2; ++channel)
        node.registerResource(pcd::RES_RELAY, channel, multiHandler);
    for (uint8_t channel = 1; channel <= 2; ++channel)
        node.registerResource(pcd::RES_DIMMER, channel, multiHandler);
    for (uint8_t channel = 1; channel <= 4; ++channel)
        node.registerResource(pcd::RES_DIGITAL_INPUT, channel, multiHandler);
    node.begin(500000);
}
void loop() { node.poll(millis()); }
