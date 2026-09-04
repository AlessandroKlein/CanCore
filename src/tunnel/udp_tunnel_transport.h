#pragma once

/*
 * Implementacion concreta de referencia para un transporte de datagramas
 * CAN-over-IP sin acoplar ninguna libreria de red.
 *
 * Sirve como adaptador base para stacks reales (WiFi UDP, Ethernet, TCP,
 * sockets nativos, etc.) y como cola de prueba para el entorno "native".
 */

#include <stddef.h>
#include <stdint.h>

#include "tunnel/tunnel_transport.h"

namespace pcd {

class UdpTunnelTransport : public ITunnelTransport {
  public:
    static const size_t kMaxDatagrams = 16;
    static const size_t kMaxPayload = 64;

    UdpTunnelTransport();

    /* Encola un datagrama recibido desde la red hacia la capa de tunel. */
    bool enqueue(uint16_t peer, const uint8_t *data, size_t len);

    bool send(uint16_t peer, const uint8_t *data, size_t len) override;
    bool receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                 size_t &len) override;
    bool available() const override;

  private:
    struct Datagram {
        uint16_t peer;
        uint8_t data[kMaxPayload];
        size_t len;
    };

    Datagram queue_[kMaxDatagrams];
    size_t head_;
    size_t count_;
};

}  // namespace pcd
