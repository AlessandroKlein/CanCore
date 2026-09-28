#pragma once

/*
 * PCD v6.1 - Gestor de alarmas.
 *
 * Alarmas por umbral con histeresis y debounce:
 *
 *   Temperature > 35  -> WARNING
 *   Temperature > 40  -> ALARM
 *   Temperature > 45  -> CRITICAL
 *
 * La histeresis evita oscilaciones (no se limpia hasta bajar del umbral menos
 * la banda). El debounce (delayMs) exige que la condicion se mantenga un tiempo
 * antes de disparar, de modo que un pico instantaneo no genere una alarma.
 *
 * Estados: NORMAL, WARNING, ALARM, CRITICAL, ACKNOWLEDGED, CLEARED.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

enum PCD_AlarmState : uint8_t {
    PCD_ALARM_NORMAL = 0,
    PCD_ALARM_WARNING,
    PCD_ALARM_ALARM,
    PCD_ALARM_CRITICAL,
    PCD_ALARM_ACKNOWLEDGED,
    PCD_ALARM_CLEARED
};

const char *pcdAlarmStateName(PCD_AlarmState state);

/* Centinela de "no armado" para PCD_Alarm::armedAtMs (0 es un instante valido). */
static const uint32_t PCD_ALARM_NOT_ARMED = 0xFFFFFFFFUL;

struct PCD_Alarm {
    uint8_t id;
    float warningThreshold;
    float alarmThreshold;
    float criticalThreshold;
    float hysteresis;
    uint32_t delayMs;      /* debounce: condicion sostenida antes de disparar */
    PCD_AlarmState state;
    uint32_t armedAtMs;    /* instante en que la condicion empezo a sostenerse */

    PCD_Alarm()
        : id(0), warningThreshold(0.0f), alarmThreshold(0.0f), criticalThreshold(0.0f),
          hysteresis(0.0f), delayMs(0), state(PCD_ALARM_NORMAL), armedAtMs(PCD_ALARM_NOT_ARMED) {}
};

class PCD_AlarmManager {
  public:
    static const uint8_t kMaxAlarms = 8;

    PCD_AlarmManager();

    /* Registra una alarma con sus tres umbrales, histeresis y debounce. */
    PCD_Error addAlarm(uint8_t id, float warning, float alarm, float critical,
                       float hysteresis, uint32_t delayMs);

    /* Evalua el valor y actualiza el estado. Devuelve el estado resultante. */
    PCD_AlarmState evaluate(uint8_t id, float value, uint32_t now_ms);

    /* Marca la alarma como reconocida por el operador. */
    PCD_Error acknowledge(uint8_t id);

    /* Fuerza el retorno a NORMAL (limpia la alarma). */
    PCD_Error clear(uint8_t id);

    PCD_AlarmState state(uint8_t id) const;
    uint8_t count() const { return count_; }

    /* Cantidad de alarmas fuera de NORMAL/CLEARED. */
    uint8_t activeAlarmCount() const;

  private:
    PCD_Alarm *find(uint8_t id);
    const PCD_Alarm *find(uint8_t id) const;

    PCD_Alarm alarms_[kMaxAlarms];
    uint8_t count_;
};

}  // namespace pcd
