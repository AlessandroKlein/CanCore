/*
 * CAN <-> Modbus RTU/TCP bidireccional.
 * Dependencia: ModbusMaster, ArduinoModbus o SimpleModbus elegida por el
 * proyecto. ModbusBridge ya traduce coils/registers sin conocer esa libreria.
 */
#include <PCD_CAN.h>

struct RegisterMap {
    static bool readRegister(uint16_t, uint16_t &, void *) { return true; }
    static bool readCoil(uint16_t, bool &, void *) { return true; }
    static bool writeRegister(uint16_t, uint16_t, void *) { return true; }
    static bool writeCoil(uint16_t, bool, void *) { return true; }
};

pcd::IModbusRegisterMap map = {
    RegisterMap::readRegister, RegisterMap::readCoil,
    RegisterMap::writeRegister, RegisterMap::writeCoil, 0
};
pcd::ModbusBridge bridge(map, 0x0100);
void setup() {
    // modbus.begin(...); gateway.attachBridge(bridge);
    // mapear coils a reles y holding registers a dimmers/consignas.
}
void loop() {
    // modbus.poll(); bridge.queueIncoming(...); gateway.poll(millis());
}
