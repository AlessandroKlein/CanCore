#include "v6/pcd_security.h"

#include <string.h>

#include "v6/pcd_crc.h"

namespace pcd {

bool PCD_NoSecurityProvider::encrypt(const uint8_t *in, size_t in_len, uint8_t *out,
                                     size_t *out_len) {
    if (in == 0 || out == 0 || out_len == 0 || in_len > *out_len) {
        return false;
    }
    memcpy(out, in, in_len);
    *out_len = in_len;
    return true;
}

bool PCD_NoSecurityProvider::decrypt(const uint8_t *in, size_t in_len, uint8_t *out,
                                     size_t *out_len) {
    return encrypt(in, in_len, out, out_len);
}

bool PCD_CrcOnlyProvider::sign(const uint8_t *data, size_t len, uint8_t *tag, size_t tag_len) {
    if (data == 0 || tag == 0 || tag_len < 4) {
        return false;
    }
    const uint32_t crc = pcd_crc32(data, len);
    tag[0] = static_cast<uint8_t>(crc & 0xFFu);
    tag[1] = static_cast<uint8_t>((crc >> 8) & 0xFFu);
    tag[2] = static_cast<uint8_t>((crc >> 16) & 0xFFu);
    tag[3] = static_cast<uint8_t>((crc >> 24) & 0xFFu);
    return true;
}

bool PCD_CrcOnlyProvider::verify(const uint8_t *data, size_t len, const uint8_t *tag,
                                 size_t tag_len) {
    if (data == 0 || tag == 0 || tag_len < 4) {
        return false;
    }
    const uint32_t crc = pcd_crc32(data, len);
    return (tag[0] == static_cast<uint8_t>(crc & 0xFFu)) &&
           (tag[1] == static_cast<uint8_t>((crc >> 8) & 0xFFu)) &&
           (tag[2] == static_cast<uint8_t>((crc >> 16) & 0xFFu)) &&
           (tag[3] == static_cast<uint8_t>((crc >> 24) & 0xFFu));
}

PCD_ReplayGuard::PCD_ReplayGuard() : count_(0) {
    for (uint8_t i = 0; i < kMaxPeers; ++i) {
        peers_[i].valid = false;
    }
}

PCD_ReplayGuard::Peer *PCD_ReplayGuard::find(uint16_t nodeId) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (peers_[i].valid && peers_[i].nodeId == nodeId) {
            return &peers_[i];
        }
    }
    return 0;
}

const PCD_ReplayGuard::Peer *PCD_ReplayGuard::find(uint16_t nodeId) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (peers_[i].valid && peers_[i].nodeId == nodeId) {
            return &peers_[i];
        }
    }
    return 0;
}

bool PCD_ReplayGuard::isReplay(uint16_t nodeId, uint32_t counter) const {
    const Peer *peer = find(nodeId);
    if (peer == 0) {
        return false; /* primer mensaje: valido */
    }
    if (counter > peer->highest) {
        return false;
    }
    const uint32_t delta = peer->highest - counter;
    if (delta == 0) {
        return true; /* ya se vio el mayor */
    }
    if (delta > kWindowBits) {
        return true; /* demasiado antiguo */
    }
    return (peer->seen & (1u << (delta - 1))) != 0;
}

bool PCD_ReplayGuard::accept(uint16_t nodeId, uint32_t counter) {
    if (isReplay(nodeId, counter)) {
        return false;
    }
    Peer *peer = find(nodeId);
    if (peer == 0) {
        if (count_ >= kMaxPeers) {
            return false;
        }
        peer = &peers_[count_++];
        peer->nodeId = nodeId;
        peer->highest = counter;
        peer->seen = 0;
        peer->valid = true;
        return true;
    }

    if (counter > peer->highest) {
        const uint32_t delta = counter - peer->highest;
        if (delta >= kWindowBits) {
            peer->seen = 0;
        } else {
            const uint32_t oldHighestBit = (delta > 0) ? (1u << (delta - 1)) : 0u;
            peer->seen = (peer->seen << delta) | oldHighestBit;
        }
        peer->highest = counter;
    } else {
        const uint32_t delta = peer->highest - counter;
        peer->seen |= (1u << (delta - 1));
    }
    return true;
}

void PCD_ReplayGuard::reset(uint16_t nodeId) {
    Peer *peer = find(nodeId);
    if (peer != 0) {
        peer->valid = false;
    }
}

void PCD_ReplayGuard::clear() {
    for (uint8_t i = 0; i < kMaxPeers; ++i) {
        peers_[i].valid = false;
    }
    count_ = 0;
}

}  // namespace pcd
