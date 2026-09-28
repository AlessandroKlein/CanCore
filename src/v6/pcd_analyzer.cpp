#include "v6/pcd_analyzer.h"

namespace pcd {

PCD_TrafficAnalyzer::PCD_TrafficAnalyzer()
    : total_frames_(0), total_bytes_(0), total_errors_(0), frames_this_second_(0),
      second_start_ms_(0), node_count_(0) {}

void PCD_TrafficAnalyzer::rollWindow(uint32_t nowMs) {
    if (nowMs >= second_start_ms_ + 1000u) {
        frames_this_second_ = 0;
        second_start_ms_ = nowMs;
    }
}

void PCD_TrafficAnalyzer::record(const PCD_CAN_ID &id, uint8_t len, uint32_t nowMs) {
    rollWindow(nowMs);
    ++total_frames_;
    total_bytes_ += len;
    ++frames_this_second_;

    /* registra el nodo origen si es nuevo */
    bool seen = false;
    for (uint8_t i = 0; i < node_count_; ++i) {
        if (seen_nodes_[i] == id.source) {
            seen = true;
            break;
        }
    }
    if (!seen && node_count_ < kMaxTrackedNodes) {
        seen_nodes_[node_count_++] = id.source;
    }
}

void PCD_TrafficAnalyzer::recordError(uint32_t nowMs) {
    rollWindow(nowMs);
    ++total_errors_;
}

uint8_t PCD_TrafficAnalyzer::utilizationPercent(uint32_t bitrate) const {
    if (bitrate == 0 || frames_this_second_ == 0) {
        return 0;
    }
    const uint64_t bitsPerSec = static_cast<uint64_t>(frames_this_second_) * 130u;
    const uint64_t util = (bitsPerSec * 100u) / bitrate;
    return util > 255u ? 255u : static_cast<uint8_t>(util);
}

bool PCD_TrafficAnalyzer::hasSeenNode(uint8_t address) const {
    for (uint8_t i = 0; i < node_count_; ++i) {
        if (seen_nodes_[i] == address) {
            return true;
        }
    }
    return false;
}

void PCD_TrafficAnalyzer::reset() {
    total_frames_ = 0;
    total_bytes_ = 0;
    total_errors_ = 0;
    frames_this_second_ = 0;
    second_start_ms_ = 0;
    node_count_ = 0;
}

}  // namespace pcd
