#include "v6/pcd_store_forward.h"

namespace pcd {

PCD_StoreAndForward::PCD_StoreAndForward() : count_(0) {}

PCD_Error PCD_StoreAndForward::store(uint16_t nodeId, uint8_t resourceId, uint8_t action,
                                     uint32_t param) {
    if (count_ >= kMaxPending) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_PendingMessage &m = pending_[count_++];
    m.targetNodeId = nodeId;
    m.resourceId = resourceId;
    m.action = action;
    m.param = param;
    m.valid = true;
    return PCD_OK;
}

uint8_t PCD_StoreAndForward::pendingFor(uint16_t nodeId) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (pending_[i].valid && pending_[i].targetNodeId == nodeId) {
            ++n;
        }
    }
    return n;
}

bool PCD_StoreAndForward::popFor(uint16_t nodeId, PCD_PendingMessage &out) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (pending_[i].valid && pending_[i].targetNodeId == nodeId) {
            out = pending_[i];
            pending_[i].valid = false;
            /* compacta */
            for (uint8_t j = i + 1; j < count_; ++j) {
                pending_[j - 1] = pending_[j];
            }
            --count_;
            return true;
        }
    }
    return false;
}

void PCD_StoreAndForward::clear() {
    count_ = 0;
}

}  // namespace pcd
