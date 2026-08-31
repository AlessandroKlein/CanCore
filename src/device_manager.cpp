#include "device_manager.h"

namespace pcd {

uint32_t packSetValue(uint16_t value, uint16_t fade_ms) {
    return (static_cast<uint32_t>(value) << 16) | fade_ms;
}

uint16_t unpackSetValueTarget(uint32_t param) {
    return static_cast<uint16_t>((param >> 16) & 0xFFFF);
}

uint16_t unpackSetValueFade(uint32_t param) {
    return static_cast<uint16_t>(param & 0xFFFF);
}

DeviceManager::DeviceManager() : count_(0), emitter_(0), emitter_ctx_(0) {}

bool DeviceManager::registerChannel(uint8_t resource, uint8_t channel, ResourceHandler handler,
                                    void *ctx) {
    if (count_ >= CAN_MAX_CHANNELS || handler == 0) {
        return false;
    }
    Channel &entry = channels_[count_++];
    entry.resource = resource;
    entry.channel = channel;
    entry.handler = handler;
    entry.ctx = ctx;
    entry.value = 0.0f;
    entry.start_value = 0.0f;
    entry.target_value = 0.0f;
    entry.flags = DIAG_NONE;
    entry.phase = PHASE_IDLE;
    entry.phase_started = false;
    entry.start_ms = 0;
    entry.duration_ms = 0;
    return true;
}

Channel *DeviceManager::find(uint8_t resource, uint8_t channel) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (channels_[i].resource == resource &&
            (channels_[i].channel == channel || channels_[i].channel == kAnyChannel)) {
            return &channels_[i];
        }
    }
    return 0;
}

const Channel *DeviceManager::find(uint8_t resource, uint8_t channel) const {
    return const_cast<DeviceManager *>(this)->find(resource, channel);
}

void DeviceManager::setStateEmitter(StateEmitter emitter, void *ctx) {
    emitter_ = emitter;
    emitter_ctx_ = ctx;
}

bool DeviceManager::drive(Channel &entry, uint8_t channel, uint8_t action, uint32_t param,
                          float &out_value) {
    float value = entry.value;
    if (!entry.handler(channel, action, param, value, entry.ctx)) {
        return false;
    }
    entry.value = value;
    out_value = value;
    return true;
}

void DeviceManager::cancelPending(uint8_t resource, uint8_t channel) {
    Channel *entry = find(resource, channel);
    if (entry != 0) {
        entry->phase = PHASE_IDLE;
        entry->phase_started = false;
    }
}

bool DeviceManager::apply(uint8_t resource, uint8_t channel, uint8_t action, uint32_t param,
                          float &out_value) {
    Channel *entry = find(resource, channel);
    if (entry == 0) {
        return false;
    }
    entry->phase = PHASE_IDLE;
    entry->phase_started = false;

    if (action == ACT_SET_VALUE) {
        const uint16_t fade_ms = unpackSetValueFade(param);
        if (fade_ms == 0) {
            return drive(*entry, channel, ACT_SET_VALUE, param, out_value);
        }
        /* Rampa no bloqueante: el valor converge dentro de update(). */
        entry->start_value = entry->value;
        entry->target_value = static_cast<float>(unpackSetValueTarget(param));
        entry->duration_ms = fade_ms;
        entry->phase = PHASE_RAMPING;
        out_value = entry->value;
        return true;
    }

    if (!drive(*entry, channel, action, param, out_value)) {
        return false;
    }

    /* param actua como temporizador de apagado automatico en ON / TOGGLE. */
    if (param > 0 && out_value != 0.0f && (action == ACT_ON || action == ACT_TOGGLE)) {
        entry->phase = PHASE_TIMED;
        entry->duration_ms = param;
    }
    return true;
}

void DeviceManager::update(uint32_t now_ms) {
    for (uint8_t i = 0; i < count_; ++i) {
        Channel &entry = channels_[i];
        if (entry.phase == PHASE_IDLE) {
            continue;
        }
        if (!entry.phase_started) {
            entry.phase_started = true;
            entry.start_ms = now_ms;
            continue;
        }

        const uint32_t elapsed = now_ms - entry.start_ms;

        if (entry.phase == PHASE_RAMPING) {
            const bool finished = elapsed >= entry.duration_ms;
            const float ratio =
                finished ? 1.0f
                         : static_cast<float>(elapsed) / static_cast<float>(entry.duration_ms);
            const float value =
                entry.start_value + (entry.target_value - entry.start_value) * ratio;

            float applied = value;
            const uint32_t param = packSetValue(static_cast<uint16_t>(value), 0);
            if (!drive(entry, entry.channel, ACT_SET_VALUE, param, applied)) {
                entry.phase = PHASE_IDLE;
                continue;
            }
            if (finished) {
                entry.phase = PHASE_IDLE;
                if (emitter_ != 0) {
                    emitter_(entry.resource, entry.channel, applied, entry.flags, emitter_ctx_);
                }
            }
            continue;
        }

        if (entry.phase == PHASE_TIMED && elapsed >= entry.duration_ms) {
            float applied = entry.value;
            entry.phase = PHASE_IDLE;
            if (drive(entry, entry.channel, ACT_OFF, 0, applied) && emitter_ != 0) {
                emitter_(entry.resource, entry.channel, applied, entry.flags, emitter_ctx_);
            }
        }
    }
}

void DeviceManager::setFlags(uint8_t resource, uint8_t channel, uint16_t flags) {
    Channel *entry = find(resource, channel);
    if (entry != 0) {
        entry->flags = flags;
    }
}

uint16_t DeviceManager::flags(uint8_t resource, uint8_t channel) const {
    const Channel *entry = find(resource, channel);
    return (entry == 0) ? static_cast<uint16_t>(DIAG_NONE) : entry->flags;
}

bool DeviceManager::value(uint8_t resource, uint8_t channel, float &out_value) const {
    const Channel *entry = find(resource, channel);
    if (entry == 0) {
        return false;
    }
    out_value = entry->value;
    return true;
}

}  // namespace pcd
