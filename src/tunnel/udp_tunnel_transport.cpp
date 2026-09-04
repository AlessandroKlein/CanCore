#include "tunnel/udp_tunnel_transport.h"

#include <string.h>

namespace pcd {

UdpTunnelTransport::UdpTunnelTransport() : head_(0), count_(0) {
    for (size_t i = 0; i < kMaxDatagrams; ++i) {
        queue_[i].peer = 0;
        queue_[i].len = 0;
        memset(queue_[i].data, 0, sizeof(queue_[i].data));
    }
}

bool UdpTunnelTransport::enqueue(uint16_t peer, const uint8_t *data, size_t len) {
    if (data == 0 || len == 0 || len > kMaxPayload || count_ >= kMaxDatagrams) {
        return false;
    }

    const size_t index = (head_ + count_) % kMaxDatagrams;
    queue_[index].peer = peer;
    queue_[index].len = len;
    memcpy(queue_[index].data, data, len);
    ++count_;
    return true;
}

bool UdpTunnelTransport::send(uint16_t peer, const uint8_t *data, size_t len) {
    return enqueue(peer, data, len);
}

bool UdpTunnelTransport::receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                                size_t &len) {
    if (count_ == 0 || data == 0) {
        return false;
    }

    Datagram &datagram = queue_[head_];
    peer_out = datagram.peer;
    len = datagram.len;

    if (len > max_len) {
        len = max_len;
    }

    memcpy(data, datagram.data, len);

    datagram.peer = 0;
    datagram.len = 0;
    memset(datagram.data, 0, sizeof(datagram.data));

    head_ = (head_ + 1) % kMaxDatagrams;
    --count_;
    return true;
}

bool UdpTunnelTransport::available() const {
    return count_ > 0;
}

}  // namespace pcd
