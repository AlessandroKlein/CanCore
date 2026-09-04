#pragma once

#include <stdint.h>

#include "can_protocol.h"

namespace pcd {

#ifndef CAN_MAX_DISCOVERED_NODES
#define CAN_MAX_DISCOVERED_NODES 16
#endif

#ifndef CAN_MAX_DISCOVERED_RESOURCES
#define CAN_MAX_DISCOVERED_RESOURCES 16
#endif

struct DiscoveredResource {
    uint8_t resource;
    uint8_t channel;
};

struct DiscoveredNode {
    uint16_t node_id;
    NodeIdMode id_mode;
    uint32_t uptime_s;
    uint8_t health;
    uint8_t resource_count;
    uint32_t seen_count;
    DiscoveredResource resources[CAN_MAX_DISCOVERED_RESOURCES];
};

class NodeRegistry {
  public:
    NodeRegistry();

    /* Actualiza inventario con heartbeat, announce o resource discovery. */
    bool observe(const CanFrame &frame);
    void clear();

    uint8_t count() const { return count_; }
    const DiscoveredNode *at(uint8_t index) const;
    const DiscoveredNode *find(uint16_t node_id) const;

  private:
    DiscoveredNode *ensure(uint16_t node_id);
    bool addResource(DiscoveredNode &node, uint8_t resource, uint8_t channel);

    DiscoveredNode nodes_[CAN_MAX_DISCOVERED_NODES];
    uint8_t count_;
};

}  // namespace pcd
