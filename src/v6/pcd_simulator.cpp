#include "v6/pcd_simulator.h"

namespace pcd {

PCD_NetworkSimulator::PCD_NetworkSimulator() : node_count_(0) {}

PCD_MockCAN *PCD_NetworkSimulator::addNode() {
    if (node_count_ >= kMaxNodes) {
        return 0;
    }
    PCD_MockCAN *node = &nodes_[node_count_];
    node->setNetwork(&network_);
    node->begin();
    ++node_count_;
    return node;
}

void PCD_NetworkSimulator::step(uint32_t nowMs) {
    for (uint8_t i = 0; i < node_count_; ++i) {
        nodes_[i].tick(nowMs);
    }
}

PCD_MockCAN *PCD_NetworkSimulator::node(uint8_t index) {
    return index < node_count_ ? &nodes_[index] : 0;
}

}  // namespace pcd
