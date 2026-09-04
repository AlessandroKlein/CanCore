/*
 * Identidad manual o automatica.
 *
 * Manual: conserva el Node-ID configurado por el instalador.
 * Automatico: deriva un ID estable desde una identidad unica de la placa.
 * La eleccion se persiste junto con las reglas y puede cambiarse por CAN.
 */
#include <PCD_CAN.h>

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus bus(5, 4);
pcd::NvsStorage storage;
const uint32_t kHardwareUnique = static_cast<uint32_t>(ESP.getEfuseMac());
#else
pcd::Mcp2515Bus bus(10, pcd::MCP_CLOCK_16MHZ, 2);
pcd::EepromStorage storage;
const uint32_t kHardwareUnique = 0x32800016UL;
#endif

const bool kUseAutomaticId = true;
pcd::ConfigStore config(storage);
uint16_t selectedId() {
    return kUseAutomaticId ? pcd::deriveAutomaticNodeId(kHardwareUnique) : 0x0016;
}
pcd::CanNode node(bus, selectedId());
pcd::RuleEngine rules(node, config);

void setup() {
    const uint16_t fallback = selectedId();
    config.begin(fallback);
    if (kUseAutomaticId) {
        node.setAutomaticNodeId(kHardwareUnique);
        config.setNodeIdMode(pcd::NODE_ID_AUTOMATIC);
    } else {
        node.setNodeId(config.nodeId());
        config.setNodeIdMode(pcd::NODE_ID_MANUAL);
    }
    node.setNodeIdMode(config.nodeIdMode());
    config.setNodeId(node.nodeId());
    config.save();
    node.begin(500000);
    rules.begin();
}

void loop() {
    node.poll(millis());
}
