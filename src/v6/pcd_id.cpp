#include "v6/pcd_id.h"

namespace pcd {

uint32_t PCD_CAN_ID::encode() const {
    uint32_t id = 0;
    id |= (static_cast<uint32_t>(priority) & kPcdPriorityMask) << kPcdPriorityShift;
    id |= (static_cast<uint32_t>(messageType) & kPcdMsgTypeMask) << kPcdMsgTypeShift;
    id |= (static_cast<uint32_t>(destination) & kPcdDestinationMask) << kPcdDestinationShift;
    id |= (static_cast<uint32_t>(source) & kPcdSourceMask) << kPcdSourceShift;
    id |= (static_cast<uint32_t>(network) & kPcdNetworkMask) << kPcdNetworkShift;
    return id & 0x1FFFFFFFUL;
}

PCD_CAN_ID PCD_CAN_ID::decode(uint32_t id) {
    PCD_CAN_ID out;
    out.priority = static_cast<uint8_t>((id >> kPcdPriorityShift) & kPcdPriorityMask);
    out.messageType = static_cast<uint8_t>((id >> kPcdMsgTypeShift) & kPcdMsgTypeMask);
    out.destination = static_cast<uint8_t>((id >> kPcdDestinationShift) & kPcdDestinationMask);
    out.source = static_cast<uint8_t>((id >> kPcdSourceShift) & kPcdSourceMask);
    out.network = static_cast<uint8_t>((id >> kPcdNetworkShift) & kPcdNetworkMask);
    return out;
}

uint16_t PCD_CAN_ID::nodeId() const {
    return pcdMakeNodeId(network, source);
}

uint16_t pcdMakeNodeId(uint8_t network, uint8_t address) {
    return static_cast<uint16_t>(((network & PCD_NETWORK_MAX) << 8) | (address & 0xFF));
}

uint8_t pcdNodeNetwork(uint16_t node_id) {
    return static_cast<uint8_t>((node_id >> 8) & PCD_NETWORK_MAX);
}

uint8_t pcdNodeAddress(uint16_t node_id) {
    return static_cast<uint8_t>(node_id & 0xFF);
}

bool pcdIsValidNodeAddress(uint8_t address) {
    return address != PCD_NODE_INVALID && address != PCD_NODE_BROADCAST;
}

}  // namespace pcd
