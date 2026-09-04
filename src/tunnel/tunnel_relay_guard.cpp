#include "tunnel/tunnel_relay_guard.h"

#include <string.h>

namespace pcd {

TunnelRelayGuard::TunnelRelayGuard(uint8_t local_segment)
    : local_segment_(local_segment), window_ms_(5000), next_slot_(0) {
    clear();
}

void TunnelRelayGuard::clear() {
    memset(entries_, 0, sizeof(entries_));
    next_slot_ = 0;
}

uint32_t TunnelRelayGuard::fingerprint(const CanFrame &frame) const {
    uint32_t hash = 2166136261UL;
    hash ^= frame.id;
    hash *= 16777619UL;
    hash ^= frame.dlc;
    hash *= 16777619UL;
    for (uint8_t i = 0; i < frame.dlc && i < kPayloadSize; ++i) {
        hash ^= frame.data[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool TunnelRelayGuard::accept(uint8_t source_segment, const CanFrame &frame,
                              uint32_t now_ms) {
    if (source_segment == local_segment_) {
        return false;
    }

    const uint32_t frame_fingerprint = fingerprint(frame);
    for (uint8_t i = 0; i < CAN_TUNNEL_REPLAY_SLOTS; ++i) {
        Entry &entry = entries_[i];
        if (!entry.valid) {
            continue;
        }
        if ((now_ms - entry.seen_ms) >= window_ms_) {
            entry.valid = false;
            continue;
        }
        if (entry.fingerprint == frame_fingerprint &&
            entry.source_segment == source_segment) {
            return false;
        }
    }

    Entry &slot = entries_[next_slot_];
    slot.fingerprint = frame_fingerprint;
    slot.seen_ms = now_ms;
    slot.source_segment = source_segment;
    slot.valid = true;
    next_slot_ = static_cast<uint8_t>((next_slot_ + 1) % CAN_TUNNEL_REPLAY_SLOTS);
    return true;
}

}  // namespace pcd
