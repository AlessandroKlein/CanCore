#include "bridge/bridge_modbus.h"

namespace pcd {

ModbusBridge::ModbusBridge(IModbusRegisterMap &map, uint16_t base_address)
    : map_(map), base_address_(base_address), pending_count_(0) {}

void ModbusBridge::queueIncoming(uint16_t node_id, uint8_t resource, uint8_t channel,
                                uint8_t action, uint32_t param) {
    if (pending_count_ >= 8) {
        return;
    }
    CanonicalFrame frame;
    frame.protocol = PROTO_MODBUS;
    frame.source_id = node_id;
    frame.resource = resource;
    frame.channel = channel;
    frame.action = action;
    frame.param = param;
    frame.is_command = 1;
    pending_[pending_count_++] = frame;
}

bool ModbusBridge::process(const CanonicalFrame &frame) {
    if (map_.writeCoil == 0 && map_.writeRegister == 0) {
        return false;
    }

    if (frame.resource == RES_RELAY || frame.resource == RES_DIGITAL_INPUT) {
        const uint16_t coil_addr = coilForResource(frame.resource, frame.channel);
        if (map_.writeCoil != 0) {
            bool value = false;
            switch (frame.action) {
                case ACT_ON:
                    value = true;
                    break;
                case ACT_OFF:
                    value = false;
                    break;
                case ACT_TOGGLE:
                    value = true;
                    break;
                case ACT_SET_VALUE:
                    value = (frame.param != 0);
                    break;
                default:
                    return false;
            }
            return map_.writeCoil(coil_addr, value, map_.ctx);
        }
    }

    if (frame.resource == RES_DIMMER || frame.resource == RES_COVER || frame.resource == RES_ENV_SENSOR ||
        frame.resource == RES_GAS_SENSOR || frame.resource == RES_POWER_SENSOR) {
        const uint16_t reg_addr = registerForResource(frame.resource, frame.channel);
        if (map_.writeRegister != 0) {
            uint16_t value = static_cast<uint16_t>(frame.param & 0xFFFFU);
            if (frame.action == ACT_ON || frame.action == ACT_OFF || frame.action == ACT_TOGGLE) {
                value = (frame.action == ACT_ON || frame.action == ACT_TOGGLE) ? 1u : 0u;
            }
            return map_.writeRegister(base_address_ + reg_addr, value, map_.ctx);
        }
    }

    return false;
}

bool ModbusBridge::buildCanonical(CanonicalFrame &out) {
    if (pending_count_ == 0) {
        return false;
    }
    out = pending_[0];
    for (uint8_t i = 0; i + 1 < pending_count_; ++i) {
        pending_[i] = pending_[i + 1];
    }
    --pending_count_;
    return true;
}

bool ModbusBridge::available() const {
    return pending_count_ > 0;
}

uint16_t ModbusBridge::registerForResource(uint8_t resource, uint8_t channel) {
    (void)resource;
    uint16_t index = (channel > 0) ? static_cast<uint16_t>(channel - 1u) : 0u;
    return index * 10u;
}

uint16_t ModbusBridge::coilForResource(uint8_t resource, uint8_t channel) {
    (void)resource;
    return (channel > 0) ? static_cast<uint16_t>(channel - 1u) : 0u;
}

}  // namespace pcd
