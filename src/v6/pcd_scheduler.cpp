#include "v6/pcd_scheduler.h"

namespace pcd {

PCD_Scheduler::PCD_Scheduler() : count_(0) {}

PCD_ScheduleEntry *PCD_Scheduler::find(uint8_t id) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (entries_[i].valid && entries_[i].id == id) {
            return &entries_[i];
        }
    }
    return 0;
}

PCD_Error PCD_Scheduler::addInterval(uint8_t id, uint32_t intervalMs, uint32_t nowMs) {
    if (count_ >= kMaxEntries) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_ScheduleEntry &e = entries_[count_++];
    e.id = id;
    e.type = PCD_SCHED_INTERVAL;
    e.param = intervalMs;
    e.nextRunMs = nowMs + intervalMs;
    e.enabled = true;
    e.valid = true;
    return PCD_OK;
}

PCD_Error PCD_Scheduler::addAtTime(uint8_t id, uint32_t secondsOfDay, uint32_t nowMs) {
    if (count_ >= kMaxEntries) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_ScheduleEntry &e = entries_[count_++];
    e.id = id;
    e.type = PCD_SCHED_AT_TIME;
    e.param = secondsOfDay;
    /* La proxima ejecucion es el primer instante del dia en que ya paso `nowMs`. */
    const uint32_t dayMs = 86400000u;
    uint32_t targetMs = secondsOfDay * 1000u;
    if (targetMs <= nowMs) {
        targetMs += dayMs;
    }
    e.nextRunMs = targetMs;
    e.enabled = true;
    e.valid = true;
    return PCD_OK;
}

PCD_Error PCD_Scheduler::addAfterDelay(uint8_t id, uint32_t delayMs, uint32_t nowMs) {
    if (count_ >= kMaxEntries) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_ScheduleEntry &e = entries_[count_++];
    e.id = id;
    e.type = PCD_SCHED_AFTER_DELAY;
    e.param = delayMs;
    e.nextRunMs = nowMs + delayMs;
    e.enabled = true;
    e.valid = true;
    return PCD_OK;
}

bool PCD_Scheduler::tick(uint32_t nowMs, uint8_t &firedId) {
    for (uint8_t i = 0; i < count_; ++i) {
        PCD_ScheduleEntry &e = entries_[i];
        if (!e.valid || !e.enabled || nowMs < e.nextRunMs) {
            continue;
        }
        firedId = e.id;
        if (e.type == PCD_SCHED_INTERVAL) {
            e.nextRunMs = nowMs + e.param; /* re-agenda */
        } else {
            e.valid = false; /* at-time y after-delay se disparan una vez */
        }
        return true;
    }
    return false;
}

void PCD_Scheduler::enable(uint8_t id, bool enabled) {
    PCD_ScheduleEntry *e = find(id);
    if (e != 0) {
        e->enabled = enabled;
    }
}

void PCD_Scheduler::clear() {
    count_ = 0;
}

}  // namespace pcd
