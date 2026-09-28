#pragma once

/*
 * PCD v6.1 - Configuracion remota con commit/rollback.
 *
 * Permite administrar un nodo desde el gateway con una semantica transaccional:
 *
 *   configuracion actual
 *        |
 *        v
 *   CONFIG_BEGIN / SET  -> etapa cambios (STAGING)
 *        |
 *        v
 *   CONFIG_APPLY        -> cambios efectivos (fase TEST, estado APPLIED)
 *        |
 *        v
 *   nodo responde?      -> SI  : CONFIG_COMMIT   (pendiente -> activa)
 *                        -> NO : CONFIG_ROLLBACK (descarta pendiente)
 *
 * Asi un nodo no queda inutilizado por una configuracion incorrecta: hasta que
 * no se confirma (COMMIT) la configuracion activa anterior permanece intacta
 * para poder revertir. La persistencia real queda como un callback externo; esta
 * capa solo gestiona el ciclo de vida y las transiciones.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

/* Claves de configuracion administrables remotamente. */
enum PCD_ConfigKey : uint8_t {
    PCD_CFG_NODE_ID = 1,
    PCD_CFG_NETWORK_ID,
    PCD_CFG_BITRATE,
    PCD_CFG_HEARTBEAT_INTERVAL_MS,
    PCD_CFG_HEARTBEAT_TIMEOUT_MS,
    PCD_CFG_RETRY_COUNT,
    PCD_CFG_WATCHDOG_ENABLED,
    PCD_CFG_SAFE_STATE,        /* 0/1: estado seguro del recurso (rele, pwm) */
    PCD_CFG_DEBUG,
    PCD_CFG_SECURITY_MODE,
    PCD_CFG_COUNT
};

/* Estado del ciclo de configuracion. */
enum PCD_ConfigState : uint8_t {
    PCD_CFG_STATE_IDLE = 0,
    PCD_CFG_STATE_STAGING,     /* cambios acumulandose, sin aplicar      */
    PCD_CFG_STATE_APPLIED,     /* pendiente en fase TEST                 */
    PCD_CFG_STATE_COMMITTED,   /* pendiente confirmada como activa       */
    PCD_CFG_STATE_ROLLED_BACK  /* pendiente descartada                   */
};

/* Comandos de configuracion remota (wire format). */
enum PCD_ConfigCommand : uint8_t {
    PCD_CFG_CMD_BEGIN = 0x01,
    PCD_CFG_CMD_SET = 0x02,
    PCD_CFG_CMD_GET = 0x03,
    PCD_CFG_CMD_COMMIT = 0x04,
    PCD_CFG_CMD_ROLLBACK = 0x05,
    PCD_CFG_CMD_FACTORY_RESET = 0x06,
    PCD_CFG_CMD_APPLY = 0x07
};

class PCD_ConfigManager {
  public:
    static const uint8_t kMaxEntries = 16;

    PCD_ConfigManager();

    /* Vuelve a un estado limpio (sin activa, pendiente ni defaults). */
    void reset();

    /* Registra un valor por defecto de fabrica para una clave. */
    PCD_Error setDefault(PCD_ConfigKey key, uint32_t value);

    /* Acumula un cambio en la configuracion pendiente (inicia STAGING). */
    PCD_Error stageSet(PCD_ConfigKey key, uint32_t value);

    /* Lee la configuracion pendiente si existe; si no, la activa. */
    PCD_Error stageGet(PCD_ConfigKey key, uint32_t &value) const;

    /* Lee solo la configuracion activa (la ultima confirmada). */
    PCD_Error get(PCD_ConfigKey key, uint32_t &value) const;

    /* Marca la pendiente como efectiva (fase TEST). */
    PCD_Error apply();

    /* Confirma la pendiente: pasa a activa y limpia la pendiente. */
    PCD_Error commit();

    /* Descarta la pendiente: la activa anterior permanece intacta. */
    PCD_Error rollback();

    /* Restaura los defaults de fabrica en la activa. */
    PCD_Error factoryReset();

    PCD_ConfigState state() const { return state_; }
    bool hasPending() const { return pending_count_ > 0; }
    uint8_t pendingCount() const { return pending_count_; }
    uint8_t activeCount() const { return active_count_; }

  private:
    struct Entry {
        uint8_t key;
        uint32_t value;
        bool valid;
    };

    Entry *findEntry(Entry *entries, uint8_t count, PCD_ConfigKey key);
    const Entry *findEntry(const Entry *entries, uint8_t count, PCD_ConfigKey key) const;
    PCD_Error setValue(Entry *entries, uint8_t &count, PCD_ConfigKey key, uint32_t value);

    Entry active_[kMaxEntries];
    Entry pending_[kMaxEntries];
    Entry defaults_[kMaxEntries];
    uint8_t active_count_;
    uint8_t pending_count_;
    uint8_t default_count_;
    PCD_ConfigState state_;
};

}  // namespace pcd
