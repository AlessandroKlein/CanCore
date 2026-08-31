#pragma once

/*
 * Abstraccion de la capa fisica del bus CAN.
 *
 *   ESP32  -> controlador TWAI integrado
 *   AVR    -> MCP2515 sobre SPI
 *   native -> implementacion en memoria para pruebas unitarias de escritorio
 *
 * Toda la logica del ecosistema depende unicamente de ICanBus, de modo que el
 * mismo codigo de aplicacion compila en cualquiera de las plataformas.
 */

#include <stdint.h>

#include "can_protocol.h"
#include "system_config.h"

namespace pcd {

enum CanStatus : uint8_t {
    CAN_OK = 0,
    CAN_ERR_INIT,
    CAN_ERR_TX_FAIL,
    CAN_ERR_TX_BUSY,
    CAN_ERR_NO_DATA,
    CAN_ERR_BUS_OFF,
    CAN_ERR_NOT_STARTED
};

/*
 * Filtro de aceptacion aplicado sobre el identificador de 29 bits.
 * Se acepta la trama cuando (id & mask) == (match & mask).
 */
struct CanFilter {
    uint32_t match;
    uint32_t mask;

    CanFilter() : match(0), mask(0) {}
    CanFilter(uint32_t m, uint32_t k) : match(m), mask(k) {}

    bool accepts(uint32_t id) const { return (id & mask) == (match & mask); }

    /* Acepta solo tramas dirigidas al nodo indicado y los broadcast. */
    static CanFilter forTarget(uint8_t target) {
        return CanFilter(static_cast<uint32_t>(target & kTargetMask) << kTargetShift,
                         kTargetMask << kTargetShift);
    }

    /* Acepta solo un tipo de mensaje (heartbeat, OTA, estado, ...). */
    static CanFilter forMsgType(uint8_t msg_type) {
        return CanFilter(static_cast<uint32_t>(msg_type & kMsgTypeMask) << kMsgTypeShift,
                         kMsgTypeMask << kMsgTypeShift);
    }

    static CanFilter acceptAll() { return CanFilter(0, 0); }
};

class ICanBus {
  public:
    virtual ~ICanBus() {}

    /* Inicializa el controlador con la velocidad indicada en bits por segundo. */
    virtual CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) = 0;
    virtual void end() = 0;

    /* Transmite una trama extendida; timeout_ms = 0 significa no bloqueante. */
    virtual CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0) = 0;

    /* Extrae la siguiente trama recibida. Devuelve CAN_ERR_NO_DATA si no hay. */
    virtual CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0) = 0;

    /* Filtro de hardware/software aplicado a la recepcion. */
    virtual void setFilter(const CanFilter &filter) = 0;

    /* true cuando el controlador entro en estado bus-off y requiere recuperacion. */
    virtual bool isBusOff() const = 0;

    /* Intenta recuperar el controlador tras un bus-off. */
    virtual CanStatus recover() = 0;
};

}  // namespace pcd

#if defined(ARDUINO_ARCH_ESP32)
#include "hal/hal_can_esp32.h"
#elif defined(__AVR__)
#include "hal/hal_can_mcp2515.h"
#else
#include "hal/hal_can_native.h"
#endif
