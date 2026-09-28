#pragma once

/*
 * PCD v6.3 - Descubrimiento de topologia de red.
 *
 * El gateway construye el arbol de segmentos -> nodos y su estado
 * (ONLINE/OFFLINE/WARNING) a partir de los anuncios de los nodos. Cada nodo
 * declara su networkId; el discovery agrupa por red sin requerir un servidor
 * externo.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

enum PCD_NodeState : uint8_t {
    PCD_NODE_ONLINE = 0,
    PCD_NODE_OFFLINE,
    PCD_NODE_WARNING
};

struct PCD_TopologyNode {
    uint16_t nodeId;
    uint8_t networkId;
    PCD_NodeState state;
    uint32_t lastSeenMs;
    bool valid;
};

class PCD_TopologyDiscovery {
  public:
    static const uint8_t kMaxNodes = 32;

    PCD_TopologyDiscovery();

    /* Registra/actualiza un nodo como online. */
    PCD_Error update(uint16_t nodeId, uint8_t networkId, uint32_t nowMs);

    /* Marca offline a los nodos sin anuncio reciente. Devuelve cuantos cambiaron. */
    uint8_t poll(uint32_t nowMs, uint32_t timeoutMs);

    uint8_t nodeCount() const { return count_; }
    uint8_t onlineCount() const;
    uint8_t offlineCount() const;
    uint8_t networkCount() const; /* redes distintas vistas */

    PCD_NodeState state(uint16_t nodeId) const;
    const PCD_TopologyNode *at(uint8_t index) const;

  private:
    PCD_TopologyNode *find(uint16_t nodeId);
    const PCD_TopologyNode *find(uint16_t nodeId) const;

    PCD_TopologyNode nodes_[kMaxNodes];
    uint8_t count_;
};

}  // namespace pcd
