#include "v6/pcd_topology.h"

namespace pcd {

PCD_TopologyDiscovery::PCD_TopologyDiscovery() : count_(0) {
    for (uint8_t i = 0; i < kMaxNodes; ++i) {
        nodes_[i].valid = false;
    }
}

PCD_TopologyNode *PCD_TopologyDiscovery::find(uint16_t nodeId) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].valid && nodes_[i].nodeId == nodeId) {
            return &nodes_[i];
        }
    }
    return 0;
}

const PCD_TopologyNode *PCD_TopologyDiscovery::find(uint16_t nodeId) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].valid && nodes_[i].nodeId == nodeId) {
            return &nodes_[i];
        }
    }
    return 0;
}

PCD_Error PCD_TopologyDiscovery::update(uint16_t nodeId, uint8_t networkId, uint32_t nowMs) {
    PCD_TopologyNode *node = find(nodeId);
    if (node == 0) {
        if (count_ >= kMaxNodes) {
            return PCD_ERR_NO_MEMORY;
        }
        node = &nodes_[count_++];
        node->nodeId = nodeId;
        node->valid = true;
    }
    node->networkId = networkId;
    node->state = PCD_NODE_ONLINE;
    node->lastSeenMs = nowMs;
    return PCD_OK;
}

uint8_t PCD_TopologyDiscovery::poll(uint32_t nowMs, uint32_t timeoutMs) {
    uint8_t changed = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].valid && nodes_[i].state == PCD_NODE_ONLINE &&
            (nowMs - nodes_[i].lastSeenMs) >= timeoutMs) {
            nodes_[i].state = PCD_NODE_OFFLINE;
            ++changed;
        }
    }
    return changed;
}

uint8_t PCD_TopologyDiscovery::onlineCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].valid && nodes_[i].state == PCD_NODE_ONLINE) {
            ++n;
        }
    }
    return n;
}

uint8_t PCD_TopologyDiscovery::offlineCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].valid && nodes_[i].state == PCD_NODE_OFFLINE) {
            ++n;
        }
    }
    return n;
}

uint8_t PCD_TopologyDiscovery::networkCount() const {
    uint8_t networks[8] = {0};
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        bool seen = false;
        for (uint8_t j = 0; j < n; ++j) {
            if (networks[j] == nodes_[i].networkId) {
                seen = true;
                break;
            }
        }
        if (!seen && n < 8) {
            networks[n++] = nodes_[i].networkId;
        }
    }
    return n;
}

PCD_NodeState PCD_TopologyDiscovery::state(uint16_t nodeId) const {
    const PCD_TopologyNode *node = find(nodeId);
    return node == 0 ? PCD_NODE_OFFLINE : node->state;
}

const PCD_TopologyNode *PCD_TopologyDiscovery::at(uint8_t index) const {
    return index < count_ ? &nodes_[index] : 0;
}

}  // namespace pcd
