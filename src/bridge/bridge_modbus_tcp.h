#pragma once

/*
 * Servidor Modbus TCP (slave) agnostico a la red.
 *
 * Loxone Config integra hardware de terceros como maestro Modbus TCP: el
 * Miniserver abre una conexion TCP al puerto 502 del gateway y lee/escribe
 * coils y holding registers. Este modulo es la mitad que faltaba: el
 * `ModbusBridge` ya traduce coils/registers <-> CAN, pero solo define el
 * modelo de datos (`IModbusRegisterMap`); aqui vive el servidor que habla el
 * ADU Modbus TCP (MBAP + PDU) y responde a las consultas del maestro.
 *
 *   Loxone Config (maestro) --TCP:502--> ModbusTcpServer --IModbusRegisterMap-->
 *                                                                  ModbusBridge <-> CAN
 *
 * Como el resto de la libreria, no se acopla a ningun stack IP: la aplicacion
 * implementa `IModbusTcpStream` con su servidor TCP preferido (WiFiServer,
 * EthernetServer, AsyncTCP, sockets nativos...) y la pasa al servidor.
 */

#include <stddef.h>
#include <stdint.h>

#include "bridge/bridge.h" /* IModbusRegisterMap */

namespace pcd {

/* ------------------------------------------------------------------ */
/* Transporte de flujo TCP inyectable                                 */
/* ------------------------------------------------------------------ */

class IModbusTcpStream {
  public:
    virtual ~IModbusTcpStream() {}

    /* Inicia el servidor TCP. Devuelve false si no pudo escuchar. */
    virtual bool begin() = 0;

    /* true si hay un cliente conectado en este instante. */
    virtual bool clientConnected() = 0;

    /* Bytes pendientes de lectura del cliente conectado. */
    virtual size_t available() = 0;

    /* Lee hasta max_len bytes del cliente. Devuelve la cantidad leida. */
    virtual size_t read(uint8_t *data, size_t max_len) = 0;

    /* Escribe len bytes al cliente. Devuelve la cantidad escrita. */
    virtual size_t write(const uint8_t *data, size_t len) = 0;

    /* Cierra la conexion con el cliente actual. */
    virtual void stopClient() = 0;
};

/* ------------------------------------------------------------------ */
/* Constantes del protocolo                                            */
/* ------------------------------------------------------------------ */

static const uint16_t kModbusTcpDefaultPort = 502;

/* Tamano maximo de un ADU Modbus TCP: 7 (MBAP) + 253 (PDU). */
static const uint16_t kModbusTcpMaxAdu = 260;

/* Buffer interno del servidor (margen sobre el ADU maximo). */
static const uint16_t kModbusTcpBuffer = kModbusTcpMaxAdu + 8;

/* Unidad de direccionamiento (slave) por defecto: 0xFF = aceptar cualquiera. */
static const uint8_t kModbusTcpAnyUnit = 0xFF;

/* Codigos de funcion Modbus soportados. */
enum ModbusFunction : uint8_t {
    MODBUS_FC_READ_COILS = 0x01,
    MODBUS_FC_READ_DISCRETE_INPUTS = 0x02,
    MODBUS_FC_READ_HOLDING_REGISTERS = 0x03,
    MODBUS_FC_READ_INPUT_REGISTERS = 0x04,
    MODBUS_FC_WRITE_SINGLE_COIL = 0x05,
    MODBUS_FC_WRITE_SINGLE_REGISTER = 0x06,
    MODBUS_FC_WRITE_MULTIPLE_COILS = 0x0F,
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS = 0x10
};

/* Codigos de excepcion Modbus. */
enum ModbusException : uint8_t {
    MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    MODBUS_EX_DEVICE_FAILURE = 0x04
};

/* ------------------------------------------------------------------ */
/* Codec del ADU Modbus TCP (funciones puras, testeables)              */
/* ------------------------------------------------------------------ */

/*
 * Parsea un request Modbus TCP completo y lo ejecuta contra `map`.
 *
 *   request      bytes recibidos (ADU completo, con cabecera MBAP)
 *   request_len  cantidad de bytes
 *   response     buffer de salida con el ADU de respuesta
 *   response_len cantidad de bytes escritos en response (salida)
 *
 * Devuelve true si se genero una respuesta (normal o de excepcion). Si el
 * request esta incompleto o malformado devuelve false y no escribe nada.
 */
bool modbusTcpHandleRequest(const uint8_t *request, size_t request_len,
                            IModbusRegisterMap &map, uint8_t *response,
                            size_t &response_len);

/*
 * true si `data` contiene al menos un ADU completo (cabecera MBAP de 7 bytes
 * cuya longitud coincide con lo disponible). Util para saber si el stream ya
 * acumulo un pedido entero.
 */
bool modbusTcpHasCompleteAdu(const uint8_t *data, size_t len);

/* ------------------------------------------------------------------ */
/* Servidor Modbus TCP                                                 */
/* ------------------------------------------------------------------ */

class ModbusTcpServer {
  public:
    ModbusTcpServer(IModbusTcpStream &stream, IModbusRegisterMap &map);

    /* Inicia el stream TCP. */
    bool begin();

    /*
     * Atiende una iteracion del servidor: acepta datos del cliente, procesa
     * pedidos completos y responde. Se llama desde el loop principal.
     */
    void poll();

    /* Restringe el slave/unit id aceptado (kModbusTcpAnyUnit = cualquiera). */
    void setUnitId(uint8_t unit_id) { unit_id_ = unit_id; }
    uint8_t unitId() const { return unit_id_; }

  private:
    void resetBuffer();

    IModbusTcpStream &stream_;
    IModbusRegisterMap &map_;
    uint8_t unit_id_;
    uint8_t buffer_[kModbusTcpBuffer];
    size_t buffered_;
    bool client_was_connected_;
};

}  // namespace pcd
