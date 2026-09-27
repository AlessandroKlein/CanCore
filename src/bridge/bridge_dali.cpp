#include "bridge/bridge_dali.h"

#include <string.h>

#include "device_manager.h" /* unpackSetValueTarget / unpackSetValueFade */

namespace pcd {

uint16_t daliDirectArcPower(uint8_t short_address, uint8_t level) {
    const uint8_t address = static_cast<uint8_t>(((short_address & 0x3F) << 1) | 0x01);
    return static_cast<uint16_t>((static_cast<uint16_t>(address) << 8) | (level & 0xFF));
}

uint16_t daliShortCommand(uint8_t short_address, uint8_t command) {
    const uint8_t address = static_cast<uint8_t>((short_address & 0x3F) << 1);
    return static_cast<uint16_t>((static_cast<uint16_t>(address) << 8) | command);
}

uint16_t daliGroupArcPower(uint8_t group, uint8_t level) {
    const uint8_t address = static_cast<uint8_t>(0x80 | ((group & 0x0F) << 1) | 0x01);
    return static_cast<uint16_t>((static_cast<uint16_t>(address) << 8) | (level & 0xFF));
}

uint16_t daliGroupCommand(uint8_t group, uint8_t command) {
    const uint8_t address = static_cast<uint8_t>(0x80 | ((group & 0x0F) << 1));
    return static_cast<uint16_t>((static_cast<uint16_t>(address) << 8) | command);
}

uint16_t daliBroadcastArcPower(uint8_t level) {
    return static_cast<uint16_t>((static_cast<uint16_t>(kDaliBroadcastDapc) << 8) |
                                 (level & 0xFF));
}

uint16_t daliBroadcastCommand(uint8_t command) {
    return static_cast<uint16_t>((static_cast<uint16_t>(kDaliBroadcastCommand) << 8) | command);
}

DaliFrame splitDaliFrame(uint16_t frame) {
    DaliFrame out;
    out.address_byte = static_cast<uint8_t>(frame >> 8);
    out.data_byte = static_cast<uint8_t>(frame & 0xFF);
    return out;
}

bool daliIsDirectArcPower(uint16_t frame) {
    const uint8_t address = static_cast<uint8_t>(frame >> 8);
    if ((address & 0x80) == 0) {
        return (address & 0x01) != 0; /* direccion corta */
    }
    if ((address & 0xC0) == 0x80) {
        return (address & 0x01) != 0; /* direccion de grupo */
    }
    if (address >= 0xFC) {
        return (address & 0x01) != 0; /* broadcast */
    }
    return false; /* comando especial: 101CCCC1 / 110CCCC1 */
}

/*
 * Mapeo porcentaje -> nivel DALI.
 *
 * La curva luminosa real de DALI es logaritmica; aqui el porcentaje se
 * interpreta como posicion de consigna (arc power) para que el mapeo sea
 * determinista e invertible en ambos sentidos sin libreria matematica. El
 * proyecto que quiera curva perceptual puede aplicar su propia tabla antes de
 * llamar al puente (ver wiki 11).
 */
uint8_t daliLevelFromPercent(uint8_t percent) {
    if (percent == 0) {
        return 0;
    }
    if (percent >= 100) {
        return kDaliMaxLevel;
    }
    const uint32_t level = (static_cast<uint32_t>(percent) * kDaliMaxLevel + 50UL) / 100UL;
    return static_cast<uint8_t>(level > kDaliMaxLevel ? kDaliMaxLevel : level);
}

uint8_t daliPercentFromLevel(uint8_t level) {
    if (level == 0) {
        return 0;
    }
    if (level >= kDaliMaxLevel) {
        return 100;
    }
    return static_cast<uint8_t>((static_cast<uint32_t>(level) * 100UL + kDaliMaxLevel / 2) /
                                kDaliMaxLevel);
}

/*
 * Codigo de fade time DALI (0 = sin fundido). La escala normalizada crece en
 * razon raiz de 2 desde 0,7 s: 0,7 / 1 / 1,4 / 2 / 2,8 / 4 / 5,7 / 8 / 11,3 /
 * 16 / 22,6 / 32 / 45 / 64 / 90 segundos. Se calcula con aritmetica entera.
 */
uint8_t daliFadeTimeCode(uint32_t fade_ms) {
    if (fade_ms == 0) {
        return 0;
    }
    uint32_t candidate_ms = 707; /* codigo 1 */
    for (uint8_t code = 1; code < 15; ++code) {
        const uint32_t next_ms = (candidate_ms * 1414UL) / 1000UL;
        if (fade_ms < (candidate_ms + next_ms) / 2UL) {
            return code;
        }
        candidate_ms = next_ms;
    }
    return 15;
}

DaliBridge::DaliBridge(IDaliTransport &transport)
    : transport_(transport),
      track_next_(0),
      pending_count_(0),
      frames_sent_(0),
      backward_received_(0) {
    memset(tracked_, 0, sizeof(tracked_));
    for (uint8_t i = 0; i < 8; ++i) {
        pending_[i] = CanonicalFrame();
    }
}

bool DaliBridge::sendFrame(uint16_t frame) {
    if (transport_.sendFrame == 0) {
        return false;
    }
    if (!transport_.sendFrame(frame, transport_.ctx)) {
        return false;
    }
    ++frames_sent_;
    return true;
}

void DaliBridge::rememberLevel(uint8_t short_address, uint8_t level) {
    for (uint8_t i = 0; i < CAN_DALI_TRACK_SLOTS; ++i) {
        if (tracked_[i].valid && tracked_[i].address == short_address) {
            tracked_[i].level = level;
            return;
        }
    }
    TrackedChannel &slot = tracked_[track_next_];
    slot.address = short_address;
    slot.level = level;
    slot.valid = true;
    track_next_ = static_cast<uint8_t>((track_next_ + 1) % CAN_DALI_TRACK_SLOTS);
}

uint8_t DaliBridge::trackedLevel(uint8_t short_address) const {
    for (uint8_t i = 0; i < CAN_DALI_TRACK_SLOTS; ++i) {
        if (tracked_[i].valid && tracked_[i].address == short_address) {
            return tracked_[i].level;
        }
    }
    return 0;
}

bool DaliBridge::sendLevel(uint8_t channel, uint8_t level) {
    if (channel <= kDaliShortMax) {
        rememberLevel(channel, level);
        return sendFrame(daliDirectArcPower(channel, level));
    }
    if (channel >= kDaliGroupBase && channel <= kDaliGroupMax) {
        return sendFrame(daliGroupArcPower(static_cast<uint8_t>(channel - kDaliGroupBase), level));
    }
    if (channel == kDaliBroadcastChannel) {
        return sendFrame(daliBroadcastArcPower(level));
    }
    return false;
}

bool DaliBridge::process(const CanonicalFrame &frame) {
    if (frame.resource != RES_DALI_LIGHT && frame.resource != RES_LIGHT &&
        frame.resource != RES_DIMMER) {
        return false;
    }
    const uint8_t channel = frame.channel;

    if (frame.action == ACT_SCENE) {
        const uint8_t scene = static_cast<uint8_t>(frame.param & 0x0F);
        const uint8_t command = static_cast<uint8_t>(DALI_CMD_GO_TO_SCENE_0 + scene);
        if (channel <= kDaliShortMax) {
            return sendFrame(daliShortCommand(channel, command));
        }
        if (channel >= kDaliGroupBase && channel <= kDaliGroupMax) {
            return sendFrame(
                daliGroupCommand(static_cast<uint8_t>(channel - kDaliGroupBase), command));
        }
        if (channel == kDaliBroadcastChannel) {
            return sendFrame(daliBroadcastCommand(command));
        }
        return false;
    }

    uint8_t level = 0;
    switch (frame.action) {
        case ACT_OFF:
            level = 0;
            break;
        case ACT_ON: {
            const uint8_t command = DALI_CMD_RECALL_MAX_LEVEL;
            if (channel <= kDaliShortMax) {
                rememberLevel(channel, kDaliMaxLevel);
                return sendFrame(daliShortCommand(channel, command));
            }
            if (channel >= kDaliGroupBase && channel <= kDaliGroupMax) {
                return sendFrame(
                    daliGroupCommand(static_cast<uint8_t>(channel - kDaliGroupBase), command));
            }
            if (channel == kDaliBroadcastChannel) {
                return sendFrame(daliBroadcastCommand(command));
            }
            return false;
        }
        case ACT_TOGGLE: {
            const uint8_t current = trackedLevel(channel);
            level = current > 0 ? 0 : kDaliMaxLevel;
            break;
        }
        case ACT_SET_VALUE: {
            uint8_t target = static_cast<uint8_t>(unpackSetValueTarget(frame.param));
            const uint16_t fade_ms = unpackSetValueFade(frame.param);
            if (target > 100) {
                target = 100;
            }
            level = daliLevelFromPercent(target);
            if (fade_ms > 0 && channel <= kDaliShortMax) {
                /* El fade time DALI se programa por balasto mediante DTR. */
                sendFrame(static_cast<uint16_t>((static_cast<uint16_t>(kDaliSpecialSetDtr)
                                                 << 8) |
                                                daliFadeTimeCode(fade_ms)));
                sendFrame(daliShortCommand(channel, DALI_CMD_STORE_FADE_TIME));
            }
            break;
        }
        default:
            return false;
    }
    return sendLevel(channel, level);
}

bool DaliBridge::queueLevel(uint8_t short_address, uint8_t level, uint16_t source_id,
                            uint8_t resource) {
    if (short_address > kDaliShortMax || pending_count_ >= 8) {
        return false;
    }
    CanonicalFrame &entry = pending_[pending_count_++];
    entry = CanonicalFrame();
    entry.protocol = PROTO_CAN;
    entry.source_id = source_id;
    entry.resource = resource;
    entry.channel = short_address;
    entry.value = static_cast<float>(daliPercentFromLevel(level));
    entry.flags = DIAG_NONE;
    entry.is_command = 0;
    rememberLevel(short_address, level);
    return true;
}

void DaliBridge::onBackwardFrame(uint8_t short_address, uint8_t status) {
    ++backward_received_;
    if (short_address <= kDaliShortMax) {
        rememberLevel(short_address, status);
    }
}

bool DaliBridge::buildCanonical(CanonicalFrame &out) {
    if (pending_count_ == 0) {
        return false;
    }
    out = pending_[0];
    for (uint8_t i = 1; i < pending_count_; ++i) {
        pending_[i - 1] = pending_[i];
    }
    --pending_count_;
    pending_[pending_count_] = CanonicalFrame();
    return true;
}

bool DaliBridge::available() const {
    return pending_count_ > 0;
}

}  // namespace pcd
