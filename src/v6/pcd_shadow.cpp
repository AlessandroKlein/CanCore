#include "v6/pcd_shadow.h"

namespace pcd {

PCD_DeviceShadow::PCD_DeviceShadow() : count_(0) {}

PCD_ShadowEntry *PCD_DeviceShadow::find(uint16_t nodeId, uint8_t resourceId) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (entries_[i].nodeId == nodeId && entries_[i].resourceId == resourceId) {
            return &entries_[i];
        }
    }
    return 0;
}

const PCD_ShadowEntry *PCD_DeviceShadow::find(uint16_t nodeId, uint8_t resourceId) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (entries_[i].nodeId == nodeId && entries_[i].resourceId == resourceId) {
            return &entries_[i];
        }
    }
    return 0;
}

PCD_Error PCD_DeviceShadow::setDesired(uint16_t nodeId, uint8_t resourceId, bool value) {
    PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0) {
        if (count_ >= kMaxEntries) {
            return PCD_ERR_NO_MEMORY;
        }
        entry = &entries_[count_++];
        entry->nodeId = nodeId;
        entry->resourceId = resourceId;
    }
    entry->desired = value;
    entry->desiredValid = true;
    return PCD_OK;
}

PCD_Error PCD_DeviceShadow::setReported(uint16_t nodeId, uint8_t resourceId, bool value) {
    PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0) {
        if (count_ >= kMaxEntries) {
            return PCD_ERR_NO_MEMORY;
        }
        entry = &entries_[count_++];
        entry->nodeId = nodeId;
        entry->resourceId = resourceId;
    }
    entry->reported = value;
    entry->reportedValid = true;
    return PCD_OK;
}

bool PCD_DeviceShadow::getReported(uint16_t nodeId, uint8_t resourceId, bool &value) const {
    const PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0 || !entry->reportedValid) {
        return false;
    }
    value = entry->reported;
    return true;
}

bool PCD_DeviceShadow::getDesired(uint16_t nodeId, uint8_t resourceId, bool &value) const {
    const PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0 || !entry->desiredValid) {
        return false;
    }
    value = entry->desired;
    return true;
}

bool PCD_DeviceShadow::isPending(uint16_t nodeId, uint8_t resourceId) const {
    const PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0) {
        return false;
    }
    /* Pendiente cuando hay un deseo valido que el nodo no confirmo. */
    return entry->desiredValid && (!entry->reportedValid || entry->reported != entry->desired);
}

bool PCD_DeviceShadow::isSynced(uint16_t nodeId, uint8_t resourceId) const {
    const PCD_ShadowEntry *entry = find(nodeId, resourceId);
    if (entry == 0) {
        return false;
    }
    return entry->desiredValid && entry->reportedValid && entry->reported == entry->desired;
}

uint8_t PCD_DeviceShadow::pendingCount() const {
    uint8_t pending = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (isPending(entries_[i].nodeId, entries_[i].resourceId)) {
            ++pending;
        }
    }
    return pending;
}

void PCD_DeviceShadow::clear() {
    count_ = 0;
}

}  // namespace pcd
