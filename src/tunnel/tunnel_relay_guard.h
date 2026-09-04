#pragma once

#include <stdint.h>

#include "can_protocol.h"

namespace pcd {

#ifndef CAN_TUNNEL_REPLAY_SLOTS
#define CAN_TUNNEL_REPLAY_SLOTS 16
#endif

/*
 * Politica local de relay: evita reenviar la misma trama dentro de una
 * ventana de tiempo y recuerda el segmento de origen. No altera el datagrama
 * PCD de 16 bytes ni reemplaza autenticacion criptografica.
 */
class TunnelRelayGuard {
  public:
    explicit TunnelRelayGuard(uint8_t local_segment = 0);

    /* true si la trama puede entrar a este segmento. */
    bool accept(uint8_t source_segment, const CanFrame &frame, uint32_t now_ms);
    void clear();
    void setWindow(uint32_t window_ms) { window_ms_ = window_ms; }
    uint8_t localSegment() const { return local_segment_; }

  private:
    struct Entry {
        uint32_t fingerprint;
        uint32_t seen_ms;
        uint8_t source_segment;
        bool valid;
    };

    uint32_t fingerprint(const CanFrame &frame) const;
    uint8_t local_segment_;
    uint32_t window_ms_;
    uint8_t next_slot_;
    Entry entries_[CAN_TUNNEL_REPLAY_SLOTS];
};

}  // namespace pcd
