#if defined(ARDUINO_ARCH_ESP32)

#include "tunnel/tunnel_udp_esp32.h"

#include <string.h>

namespace pcd {

UdpTunnelTransport::UdpTunnelTransport(uint16_t listen_port)
    : listen_port_(listen_port),
      started_(false),
      peer_count_(0),
      rx_head_(0),
      rx_count_(0) {
    for (uint8_t i = 0; i < kMaxUdpPeers; ++i) {
        peers_[i].id = 0;
        peers_[i].host[0] = '\0';
        peers_[i].port = 0;
    }
    for (size_t i = 0; i < kUdpRxDepth; ++i) {
        rx_queue_[i].peer = kBroadcastPeer;
        rx_queue_[i].len = 0;
    }
}

bool UdpTunnelTransport::begin() {
    if (started_) {
        return true;
    }
    if (!udp_.begin(listen_port_)) {
        return false;
    }
    started_ = true;
    rx_head_ = 0;
    rx_count_ = 0;
    return true;
}

void UdpTunnelTransport::end() {
    if (started_) {
        udp_.stop();
        started_ = false;
    }
    rx_head_ = 0;
    rx_count_ = 0;
}

int16_t UdpTunnelTransport::findPeer(uint16_t peer_id) const {
    for (uint8_t i = 0; i < peer_count_; ++i) {
        if (peers_[i].id == peer_id) {
            return static_cast<int16_t>(i);
        }
    }
    return -1;
}

bool UdpTunnelTransport::addPeer(uint16_t peer_id, const char *host, uint16_t port) {
    if (peer_id == kBroadcastPeer || host == 0 || host[0] == '\0') {
        return false;
    }
    if (findPeer(peer_id) >= 0) {
        return false;  /* ya existe */
    }
    if (peer_count_ >= kMaxUdpPeers) {
        return false;
    }
    Peer &peer = peers_[peer_count_++];
    peer.id = peer_id;
    peer.port = port;
    strncpy(peer.host, host, sizeof(peer.host) - 1);
    peer.host[sizeof(peer.host) - 1] = '\0';
    return true;
}

bool UdpTunnelTransport::removePeer(uint16_t peer_id) {
    const int16_t index = findPeer(peer_id);
    if (index < 0) {
        return false;
    }
    for (uint8_t i = static_cast<uint8_t>(index); i + 1 < peer_count_; ++i) {
        peers_[i] = peers_[i + 1];
    }
    --peer_count_;
    return true;
}

void UdpTunnelTransport::clearPeers() {
    peer_count_ = 0;
}

bool UdpTunnelTransport::send(uint16_t peer, const uint8_t *data, size_t len) {
    if (!started_) {
        return false;
    }
    if (len > kMaxUdpDatagram) {
        return false;
    }

    if (peer == kBroadcastPeer) {
        bool all_ok = true;
        for (uint8_t i = 0; i < peer_count_; ++i) {
            const Peer &p = peers_[i];
            if (!udp_.beginPacket(p.host, p.port)) {
                all_ok = false;
                continue;
            }
            udp_.write(data, len);
            if (!udp_.endPacket()) {
                all_ok = false;
            }
        }
        return all_ok && (peer_count_ > 0);
    }

    const int16_t index = findPeer(peer);
    if (index < 0) {
        return false;
    }
    const Peer &p = peers_[static_cast<uint8_t>(index)];
    if (!udp_.beginPacket(p.host, p.port)) {
        return false;
    }
    udp_.write(data, len);
    return udp_.endPacket() != 0;
}

bool UdpTunnelTransport::pump() {
    bool got = false;
    while (rx_count_ < kUdpRxDepth && udp_.parsePacket() > 0) {
        const IPAddress remote_ip = udp_.remoteIP();
        const uint16_t remote_port = udp_.remotePort();

        /* Identifica el peer emisor por IP+puerto remotos. */
        uint16_t peer_id = kBroadcastPeer;
        for (uint8_t i = 0; i < peer_count_; ++i) {
            const Peer &p = peers_[i];
            IPAddress addr;
            if (addr.fromString(p.host) && addr == remote_ip && p.port == remote_port) {
                peer_id = p.id;
                break;
            }
        }

        const int available = udp_.available();
        const size_t packet_len = (available > 0) ? static_cast<size_t>(available) : 0;
        if (packet_len > kMaxUdpDatagram) {
            /* Descartar datagramas demasiado grandes. */
            uint8_t discard[kMaxUdpDatagram];
            udp_.read(discard, kMaxUdpDatagram);
            continue;
        }

        RxPacket &packet = rx_queue_[(rx_head_ + rx_count_) % kUdpRxDepth];
        if (packet_len > 0) {
            udp_.read(packet.data, packet_len);
        }
        packet.len = packet_len;
        packet.peer = peer_id;
        ++rx_count_;
        got = true;
    }
    return got;
}

bool UdpTunnelTransport::receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                                 size_t &len) {
    if (pump() && rx_count_ > 0) {
        const RxPacket &packet = rx_queue_[rx_head_];
        const size_t to_copy = (packet.len <= max_len) ? packet.len : max_len;
        if (to_copy > 0) {
            memcpy(data, packet.data, to_copy);
        }
        len = to_copy;
        peer_out = packet.peer;
        rx_head_ = (rx_head_ + 1) % kUdpRxDepth;
        --rx_count_;
        return true;
    }
    return false;
}

bool UdpTunnelTransport::available() const {
    return rx_count_ > 0 || (started_ && udp_.available() > 0);
}

}  // namespace pcd

#endif  /* ARDUINO_ARCH_ESP32 */
