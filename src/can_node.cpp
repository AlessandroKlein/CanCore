#include "can_node.h"

namespace pcd {

CanNode::CanNode(ICanBus &bus, uint16_t node_id)
    : bus_(bus),
      node_id_(node_id),
      last_heartbeat_ms_(0),
      boot_ms_(0),
      boot_ms_valid_(false),
      heartbeat_sent_(false),
      resource_count_(0),
      subscription_count_(0),
      config_listener_(0),
      config_ctx_(0) {}

CanStatus CanNode::begin(uint32_t bitrate) {
    const CanStatus status = bus_.begin(bitrate);
    if (status != CAN_OK) {
        return status;
    }
    bus_.setFilter(CanFilter::acceptAll());
    return CAN_OK;
}

bool CanNode::registerResource(uint8_t resource, uint8_t channel, ResourceHandler handler,
                               void *ctx) {
    if (resource_count_ >= CAN_MAX_SUBSCRIPTIONS || handler == 0) {
        return false;
    }
    ResourceEntry &entry = resources_[resource_count_++];
    entry.resource = resource;
    entry.channel = channel;
    entry.handler = handler;
    entry.ctx = ctx;
    return true;
}

bool CanNode::subscribe(uint16_t source, uint8_t resource, uint8_t channel, StateListener listener,
                        void *ctx) {
    if (subscription_count_ >= CAN_MAX_SUBSCRIPTIONS || listener == 0) {
        return false;
    }
    Subscription &entry = subscriptions_[subscription_count_++];
    entry.source = source;
    entry.resource = resource;
    entry.channel = channel;
    entry.listener = listener;
    entry.ctx = ctx;
    return true;
}

void CanNode::onConfig(ConfigListener listener, void *ctx) {
    config_listener_ = listener;
    config_ctx_ = ctx;
}

bool CanNode::isForThisNode(const CanId &id) const {
    return id.target == address() || id.target == kBroadcastTarget;
}

ResourceEntry *CanNode::findResource(uint8_t resource, uint8_t channel) {
    for (uint8_t i = 0; i < resource_count_; ++i) {
        if (resources_[i].resource == resource &&
            (resources_[i].channel == channel || resources_[i].channel == kAnyChannel)) {
            return &resources_[i];
        }
    }
    return 0;
}

void CanNode::dispatchState(const CanFrame &frame) {
    const CanId id = frame.fields();
    const float value = frameValue(frame);
    for (uint8_t i = 0; i < subscription_count_; ++i) {
        const Subscription &sub = subscriptions_[i];
        if (sub.source != kAnySource && sub.source != id.source) {
            continue;
        }
        if (sub.resource != frame.resource()) {
            continue;
        }
        if (sub.channel != kAnyChannel && sub.channel != frame.channel()) {
            continue;
        }
        sub.listener(frame, value, sub.ctx);
    }
}

bool CanNode::handleFrame(const CanFrame &frame) {
    const CanId id = frame.fields();

    /* Las tramas emitidas por el propio nodo se ignoran. */
    if (id.source == node_id_) {
        return false;
    }

    switch (id.msg_type) {
        case MSG_STATE:
            dispatchState(frame);
            return true;

        case MSG_EVENT: {
            /* Un evento de entrada remota tambien puede alimentar suscripciones. */
            if (frame.resource() == RES_DIGITAL_INPUT && id.target == kBroadcastTarget) {
                dispatchState(frame);
                return true;
            }
            if (!isForThisNode(id)) {
                return false;
            }
            return applyLocal(frame.resource(), frame.channel(), frame.data[2], frameParam(frame));
        }

        case MSG_GROUP: {
            /* Escenas: se aplican sobre los recursos locales coincidentes. */
            return applyLocal(frame.resource(), frame.channel(), frame.data[2], frameParam(frame));
        }

        case MSG_CONFIG: {
            if (!isForThisNode(id) || config_listener_ == 0) {
                return false;
            }
            config_listener_(frame, config_ctx_);
            return true;
        }

        default:
            return false;
    }
}

bool CanNode::applyLocal(uint8_t resource, uint8_t channel, uint8_t action, uint32_t param) {
    ResourceEntry *entry = findResource(resource, channel);
    if (entry == 0) {
        return false;
    }
    float value = 0.0f;
    if (!entry->handler(channel, action, param, value, entry->ctx)) {
        return false;
    }
    publishState(resource, channel, value);
    return true;
}

void CanNode::poll(uint32_t now_ms) {
    if (!boot_ms_valid_) {
        boot_ms_ = now_ms;
        boot_ms_valid_ = true;
    }

    CanFrame frame;
    while (bus_.receive(frame) == CAN_OK) {
        handleFrame(frame);
    }

    if (bus_.isBusOff()) {
        bus_.recover();
    }

    if (!heartbeat_sent_ || (now_ms - last_heartbeat_ms_) >= CAN_HEARTBEAT_PERIOD_MS) {
        sendHeartbeat(now_ms);
    }
}

CanStatus CanNode::sendCommand(uint8_t target, uint8_t resource, uint8_t channel, uint8_t action,
                               uint32_t param) {
    return bus_.send(makeCommand(node_id_, target, resource, channel, action, param));
}

CanStatus CanNode::publishState(uint8_t resource, uint8_t channel, float value, uint16_t flags) {
    return bus_.send(makeStateBroadcast(node_id_, resource, channel, value, flags));
}

CanStatus CanNode::publishTelemetry(uint8_t resource, uint8_t channel, float value,
                                    uint16_t flags) {
    return bus_.send(makeTelemetry(node_id_, resource, channel, value, flags));
}

CanStatus CanNode::publishInputEvent(uint8_t channel, uint8_t event) {
    return bus_.send(makeInputEvent(node_id_, channel, event));
}

CanStatus CanNode::sendHeartbeat(uint32_t now_ms) {
    if (!boot_ms_valid_) {
        boot_ms_ = now_ms;
        boot_ms_valid_ = true;
    }
    last_heartbeat_ms_ = now_ms;
    heartbeat_sent_ = true;
    const uint32_t uptime_s = (now_ms - boot_ms_) / 1000UL;
    return bus_.send(makeHeartbeat(node_id_, uptime_s, bus_.isBusOff() ? 1 : 0));
}

}  // namespace pcd
