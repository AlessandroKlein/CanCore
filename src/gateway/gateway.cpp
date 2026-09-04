#include "gateway/gateway.h"

#include "bridge/bridge.h"

namespace pcd {

Gateway::Gateway(CanNode &node, RouteTable &table)
    : node_(node),
      table_(table),
      router_(table),
      bridge_count_(0) {
    for (uint8_t i = 0; i < 4; ++i) {
        bridges_[i] = 0;
    }
}

void Gateway::begin(const GatewayConfig &config) {
    config_ = config;
    router_.attach(node_, config_.node_id);
    node_.onAnyFrame(&Gateway::frameTrampoline, this);
}

void Gateway::frameTrampoline(const CanFrame &frame, void *ctx) {
    static_cast<Gateway *>(ctx)->onCanFrame(frame);
}

void Gateway::onCanFrame(const CanFrame &frame) {
    nodes_.observe(frame);
    /* 1. El enrutador traduce y ejecuta la tabla de rutas. */
    router_.onCanFrame(frame);

    /* 2. Los bridges observan los estados para publicar hacia afuera. */
    const CanId id = frame.fields();
    if (id.msg_type == MSG_STATE) {
        CanonicalFrame canonical = CanonicalFrame::fromCanFrame(frame);
        for (uint8_t i = 0; i < bridge_count_; ++i) {
            if (bridges_[i] != 0) {
                bridges_[i]->process(canonical);
            }
        }
    }
}

bool Gateway::attachBridge(IBridge &bridge) {
    if (bridge_count_ >= 4) {
        return false;
    }
    bridges_[bridge_count_++] = &bridge;
    return true;
}

void Gateway::poll(uint32_t now_ms) {
    /* Drena los bridges: recibe entradas externas y las entrega al router. */
    for (uint8_t i = 0; i < bridge_count_; ++i) {
        IBridge *bridge = bridges_[i];
        if (bridge != 0 && bridge->available()) {
            CanonicalFrame incoming;
            if (bridge->buildCanonical(incoming)) {
                router_.process(incoming);
            }
        }
    }
    node_.poll(now_ms);
}

}  // namespace pcd
