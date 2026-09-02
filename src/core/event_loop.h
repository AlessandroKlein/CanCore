#pragma once

/*
 * Bucle de eventos asincrono para CAN.
 *
 * Separa estrictamente la recepcion de hardware del procesamiento de la
 * logica de negocio:
 *
 *   ISR:
 *     [can_ctrl] --leer frame--> [event_loop->enqueue(frame)]   (~us)
 *
 *   Main loop:
 *     event_loop.tick() -> drena la cola y llama a process(frame, ctx)
 *
 * De esta forma una rafaga de tramas consecutivas a alta velocidad no bloquea
 * el bus: la ISR solo copia bytes y el procesamiento (descomprimir ID de 29
 * bits, validar recurso, actualizar EEPROM, ejecutar accion) ocurre fuera de
 * la interrupcion, en el hilo principal.
 */

#include "core/ring_buffer.h"
#include "can_protocol.h"

namespace pcd {

static const size_t kCanEventDepth = 16;

typedef void (*CanEventProcessor)(const CanFrame &frame, void *ctx);

class CanEventLoop {
  public:
    CanEventLoop() : process_(0), ctx_(0) {}

    void attach(CanEventProcessor process, void *ctx) {
        process_ = process;
        ctx_ = ctx;
    }

    /* ISR-safe: deposita la trama en el ring buffer SPSC. */
    bool enqueue(const CanFrame &frame) {
        return rx_.push(frame);
    }

    /* Main loop: drena todas las tramas pendientes y las despacha. */
    void tick() {
        CanFrame frame;
        while (rx_.pop(frame)) {
            if (process_ != 0) {
                process_(frame, ctx_);
            }
        }
    }

    /* Devuelve una unica trama pendiente (poll de una por ciclo). */
    bool next(CanFrame &frame) {
        return rx_.pop(frame);
    }

    size_t pending() const { return rx_.count(); }
    bool empty() const { return rx_.empty(); }

    void clear() {
        rx_.clear();
    }

  private:
    RingBuffer<CanFrame, kCanEventDepth> rx_;
    CanEventProcessor process_;
    void *ctx_;
};

}  // namespace pcd
