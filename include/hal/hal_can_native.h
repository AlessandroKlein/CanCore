#pragma once

/*
 * Bus CAN virtual en memoria para pruebas de escritorio (entorno "native").
 * Todas las instancias conectadas a la misma VirtualBus se escuchan entre si,
 * lo que permite validar el lazo comando -> ejecucion -> difusion de estado sin
 * hardware.
 */

#include <stddef.h>

#include "can_protocol.h"

namespace pcd {

class ICanBus;
struct CanFilter;

static const size_t kNativeQueueSize = 32;

class NativeCanBus;

class VirtualBus {
  public:
    VirtualBus();

    void attach(NativeCanBus *node);
    void detach(NativeCanBus *node);

    /* Entrega la trama a todos los nodos conectados excepto el emisor. */
    void broadcast(const CanFrame &frame, NativeCanBus *sender);

    size_t traffic() const { return traffic_; }
    void resetTraffic() { traffic_ = 0; }

  private:
    static const size_t kMaxNodes = 8;
    NativeCanBus *nodes_[kMaxNodes];
    size_t node_count_;
    size_t traffic_;
};

}  // namespace pcd

#include "hal_can.h"

namespace pcd {

class NativeCanBus : public ICanBus {
  public:
    explicit NativeCanBus(VirtualBus *bus = NULL);
    ~NativeCanBus() override;

    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) override;
    void end() override;
    CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0) override;
    CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0) override;
    void setFilter(const CanFilter &filter) override;
    bool isBusOff() const override;
    CanStatus recover() override;

    /* Inyecta una trama como si viniese del bus (util en pruebas unitarias). */
    void deliver(const CanFrame &frame);

    /* Ultima trama transmitida y contador de transmisiones. */
    const CanFrame &lastSent() const { return last_sent_; }
    size_t sentCount() const { return sent_count_; }
    size_t pending() const { return rx_count_; }
    void simulateBusOff() { bus_off_ = true; }

  private:
    VirtualBus *bus_;
    CanFilter filter_;
    bool started_;
    bool bus_off_;
    CanFrame rx_queue_[kNativeQueueSize];
    size_t rx_head_;
    size_t rx_count_;
    CanFrame last_sent_;
    size_t sent_count_;
};

}  // namespace pcd
