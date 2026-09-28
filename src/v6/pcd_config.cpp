#include "v6/pcd_config.h"

namespace pcd {

PCD_ConfigManager::PCD_ConfigManager()
    : active_count_(0), pending_count_(0), default_count_(0), state_(PCD_CFG_STATE_IDLE) {
    reset();
}

void PCD_ConfigManager::reset() {
    for (uint8_t i = 0; i < kMaxEntries; ++i) {
        active_[i].valid = false;
        pending_[i].valid = false;
        defaults_[i].valid = false;
    }
    active_count_ = 0;
    pending_count_ = 0;
    default_count_ = 0;
    state_ = PCD_CFG_STATE_IDLE;
}

PCD_ConfigManager::Entry *PCD_ConfigManager::findEntry(Entry *entries, uint8_t count,
                                                       PCD_ConfigKey key) {
    for (uint8_t i = 0; i < count; ++i) {
        if (entries[i].valid && entries[i].key == static_cast<uint8_t>(key)) {
            return &entries[i];
        }
    }
    return 0;
}

const PCD_ConfigManager::Entry *PCD_ConfigManager::findEntry(const Entry *entries, uint8_t count,
                                                             PCD_ConfigKey key) const {
    for (uint8_t i = 0; i < count; ++i) {
        if (entries[i].valid && entries[i].key == static_cast<uint8_t>(key)) {
            return &entries[i];
        }
    }
    return 0;
}

PCD_Error PCD_ConfigManager::setValue(Entry *entries, uint8_t &count, PCD_ConfigKey key,
                                      uint32_t value) {
    Entry *entry = findEntry(entries, count, key);
    if (entry != 0) {
        entry->value = value;
        return PCD_OK;
    }
    if (count >= kMaxEntries) {
        return PCD_ERR_NO_MEMORY;
    }
    Entry &slot = entries[count++];
    slot.key = static_cast<uint8_t>(key);
    slot.value = value;
    slot.valid = true;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::setDefault(PCD_ConfigKey key, uint32_t value) {
    return setValue(defaults_, default_count_, key, value);
}

PCD_Error PCD_ConfigManager::stageSet(PCD_ConfigKey key, uint32_t value) {
    if (key <= 0 || key >= PCD_CFG_COUNT) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    const PCD_Error result = setValue(pending_, pending_count_, key, value);
    if (result == PCD_OK && state_ == PCD_CFG_STATE_IDLE) {
        state_ = PCD_CFG_STATE_STAGING;
    }
    return result;
}

PCD_Error PCD_ConfigManager::stageGet(PCD_ConfigKey key, uint32_t &value) const {
    const Entry *entry = findEntry(pending_, pending_count_, key);
    if (entry == 0) {
        entry = findEntry(active_, active_count_, key);
    }
    if (entry == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    value = entry->value;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::get(PCD_ConfigKey key, uint32_t &value) const {
    const Entry *entry = findEntry(active_, active_count_, key);
    if (entry == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    value = entry->value;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::apply() {
    if (pending_count_ == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    state_ = PCD_CFG_STATE_APPLIED;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::commit() {
    if (pending_count_ == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    /* pendiente -> activa */
    for (uint8_t i = 0; i < pending_count_; ++i) {
        setValue(active_, active_count_, static_cast<PCD_ConfigKey>(pending_[i].key),
                 pending_[i].value);
    }
    /* limpia pendiente */
    for (uint8_t i = 0; i < kMaxEntries; ++i) {
        pending_[i].valid = false;
    }
    pending_count_ = 0;
    state_ = PCD_CFG_STATE_COMMITTED;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::rollback() {
    if (pending_count_ == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < kMaxEntries; ++i) {
        pending_[i].valid = false;
    }
    pending_count_ = 0;
    state_ = PCD_CFG_STATE_ROLLED_BACK;
    return PCD_OK;
}

PCD_Error PCD_ConfigManager::factoryReset() {
    /* activa <- defaults */
    active_count_ = 0;
    for (uint8_t i = 0; i < kMaxEntries; ++i) {
        active_[i].valid = false;
    }
    for (uint8_t i = 0; i < default_count_; ++i) {
        setValue(active_, active_count_, static_cast<PCD_ConfigKey>(defaults_[i].key),
                 defaults_[i].value);
    }
    /* limpia pendiente */
    for (uint8_t i = 0; i < kMaxEntries; ++i) {
        pending_[i].valid = false;
    }
    pending_count_ = 0;
    state_ = PCD_CFG_STATE_IDLE;
    return PCD_OK;
}

}  // namespace pcd
