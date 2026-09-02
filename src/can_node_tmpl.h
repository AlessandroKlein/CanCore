#pragma once

/*
 * Nodo CAN con asignacion estatica en tiempo de compilacion.
 *
 * El requisito de "Sistema de Plantillas C++ para la Asignacion Estatica" se
 * materializa aqui: la misma clase instancia un búfer de tramas de tamano
 * configurable por template, sin usar malloc/new.
 *
 *   // Nodo compacto en ATmega328P: 4 tramas de RX
 *   CanNodeTmpl<4> node(bus, 0x0016);
 *
 *   // Gateway ESP32: 64 tramas de RX para rafagas de alta velocidad
 *   CanNodeTmpl<64> gateway(bus, 0x0001);
 *
 * La plantilla compone el CanNode existente (logica de protocolo, recursos y
 * suscripciones) con un RingBuffer SPSC, de modo que la ISR solo deposita la
 * trama (enqueueFrame) y el bucle principal la procesa en poll().
 */

#include "can_node.h"
#include "core/ring_buffer.h"

namespace pcd {

template <size_t RxDepth>
class CanNodeTmpl {
  public:
    CanNodeTmpl(ICanBus &bus, uint16_t node_id) : node_(bus, node_id) {}

    /* --- Delegacion completa de la interfaz CanNode --- */
    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) { return node_.begin(bitrate); }
    uint16_t nodeId() const { return node_.nodeId(); }
    uint8_t address() const { return node_.address(); }

    bool registerResource(uint8_t resource, uint8_t channel, ResourceHandler handler,
                          void *ctx = 0) {
        return node_.registerResource(resource, channel, handler, ctx);
    }

    DeviceManager &devices() { return node_.devices(); }
    const DeviceManager &devices() const { return node_.devices(); }

    bool subscribe(uint16_t source, uint8_t resource, uint8_t channel,
                   StateListener listener, void *ctx = 0) {
        return node_.subscribe(source, resource, channel, listener, ctx);
    }

    void onConfig(ConfigListener listener, void *ctx = 0) { node_.onConfig(listener, ctx); }
    void onAnyFrame(FrameListener listener, void *ctx = 0) { node_.onAnyFrame(listener, ctx); }

    /* --- Lado ISR: depositar la trama leida del controlador --- */
    bool enqueueFrame(const CanFrame &frame) { return rx_.push(frame); }

    /* --- Lado main-loop: drenar el ring buffer y procesarla --- */
    void poll(uint32_t now_ms) {
        CanFrame frame;
        while (rx_.pop(frame)) {
            node_.handleFrame(frame);
        }
        node_.poll(now_ms);
    }

    CanNode &inner() { return node_; }
    const CanNode &inner() const { return node_; }

    size_t pendingFrames() const { return rx_.count(); }
    size_t rxCapacity() const { return RxDepth; }

  private:
    CanNode node_;
    RingBuffer<CanFrame, RxDepth> rx_;
};

}  // namespace pcd
