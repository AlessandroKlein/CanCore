#include "ota_window.h"

namespace pcd {

OtaWindowController::OtaWindowController(OtaManager &manager, uint8_t window_size)
    : manager_(manager),
      window_size_(window_size > 16 ? 16 : window_size),
      window_index_(0),
      frames_in_window_(0),
      pending_mask_(0),
      waiting_ack_(false) {
    if (window_size_ == 0) window_size_ = 1;
}

void OtaWindowController::reset() {
    window_index_ = 0;
    frames_in_window_ = 0;
    pending_mask_ = 0;
    waiting_ack_ = false;
}

bool OtaWindowController::next(CanFrame &out) {
    if (!manager_.active() || waiting_ack_) return false;
    if (!manager_.nextFrame(out)) return false;
    ++frames_in_window_;
    pending_mask_ |= static_cast<uint16_t>(1U << (frames_in_window_ - 1));
    if (frames_in_window_ >= window_size_ || manager_.complete()) waiting_ack_ = true;
    return true;
}

bool OtaWindowController::acknowledge(uint8_t window, uint16_t missing_mask) {
    if (!waiting_ack_ || window != window_index_) return false;
    if (missing_mask != 0) {
        /* El emisor debe reiniciarse con la misma imagen para retransmitir.
         * La máscara queda expuesta para que la aplicación reprograme su cola. */
        pending_mask_ = missing_mask;
        return true;
    }
    ++window_index_;
    frames_in_window_ = 0;
    pending_mask_ = 0;
    waiting_ack_ = false;
    return true;
}

}  // namespace pcd
