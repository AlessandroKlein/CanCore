#pragma once

/*
 * Interfaces de puente (bridges) agnosticas.
 *
 * Cada puente traduce en ambas direcciones entre el bus CAN y un protocolo
 * externo. Para NO acoplar librerias de terceros, la libreria define solo
 * *contratos* con callbacks/funciones que la aplicacion inyecta:
 *
 *   - MQTT    -> IMqttTransport (publicar, suscribirse, callback de llegada)
 *   - Modbus  -> IModbusRegisterMap (leer/escribir registros, coils)
 *   - ESP-NOW -> IEspNowTransport (enviar/recibir un paquete de bytes)
 *   - KNX     -> IKnxTransport (enviar/recibir GroupValue con DPT)
 *   - IP      -> ITunnelTransport (ya existe en tunnel/tunnel_transport.h)
 *
 * La aplicacion conecta su libreria preferida (PubSubClient, ModbusMaster,
 * esp_now.h, eremmel/knx, WiFiUdp...) escribiendo un adaptador de pocas
 * lineas, y lo pasa al bridge correspondiente.
 */

#include <stddef.h>
#include <stdint.h>

#include "routing/canonical.h"

namespace pcd {

/* ------------------------------------------------------------------ */
/* Contrato generico de un puente                                     */
/* ------------------------------------------------------------------ */

class IBridge {
  public:
    virtual ~IBridge() {}

    /* Recibe una CanonicalFrame de salida (el enrutador la entrego porque
     * el destino era este protocolo). El puente la traduce y la envia. */
    virtual bool process(const CanonicalFrame &frame) = 0;

    /* Ruta de entrada: algo llego del protocolo externo y debe convertirse
     * en una CanonicalFrame para el enrutador. */
    virtual bool buildCanonical(CanonicalFrame &out) = 0;

    /* true si hay una tarea pendiente de procesar (por ejemplo un mensaje
     * MQTT recibido). */
    virtual bool available() const = 0;
};

/* ------------------------------------------------------------------ */
/* Transporte MQTT inyectable                                        */
/* ------------------------------------------------------------------ */

struct IMqttTransport {
    /* Publicar un mensaje en un topico. Devuelve true si se encolo. */
    bool (*publish)(const char *topic, const uint8_t *data, size_t len, bool retain);

    /* Suscribirse a un topico. */
    bool (*subscribe)(const char *topic);

    /* Callback cuando llega un mensaje MQTT. */
    void (*onMessage)(const char *topic, const uint8_t *data, size_t len, void *ctx);
    void *ctx;

    IMqttTransport() : publish(0), subscribe(0), onMessage(0), ctx(0) {}
};

/* ------------------------------------------------------------------ */
/* Transporte Modbus inyectable                                      */
/* ------------------------------------------------------------------ */

struct IModbusRegisterMap {
    /* Leer un registro holding / input / coil. */
    bool (*readRegister)(uint16_t address, uint16_t &value, void *ctx);
    bool (*readCoil)(uint16_t address, bool &value, void *ctx);

    /* Escribir un registro holding / coil. */
    bool (*writeRegister)(uint16_t address, uint16_t value, void *ctx);
    bool (*writeCoil)(uint16_t address, bool value, void *ctx);
    void *ctx;

    IModbusRegisterMap()
        : readRegister(0), readCoil(0), writeRegister(0), writeCoil(0), ctx(0) {}
};

/* ------------------------------------------------------------------ */
/* Transporte ESP-NOW inyectable                                     */
/* ------------------------------------------------------------------ */

struct IEspNowTransport {
    /* Enviar un paquete de hasta 250 bytes a la MAC destino. */
    bool (*send)(const uint8_t *mac, const uint8_t *data, size_t len);

    /* Callback al recibir un paquete ESP-NOW. */
    void (*onReceive)(const uint8_t *mac, const uint8_t *data, size_t len, void *ctx);
    void *ctx;

    IEspNowTransport() : send(0), onReceive(0), ctx(0) {}
};

/* ------------------------------------------------------------------ */
/* Transporte KNX inyectable (Group Value + DPT)                     */
/* ------------------------------------------------------------------ */

struct IKnxTransport {
    /* Enviar un GroupValue con un DPT. datos se interpreta segun dpt. */
    bool (*sendGroupValue)(const uint8_t *addr, const uint8_t *data, size_t len, uint8_t dpt);

    /* Callback al recibir un GroupValue del bus KNX. */
    void (*onGroupValue)(const uint8_t *addr, const uint8_t *data, size_t len,
                         uint8_t dpt, void *ctx);
    void *ctx;

    IKnxTransport() : sendGroupValue(0), onGroupValue(0), ctx(0) {}
};

/* ------------------------------------------------------------------ */
/* Helpers de conversion CanonicalFrame <-> protocolos                */
/* ------------------------------------------------------------------ */

/* Recurso PCD -> tipo de componente Home Assistant / UI (para MQTT discovery). */
const char *resourceComponentName(uint8_t resource);

}  // namespace pcd
