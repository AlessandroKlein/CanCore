#pragma once

/*
 * PCD v6.4 - Scheduler distribuido.
 *
 * Programa tareas por intervalo, por hora del dia o tras un retardo. Las reglas
 * pueden ejecutarse LOCalmente (nodo) o en el GATEWAY, de modo que el sistema
 * siga funcionando aunque el servidor central se caiga.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

enum PCD_ScheduleType : uint8_t {
    PCD_SCHED_INTERVAL = 0,   /* cada N ms                    */
    PCD_SCHED_AT_TIME,        /* a las HH:MM:SS (seg del dia) */
    PCD_SCHED_AFTER_DELAY     /* una vez, tras N ms           */
};

struct PCD_ScheduleEntry {
    uint8_t id;
    PCD_ScheduleType type;
    uint32_t param;      /* intervalo ms / seg-del-dia / retardo ms */
    uint32_t nextRunMs;
    bool enabled;
    bool valid;
};

class PCD_Scheduler {
  public:
    static const uint8_t kMaxEntries = 16;

    PCD_Scheduler();

    PCD_Error addInterval(uint8_t id, uint32_t intervalMs, uint32_t nowMs);
    PCD_Error addAtTime(uint8_t id, uint32_t secondsOfDay, uint32_t nowMs);
    PCD_Error addAfterDelay(uint8_t id, uint32_t delayMs, uint32_t nowMs);

    /* Procesa las entradas vencidas. Devuelve true y el id si disparo una. */
    bool tick(uint32_t nowMs, uint8_t &firedId);

    void enable(uint8_t id, bool enabled);
    void clear();

  private:
    PCD_ScheduleEntry *find(uint8_t id);

    PCD_ScheduleEntry entries_[kMaxEntries];
    uint8_t count_;
};

}  // namespace pcd
