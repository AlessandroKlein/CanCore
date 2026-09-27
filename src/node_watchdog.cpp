#include "node_watchdog.h"

#include <string.h>

namespace pcd {

NodeWatchdog::NodeWatchdog(uint32_t default_timeout_ms)
    : count_(0), default_timeout_ms_(default_timeout_ms), handler_(0), handler_ctx_(0) {
    clear();
}

void NodeWatchdog::clear() {
    count_ = 0;
    memset(nodes_, 0, sizeof(nodes_));
}

void NodeWatchdog::setDefaultTimeout(uint32_t timeout_ms) {
    default_timeout_ms_ = timeout_ms;
}

void NodeWatchdog::setSafeStateHandler(SafeStateHandler handler, void *ctx) {
    handler_ = handler;
    handler_ctx_ = ctx;
}

WatchedNode *NodeWatchdog::ensure(uint16_t node_id) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].node_id == node_id) {
            return &nodes_[i];
        }
    }
    if (count_ >= CAN_MAX_WATCHED_NODES) {
        return 0;
    }
    WatchedNode &node = nodes_[count_++];
    node.node_id = node_id;
    node.last_seen_ms = 0;
    node.timeout_ms = default_timeout_ms_;
    node.missed = 0;
    node.health = HEALTH_OK;
    node.online = true;
    node.ever_seen = true;
    return &node;
}

bool NodeWatchdog::setTimeout(uint16_t node_id, uint32_t timeout_ms) {
    WatchedNode *node = ensure(node_id);
    if (node == 0) {
        return false;
    }
    node->timeout_ms = timeout_ms;
    return true;
}

bool NodeWatchdog::feed(const CanFrame &frame, uint32_t now_ms) {
    const CanId id = frame.fields();
    if (!isValidSource(id.source)) {
        return false;
    }
    if (id.msg_type == MSG_HEARTBEAT) {
        uint32_t uptime = 0;
        uint8_t flags = 0;
        if (!decodeHeartbeat(frame, uptime, flags)) {
            return false;
        }
        WatchedNode *node = ensure(id.source);
        if (node == 0) {
            return false;
        }
        node->last_seen_ms = now_ms;
        node->online = true;
        node->missed = 0;
        node->health = flags;
        return true;
    }

    if (id.msg_type == MSG_STATE && frame.resource() == RES_SYSTEM &&
        frame.channel() == kHealthChannelReport) {
        NodeHealth health;
        if (!decodeHealthReport(frame, health)) {
            return false;
        }
        WatchedNode *node = ensure(id.source);
        if (node == 0) {
            return false;
        }
        node->health = health.flags;
        node->last_seen_ms = now_ms;
        node->online = true;
        node->missed = 0;
        return true;
    }
    return false;
}

uint8_t NodeWatchdog::poll(uint32_t now_ms) {
    uint8_t transitions = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        WatchedNode &node = nodes_[i];
        if (node.timeout_ms == 0) {
            continue;
        }
        const uint32_t elapsed = now_ms - node.last_seen_ms;
        if (elapsed < node.timeout_ms) {
            continue;
        }
        const uint16_t windows = static_cast<uint16_t>(elapsed / node.timeout_ms);
        if (windows > node.missed) {
            const bool was_online = node.online;
            node.missed = windows;
            node.online = false;
            if (was_online && handler_ != 0) {
                handler_(node.node_id, node.last_seen_ms, handler_ctx_);
            }
            if (was_online) {
                ++transitions;
            }
        }
    }
    return transitions;
}

const WatchedNode *NodeWatchdog::find(uint16_t node_id) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (nodes_[i].node_id == node_id) {
            return &nodes_[i];
        }
    }
    return 0;
}

const WatchedNode *NodeWatchdog::at(uint8_t index) const {
    return index < count_ ? &nodes_[index] : 0;
}

bool NodeWatchdog::isOnline(uint16_t node_id) const {
    const WatchedNode *node = find(node_id);
    return node != 0 && node->online;
}

bool NodeWatchdog::age(uint16_t node_id, uint32_t now_ms, uint32_t &age_ms) const {
    const WatchedNode *node = find(node_id);
    if (node == 0) {
        return false;
    }
    age_ms = now_ms - node->last_seen_ms;
    return true;
}

uint16_t NodeWatchdog::missedWindows(uint16_t node_id) const {
    const WatchedNode *node = find(node_id);
    return node == 0 ? 0 : node->missed;
}

uint8_t NodeWatchdog::healthFlags(uint16_t node_id) const {
    const WatchedNode *node = find(node_id);
    return node == 0 ? static_cast<uint8_t>(HEALTH_OK) : node->health;
}

}  // namespace pcd
