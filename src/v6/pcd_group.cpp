#include "v6/pcd_group.h"

namespace pcd {

PCD_GroupRegistry::PCD_GroupRegistry() {
    for (uint8_t i = 0; i < kMaxGroups; ++i) {
        counts_[i] = 0;
        for (uint8_t j = 0; j < kMaxMembers; ++j) {
            groups_[i][j].valid = false;
        }
    }
}

PCD_Error PCD_GroupRegistry::addMember(uint8_t groupId, uint16_t nodeId, uint8_t resourceId) {
    if (groupId >= kMaxGroups) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    for (uint8_t j = 0; j < counts_[groupId]; ++j) {
        if (groups_[groupId][j].valid && groups_[groupId][j].nodeId == nodeId &&
            groups_[groupId][j].resourceId == resourceId) {
            return PCD_OK; /* ya miembro */
        }
    }
    if (counts_[groupId] >= kMaxMembers) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_GroupMember &m = groups_[groupId][counts_[groupId]++];
    m.nodeId = nodeId;
    m.resourceId = resourceId;
    m.valid = true;
    return PCD_OK;
}

bool PCD_GroupRegistry::removeMember(uint8_t groupId, uint16_t nodeId, uint8_t resourceId) {
    if (groupId >= kMaxGroups) {
        return false;
    }
    for (uint8_t j = 0; j < counts_[groupId]; ++j) {
        if (groups_[groupId][j].valid && groups_[groupId][j].nodeId == nodeId &&
            groups_[groupId][j].resourceId == resourceId) {
            groups_[groupId][j].valid = false;
            for (uint8_t k = j + 1; k < counts_[groupId]; ++k) {
                groups_[groupId][k - 1] = groups_[groupId][k];
            }
            --counts_[groupId];
            return true;
        }
    }
    return false;
}

bool PCD_GroupRegistry::inGroup(uint8_t groupId, uint16_t nodeId, uint8_t resourceId) const {
    if (groupId >= kMaxGroups) {
        return false;
    }
    for (uint8_t j = 0; j < counts_[groupId]; ++j) {
        if (groups_[groupId][j].valid && groups_[groupId][j].nodeId == nodeId &&
            groups_[groupId][j].resourceId == resourceId) {
            return true;
        }
    }
    return false;
}

uint8_t PCD_GroupRegistry::memberCount(uint8_t groupId) const {
    return groupId < kMaxGroups ? counts_[groupId] : 0;
}

bool PCD_GroupRegistry::memberAt(uint8_t groupId, uint8_t index, uint16_t &nodeId,
                                 uint8_t &resourceId) const {
    if (groupId >= kMaxGroups || index >= counts_[groupId]) {
        return false;
    }
    nodeId = groups_[groupId][index].nodeId;
    resourceId = groups_[groupId][index].resourceId;
    return true;
}

void PCD_GroupRegistry::clearGroup(uint8_t groupId) {
    if (groupId < kMaxGroups) {
        counts_[groupId] = 0;
    }
}

void PCD_GroupRegistry::clear() {
    for (uint8_t i = 0; i < kMaxGroups; ++i) {
        counts_[i] = 0;
    }
}

}  // namespace pcd
