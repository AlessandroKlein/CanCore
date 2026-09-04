#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ota_manager.h"

namespace pcd {

/* Control local de ventanas OTA: prepara el emisor para ACK/retransmision. */
class OtaWindowController {
  public:
    explicit OtaWindowController(OtaManager &manager, uint8_t window_size = 16);

    bool next(CanFrame &out);
    bool acknowledge(uint8_t window, uint16_t missing_mask);
    void reset();
    uint8_t window() const { return window_index_; }
    uint16_t pendingMask() const { return pending_mask_; }
    bool waitingAck() const { return waiting_ack_; }

  private:
    OtaManager &manager_;
    uint8_t window_size_;
    uint8_t window_index_;
    uint8_t frames_in_window_;
    uint16_t pending_mask_;
    bool waiting_ack_;
};

}  // namespace pcd
