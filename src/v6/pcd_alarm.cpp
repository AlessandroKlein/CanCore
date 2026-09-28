#include "v6/pcd_alarm.h"

namespace pcd {

const char *pcdAlarmStateName(PCD_AlarmState state) {
    switch (state) {
        case PCD_ALARM_NORMAL: return "NORMAL";
        case PCD_ALARM_WARNING: return "WARNING";
        case PCD_ALARM_ALARM: return "ALARM";
        case PCD_ALARM_CRITICAL: return "CRITICAL";
        case PCD_ALARM_ACKNOWLEDGED: return "ACKNOWLEDGED";
        case PCD_ALARM_CLEARED: return "CLEARED";
        default: return "UNKNOWN";
    }
}

PCD_AlarmManager::PCD_AlarmManager() : count_(0) {}

PCD_Alarm *PCD_AlarmManager::find(uint8_t id) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (alarms_[i].id == id) {
            return &alarms_[i];
        }
    }
    return 0;
}

const PCD_Alarm *PCD_AlarmManager::find(uint8_t id) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (alarms_[i].id == id) {
            return &alarms_[i];
        }
    }
    return 0;
}

PCD_Error PCD_AlarmManager::addAlarm(uint8_t id, float warning, float alarm, float critical,
                                     float hysteresis, uint32_t delayMs) {
    if (find(id) != 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    if (count_ >= kMaxAlarms) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_Alarm &a = alarms_[count_++];
    a.id = id;
    a.warningThreshold = warning;
    a.alarmThreshold = alarm;
    a.criticalThreshold = critical;
    a.hysteresis = hysteresis;
    a.delayMs = delayMs;
    a.state = PCD_ALARM_NORMAL;
    a.armedAtMs = PCD_ALARM_NOT_ARMED;
    return PCD_OK;
}

PCD_AlarmState PCD_AlarmManager::evaluate(uint8_t id, float value, uint32_t now_ms) {
    PCD_Alarm *a = find(id);
    if (a == 0) {
        return PCD_ALARM_NORMAL;
    }

    /* Nivel crudo segun los umbrales. */
    PCD_AlarmState target = PCD_ALARM_NORMAL;
    if (value >= a->criticalThreshold) {
        target = PCD_ALARM_CRITICAL;
    } else if (value >= a->alarmThreshold) {
        target = PCD_ALARM_ALARM;
    } else if (value >= a->warningThreshold) {
        target = PCD_ALARM_WARNING;
    }

    const bool elevated = (a->state == PCD_ALARM_WARNING || a->state == PCD_ALARM_ALARM ||
                           a->state == PCD_ALARM_CRITICAL || a->state == PCD_ALARM_ACKNOWLEDGED);

    if (target == PCD_ALARM_NORMAL) {
        /* Sin peligro: histeresis para no limpiar antes de tiempo. */
        if (elevated && value >= (a->warningThreshold - a->hysteresis)) {
            target = a->state; /* se mantiene por histeresis */
        } else if (elevated) {
            target = PCD_ALARM_CLEARED;
        } else {
            a->armedAtMs = PCD_ALARM_NOT_ARMED; /* normal estable */
        }
    } else {
        /* Peligro presente: debounce antes de elevar. */
        if (!elevated) {
            if (a->armedAtMs == PCD_ALARM_NOT_ARMED) {
                a->armedAtMs = now_ms;
            }
            if ((now_ms - a->armedAtMs) < a->delayMs) {
                return a->state; /* todavia en debounce */
            }
        }
        /* Escalamiento inmediato cuando ya esta elevado. */
    }

    if (target != a->state) {
        a->state = target;
        a->armedAtMs = now_ms;
    }
    return a->state;
}

PCD_Error PCD_AlarmManager::acknowledge(uint8_t id) {
    PCD_Alarm *a = find(id);
    if (a == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    a->state = PCD_ALARM_ACKNOWLEDGED;
    return PCD_OK;
}

PCD_Error PCD_AlarmManager::clear(uint8_t id) {
    PCD_Alarm *a = find(id);
    if (a == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    a->state = PCD_ALARM_NORMAL;
    a->armedAtMs = PCD_ALARM_NOT_ARMED;
    return PCD_OK;
}

PCD_AlarmState PCD_AlarmManager::state(uint8_t id) const {
    const PCD_Alarm *a = find(id);
    return a == 0 ? PCD_ALARM_NORMAL : a->state;
}

uint8_t PCD_AlarmManager::activeAlarmCount() const {
    uint8_t active = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (alarms_[i].state == PCD_ALARM_WARNING || alarms_[i].state == PCD_ALARM_ALARM ||
            alarms_[i].state == PCD_ALARM_CRITICAL || alarms_[i].state == PCD_ALARM_ACKNOWLEDGED) {
            ++active;
        }
    }
    return active;
}

}  // namespace pcd
