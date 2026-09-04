#include "gateway/node_registry.h"

#include <string.h>

namespace pcd {

NodeRegistry::NodeRegistry() : count_(0) {
    clear();
}

void NodeRegistry::clear() {
    count_ = 0;
    memset(nodes_, 0, sizeof(nodes_));
    for (uint8_t i = 0; i < CAN_MAX_DISCOVERED_NODES; ++i) {
        nodes_[i].id_mode = NODE_ID_MANUAL;
    }
}

DiscoveredNode *NodeRegistry::ensure(uint16_t node_id) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].node_id == node_id) {
            return &nodes_[i];
        }
    }
    if (count_ >= CAN_MAX_DISCOVERED_NODES) {
        return 0;
    }
    DiscoveredNode &node = nodes_[count_++];
    memset(&node, 0, sizeof(node));
    node.node_id = node_id;
    node.id_mode = NODE_ID_MANUAL;
    return &node;
}

bool NodeRegistry::addResource(DiscoveredNode &node, uint8_t resource, uint8_t channel) {
    for (uint8_t i = 0; i < node.resource_count; ++i) {
        if (node.resources[i].resource == resource && node.resources[i].channel == channel) {
            return true;
        }
    }
    if (node.resource_count >= CAN_MAX_DISCOVERED_RESOURCES) {
        return false;
    }
    node.resources[node.resource_count].resource = resource;
    node.resources[node.resource_count].channel = channel;
    ++node.resource_count;
    return true;
}

bool NodeRegistry::observe(const CanFrame &frame, uint32_t now_ms) {
    const CanId id = frame.fields();
    if (id.source == 0 || (id.msg_type != MSG_HEARTBEAT && id.msg_type != MSG_DISCOVERY)) {
        return false;
    }
    DiscoveredNode *node = ensure(id.source);
    if (node == 0) {
        return false;
    }
    ++node->seen_count;
    node->online = true;
    node->last_seen_ms = now_ms;

    if (id.msg_type == MSG_HEARTBEAT) {
        node->health = frame.data[2];
        node->uptime_s = readUint32BE(&frame.data[3]);
        return true;
    }

    if (frame.data[2] == DISCOVERY_ANNOUNCE) {
        node->id_mode = frame.data[3] == NODE_ID_AUTOMATIC ? NODE_ID_AUTOMATIC : NODE_ID_MANUAL;
        return true;
    }
    if (frame.data[2] == DISCOVERY_RESOURCE) {
        return addResource(*node, frame.resource(), frame.channel());
    }
    return frame.data[2] == DISCOVERY_REQUEST;
}

void NodeRegistry::expire(uint32_t now_ms, uint32_t timeout_ms) {
    for (uint8_t i = 0; i < count_; ++i) {
        DiscoveredNode &node = nodes_[i];
        if (node.online && (now_ms - node.last_seen_ms) >= timeout_ms) {
            node.online = false;
        }
    }
}

const DiscoveredNode *NodeRegistry::at(uint8_t index) const {
    return index < count_ ? &nodes_[index] : 0;
}

const DiscoveredNode *NodeRegistry::find(uint16_t node_id) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].node_id == node_id) {
            return &nodes_[i];
        }
    }
    return 0;
}

}  // namespace pcd
