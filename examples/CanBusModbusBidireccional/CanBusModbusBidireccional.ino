/*
 * CAN <-> Modbus TCP bidireccional (esclavo) para Loxone.
 *
 * El Miniserver Loxone integra hardware de terceros como MAESTRO Modbus TCP:
 * abre una conexion al puerto 502 del gateway y lee/escribe coils y holding
 * registers. Este sketch es el lado ESCLAVO:
 *
 *   Loxone Config (maestro) --TCP:502--> ModbusTcpServer --> IModbusRegisterMap
 *                                                            --> ModbusBridge --> CAN
 *
 * Dependencia de red (elegida por el proyecto): ArduinoModbus, ModbusTCP,
 * WiFiServer/EthernetServer manual, o AsyncTCP. `IModbusTcpStream` es un
 * adaptador de pocas lineas sobre el stack TCP elegido.
 */

#include <PCD_CAN.h>

/* ------------------------------------------------------------------ */
/* 1. Mapa de registros: traduccion coils/registers <-> valores CAN   */
/* ------------------------------------------------------------------ */

/*
 * Esquema de direccionamiento recomendado (bloques por nodo):
 *
 *   Coil = slot_nodo * 16 + (canal - 1)              -> reles y entradas digitales
 *   Reg  = 0x0100 + slot_nodo * 16 + (canal - 1)     -> dimmers/consignas (holding)
 *
 * `slot_nodo` lo asigna la aplicacion al descubrir el Node-ID (NodeRegistry).
 * El ejemplo usa un solo nodo 0x0016 con slot 0 para mantenerlo corto.
 */

static const uint16_t kHoldingBase = 0x0100;

struct RegisterMap {
    static const uint16_t kCoils = 16;
    static const uint16_t kRegs = 16;
    bool coils[kCoils];
    uint16_t regs[kRegs];
    pcd::ModbusBridge *bridge;

    static bool readCoil(uint16_t a, bool &v, void *ctx) {
        RegisterMap *self = static_cast<RegisterMap *>(ctx);
        if (a >= kCoils) return false;
        v = self->coils[a];
        return true;
    }
    static bool readRegister(uint16_t a, uint16_t &v, void *ctx) {
        RegisterMap *self = static_cast<RegisterMap *>(ctx);
        if (a < kHoldingBase || a - kHoldingBase >= kRegs) return false;
        v = self->regs[a - kHoldingBase];
        return true;
    }
    static bool writeCoil(uint16_t a, bool v, void *ctx) {
        RegisterMap *self = static_cast<RegisterMap *>(ctx);
        if (a >= kCoils) return false;
        self->coils[a] = v;
        /* Escritura del maestro -> comando CAN (rele/digital en canal a+1). */
        if (self->bridge != 0) {
            self->bridge->queueIncoming(0x0016, pcd::RES_RELAY, a + 1,
                                        v ? pcd::ACT_ON : pcd::ACT_OFF);
        }
        return true;
    }
    static bool writeRegister(uint16_t a, uint16_t v, void *ctx) {
        RegisterMap *self = static_cast<RegisterMap *>(ctx);
        if (a < kHoldingBase || a - kHoldingBase >= kRegs) return false;
        self->regs[a - kHoldingBase] = v;
        /* Consigna/dimmer -> comando CAN con valor. */
        if (self->bridge != 0) {
            self->bridge->queueIncoming(0x0016, pcd::RES_DIMMER, (a - kHoldingBase) + 1,
                                        pcd::ACT_SET_VALUE, v);
        }
        return true;
    }
};

/* ------------------------------------------------------------------ */
/* 2. Glue                                                             */
/* ------------------------------------------------------------------ */

RegisterMap g_map;
pcd::IModbusRegisterMap g_contract = {
    RegisterMap::readRegister, RegisterMap::readCoil,
    RegisterMap::writeRegister, RegisterMap::writeCoil, &g_map
};
pcd::ModbusBridge g_bridge(g_contract, kHoldingBase);

/*
 * El sketch real define un adaptador `IModbusTcpStream` sobre su stack TCP:
 *
 *   struct TcpStream : public pcd::IModbusTcpStream {
 *       // begin() -> server.begin(502)
 *       // clientConnected() -> client.connected()
 *       // available() -> client.available()
 *       // read/write -> client.read()/client.write()
 *       // stopClient() -> client.stop()
 *   };
 *   TcpStream g_tcp;
 *   pcd::ModbusTcpServer g_server(g_tcp, g_contract);
 */

void setup() {
    g_map.bridge = &g_bridge;
    // g_node.begin(...); g_gateway.attachBridge(g_bridge);
    // g_tcp.begin();
}

void loop() {
    // g_gateway.poll(millis());   // CAN <-> ModbusBridge
    // g_server.poll();            // Modbus TCP <-> IModbusRegisterMap
}
