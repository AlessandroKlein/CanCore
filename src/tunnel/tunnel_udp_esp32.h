#pragma once

/*
 * Transporte de tunel por UDP para ESP32 (framework Arduino).
 *
 * Implementa ITunnelTransport sobre WiFiUdp para conectar dos o mas buses CAN
 * remotos de forma punto a punto sin servidor central:
 *
 *   - El usuario configura un puerto de escucha (p.ej. 8888) con begin().
 *   - Registra los peers remotos con addPeer(id, host, puerto).
 *   - Cada trama enviada se empaqueta en un datagrama UDP de 16 bytes.
 *
 * En PCD_CAN.h este transporte se incluye automaticamente al compilar para
 * ESP32 (ARDUINO_ARCH_ESP32).
 */

#if defined(ARDUINO_ARCH_ESP32)

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "WiFiUdp.h"

#include "tunnel/tunnel_engine.h"
#include "tunnel/tunnel_transport.h"

namespace pcd {

static const size_t kMaxUdpPeers = 4;
static const size_t kMaxUdpDatagram = 64;
static const size_t kUdpRxDepth = 8;

class UdpTunnelTransport : public ITunnelTransport {
  public:
    explicit UdpTunnelTransport(uint16_t listen_port);

    bool begin();
    void end();

    /* Gestion de peers: id -> (host, puerto). */
    bool addPeer(uint16_t peer_id, const char *host, uint16_t port);
    bool removePeer(uint16_t peer_id);
    void clearPeers();
    uint8_t peerCount() const { return peer_count_; }

    /* ITunnelTransport */
    bool send(uint16_t peer, const uint8_t *data, size_t len) override;
    bool receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                 size_t &len) override;
    bool available() const override;

  private:
    struct Peer {
        uint16_t id;
        char host[32];
        uint16_t port;
    };

    struct RxPacket {
        uint16_t peer;
        uint8_t data[kMaxUdpDatagram];
        size_t len;
    };

    bool pump();
    int16_t findPeer(uint16_t peer_id) const;

    WiFiUDP udp_;
    uint16_t listen_port_;
    bool started_;
    Peer peers_[kMaxUdpPeers];
    uint8_t peer_count_;

    RxPacket rx_queue_[kUdpRxDepth];
    size_t rx_head_;
    size_t rx_count_;
};

}  // namespace pcd

#endif  /* ARDUINO_ARCH_ESP32 */
