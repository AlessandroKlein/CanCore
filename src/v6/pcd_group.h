#pragma once

/*
 * PCD v6.4 - Direccionamiento por grupos.
 *
 * Permite enviar un comando a un grupo (ej. "luces del living", "HVAC") para
 * que varios nodos respondan con una sola trama, reduciendo el trafico en
 * instalaciones grandes.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

struct PCD_GroupMember {
    uint16_t nodeId;
    uint8_t resourceId;
    bool valid;
};

class PCD_GroupRegistry {
  public:
    static const uint8_t kMaxGroups = 16;
    static const uint8_t kMaxMembers = 16;

    PCD_GroupRegistry();

    PCD_Error addMember(uint8_t groupId, uint16_t nodeId, uint8_t resourceId);
    bool removeMember(uint8_t groupId, uint16_t nodeId, uint8_t resourceId);

    bool inGroup(uint8_t groupId, uint16_t nodeId, uint8_t resourceId) const;
    uint8_t memberCount(uint8_t groupId) const;
    bool memberAt(uint8_t groupId, uint8_t index, uint16_t &nodeId,
                  uint8_t &resourceId) const;

    void clearGroup(uint8_t groupId);
    void clear();

  private:
    PCD_GroupMember groups_[kMaxGroups][kMaxMembers];
    uint8_t counts_[kMaxGroups];
};

}  // namespace pcd
