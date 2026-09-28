#pragma once

/*
 * PCD v6.3 - Store-and-forward.
 *
 * Si un nodo destino esta offline, el gateway puede retener temporalmente
 * ciertos comandos y entregarlos cuando el nodo vuelve online. Es configurable
 * por tipo de mensaje: no conviene almacenar eventos de alta frecuencia.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

struct PCD_PendingMessage {
    uint16_t targetNodeId;
    uint8_t resourceId;
    uint8_t action;
    uint32_t param;
    bool valid;
};

class PCD_StoreAndForward {
  public:
    static const uint8_t kMaxPending = 16;

    PCD_StoreAndForward();

    /* Retiene un comando para un nodo offline. */
    PCD_Error store(uint16_t nodeId, uint8_t resourceId, uint8_t action, uint32_t param);

    /* Comandos pendientes para un nodo. */
    uint8_t pendingFor(uint16_t nodeId) const;

    /* Extrae (y elimina) el siguiente comando pendiente de un nodo. */
    bool popFor(uint16_t nodeId, PCD_PendingMessage &out);

    void clear();

  private:
    PCD_PendingMessage pending_[kMaxPending];
    uint8_t count_;
};

}  // namespace pcd
