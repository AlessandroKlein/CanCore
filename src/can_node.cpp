#include "can_node.h"

namespace pcd {

CanNode::CanNode(ICanBus &bus, uint16_t node_id)
    : bus_(bus),
      node_id_(node_id),
            node_id_mode_(NODE_ID_MANUAL),
      last_heartbeat_ms_(0),
      boot_ms_(0),
      boot_ms_valid_(false),
      heartbeat_sent_(false),
    discovery_sent_(false),
      subscription_count_(0),
    listen_filter_count_(0),
      config_listener_(0),
      config_ctx_(0),
      frame_listener_(0),
      frame_ctx_(0) {
    devices_.setStateEmitter(&CanNode::emitState, this);
}

void CanNode::setNodeId(uint16_t node_id) {
    if (isValidSource(node_id)) {
        node_id_ = node_id;
        discovery_sent_ = false;
    }
}

void CanNode::setAutomaticNodeId(uint32_t unique_value, uint16_t salt) {
    setNodeId(deriveAutomaticNodeId(unique_value, salt));
    node_id_mode_ = NODE_ID_AUTOMATIC;
}

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
    return devices_.registerChannel(resource, channel, handler, ctx);
}

void CanNode::emitState(uint8_t resource, uint8_t channel, float value, uint16_t flags,
                        void *ctx) {
    static_cast<CanNode *>(ctx)->publishState(resource, channel, value, flags);
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

bool CanNode::addListenFilter(uint16_t source, uint8_t resource, uint8_t channel) {
    if (listen_filter_count_ >= CAN_MAX_SUBSCRIPTIONS) {
        return false;
    }
    Subscription &entry = listen_filters_[listen_filter_count_++];
    entry.source = source;
    entry.resource = resource;
    entry.channel = channel;
    entry.listener = 0;
    entry.ctx = 0;
    return true;
}

void CanNode::clearListenFilters() {
    listen_filter_count_ = 0;
}

bool CanNode::acceptsListenFilter(const CanFrame &frame) const {
    if (listen_filter_count_ == 0) {
        return true;
    }
    const CanId id = frame.fields();
    for (uint8_t i = 0; i < listen_filter_count_; ++i) {
        const Subscription &filter = listen_filters_[i];
        if ((filter.source == kAnySource || filter.source == id.source) &&
            filter.resource == frame.resource() &&
            (filter.channel == kAnyChannel || filter.channel == frame.channel())) {
            return true;
        }
    }
    return false;
}

void CanNode::onConfig(ConfigListener listener, void *ctx) {
    config_listener_ = listener;
    config_ctx_ = ctx;
}

void CanNode::onAnyFrame(FrameListener listener, void *ctx) {
    frame_listener_ = listener;
    frame_ctx_ = ctx;
}

bool CanNode::isForThisNode(const CanId &id) const {
    return id.target == address() || id.target == kBroadcastTarget;
}

void CanNode::dispatchState(const CanFrame &frame) {
    if (!acceptsListenFilter(frame)) {
        return;
    }
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

    if (frame_listener_ != 0) {
        frame_listener_(frame, frame_ctx_);
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

        case MSG_DISCOVERY:
            if (frame.data[2] == DISCOVERY_REQUEST && isForThisNode(id)) {
                publishDiscovery();
                return true;
            }
            return frame.data[2] == DISCOVERY_ANNOUNCE || frame.data[2] == DISCOVERY_RESOURCE;

        default:
            return false;
    }
}

bool CanNode::applyLocal(uint8_t resource, uint8_t channel, uint8_t action, uint32_t param) {
    float value = 0.0f;
    if (!devices_.apply(resource, channel, action, param, value)) {
        return false;
    }
    publishState(resource, channel, value, devices_.flags(resource, channel));
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

    devices_.update(now_ms);

    if (!discovery_sent_) {
        publishDiscovery();
        discovery_sent_ = true;
    }

    if (bus_.isBusOff()) {
        bus_.recover();
    }

    if (!heartbeat_sent_ || (now_ms - last_heartbeat_ms_) >= CAN_HEARTBEAT_PERIOD_MS) {
        sendHeartbeat(now_ms);
    }
}

CanStatus CanNode::publishDiscovery() {
    CanStatus status = bus_.send(
        makeDiscoveryAnnounce(node_id_, node_id_mode_, devices_.count()));
    if (status != CAN_OK) {
        return status;
    }
    for (uint8_t i = 0; i < devices_.count(); ++i) {
        const Channel &channel = devices_.at(i);
        status = bus_.send(makeDiscoveryResource(node_id_, channel.resource, channel.channel));
        if (status != CAN_OK) {
            return status;
        }
    }
    return CAN_OK;
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

CanStatus CanNode::sendConfig(uint8_t target, uint8_t sub_command, const uint8_t *payload,
                              uint8_t payload_len) {
    return bus_.send(makeConfig(node_id_, target, sub_command, payload, payload_len));
}

CanStatus CanNode::sendConfigAck(const uint8_t *payload, uint8_t payload_len) {
    return bus_.send(makeConfig(node_id_, kBroadcastTarget, CFG_ACK, payload, payload_len));
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
