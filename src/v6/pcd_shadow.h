#pragma once

/*
 * PCD v6.1 - Device Shadow (estado deseado vs estado reportado).
 *
 * Modela la relacion entre lo que un usuario/gateway DESEA y lo que el nodo
 * realmente REPORTA:
 *
 *   estado fisico  <->  estado conocido por el gateway  <->  estado deseado
 *
 *   { node, resource } -> { reported, desired }
 *
 * Cuando desired != reported existe un "comando pendiente". Si el nodo esta
 * offline, el comando queda pendiente y se sincroniza al volver online.
 *
 * v6.1 cubre recursos digitales (bool); los analogicos (float) se agregan
 * despues sin cambiar el concepto.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

struct PCD_ShadowEntry {
    uint16_t nodeId;
    uint8_t resourceId;
    bool reported;
    bool desired;
    bool reportedValid;
    bool desiredValid;

    PCD_ShadowEntry()
        : nodeId(0), resourceId(0), reported(false), desired(false),
          reportedValid(false), desiredValid(false) {}
};

class PCD_DeviceShadow {
  public:
    static const uint8_t kMaxEntries = 32;

    PCD_DeviceShadow();

    /* Fija el estado deseado (desde el usuario/gateway). */
    PCD_Error setDesired(uint16_t nodeId, uint8_t resourceId, bool value);

    /* Registra el estado reportado (desde el nodo). */
    PCD_Error setReported(uint16_t nodeId, uint8_t resourceId, bool value);

    bool getReported(uint16_t nodeId, uint8_t resourceId, bool &value) const;
    bool getDesired(uint16_t nodeId, uint8_t resourceId, bool &value) const;

    /* true si hay un comando pendiente (desired != reported). */
    bool isPending(uint16_t nodeId, uint8_t resourceId) const;

    /* true si el nodo confirmo el estado deseado (sincronizado). */
    bool isSynced(uint16_t nodeId, uint8_t resourceId) const;

    uint8_t pendingCount() const;
    uint8_t count() const { return count_; }

    void clear();

  private:
    PCD_ShadowEntry *find(uint16_t nodeId, uint8_t resourceId);
    const PCD_ShadowEntry *find(uint16_t nodeId, uint8_t resourceId) const;

    PCD_ShadowEntry entries_[kMaxEntries];
    uint8_t count_;
};

}  // namespace pcd
