#include "v6/pcd_router.h"

namespace pcd {

PCD_CanRouter::PCD_CanRouter() : iface_count_(0), rule_count_(0) {
    for (uint8_t i = 0; i < kMaxInterfaces; ++i) {
        interfaces_[i] = 0;
    }
}

PCD_Error PCD_CanRouter::addInterface(PCD_CanInterface *iface) {
    if (iface == 0 || iface_count_ >= kMaxInterfaces) {
        return PCD_ERR_NO_MEMORY;
    }
    interfaces_[iface_count_++] = iface;
    return PCD_OK;
}

PCD_Error PCD_CanRouter::addRule(const PCD_RoutingRule &rule) {
    if (rule_count_ >= kMaxRules) {
        return PCD_ERR_NO_MEMORY;
    }
    rules_[rule_count_++] = rule;
    return PCD_OK;
}

PCD_CanInterface *PCD_CanRouter::interfaceForNetwork(uint8_t networkId) const {
    for (uint8_t i = 0; i < iface_count_; ++i) {
        if (interfaces_[i]->networkId() == networkId) {
            return interfaces_[i];
        }
    }
    return 0;
}

void PCD_CanRouter::sendTo(PCD_CanInterface *iface, const PCD_CAN_ID &id, const uint8_t *payload,
                           uint8_t len) {
    if (iface != 0) {
        iface->send(id, payload, len);
    }
}

uint8_t PCD_CanRouter::route(const PCD_CAN_ID &id, const uint8_t *payload, uint8_t len,
                             uint8_t sourceNetwork) {
    uint8_t delivered = 0;

    for (uint8_t r = 0; r < rule_count_; ++r) {
        const PCD_RoutingRule &rule = rules_[r];
        if (!rule.matches(sourceNetwork, id.messageType)) {
            continue;
        }
        switch (rule.action) {
            case PCD_ROUTE_DROP:
                return 0;
            case PCD_ROUTE_FORWARD: {
                PCD_CanInterface *dst = interfaceForNetwork(rule.destinationNetwork);
                sendTo(dst, id, payload, len);
                delivered += (dst != 0) ? 1 : 0;
                break;
            }
            case PCD_ROUTE_MODIFY: {
                PCD_CAN_ID modified = id;
                modified.network = rule.destinationNetwork;
                PCD_CanInterface *dst = interfaceForNetwork(rule.destinationNetwork);
                sendTo(dst, modified, payload, len);
                delivered += (dst != 0) ? 1 : 0;
                break;
            }
            case PCD_ROUTE_BROADCAST: {
                for (uint8_t i = 0; i < iface_count_; ++i) {
                    if (interfaces_[i]->networkId() != sourceNetwork) {
                        interfaces_[i]->send(id, payload, len);
                        ++delivered;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    return delivered;
}

}  // namespace pcd
