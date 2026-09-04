#include "can_protocol.h"

#include <string.h>

namespace pcd {

uint32_t encodeId(uint8_t priority, uint8_t msg_type, uint8_t target, uint16_t source) {
    uint32_t id = 0;
    id |= (static_cast<uint32_t>(priority) & kPriorityMask) << kPriorityShift;
    id |= (static_cast<uint32_t>(msg_type) & kMsgTypeMask) << kMsgTypeShift;
    id |= (static_cast<uint32_t>(target) & kTargetMask) << kTargetShift;
    id |= (static_cast<uint32_t>(source) & kSourceMask) << kSourceShift;
    return id & kExtendedIdMask;
}

uint32_t encodeId(const CanId &id) {
    return encodeId(id.priority, id.msg_type, id.target, id.source);
}

CanId decodeId(uint32_t raw_id) {
    CanId id;
    id.priority = static_cast<uint8_t>((raw_id >> kPriorityShift) & kPriorityMask);
    id.msg_type = static_cast<uint8_t>((raw_id >> kMsgTypeShift) & kMsgTypeMask);
    id.target = static_cast<uint8_t>((raw_id >> kTargetShift) & kTargetMask);
    id.source = static_cast<uint16_t>((raw_id >> kSourceShift) & kSourceMask);
    return id;
}

bool isValidTarget(uint8_t target) {
    return target <= kMaxTarget;
}

bool isValidSource(uint16_t source) {
    return source >= 1 && source <= kMaxSource;
}

uint16_t deriveAutomaticNodeId(uint32_t unique_value, uint16_t salt) {
    uint32_t hash = unique_value ^ (static_cast<uint32_t>(salt) << 16);
    hash ^= hash >> 16;
    hash *= 0x45D9F3BUL;
    hash ^= hash >> 16;
    return static_cast<uint16_t>((hash % kMaxSource) + 1);
}

const char *resourceName(uint8_t resource) {
    switch (resource) {
        case RES_RELAY: return "relay";
        case RES_DIMMER: return "dimmer";
        case RES_DIGITAL_INPUT: return "digital_input";
        case RES_ENV_SENSOR: return "environment";
        case RES_GAS_SENSOR: return "gas";
        case RES_POWER_SENSOR: return "power";
        case RES_COVER: return "cover";
        case RES_BUTTON: return "button";
        case RES_BINARY_SENSOR: return "binary_sensor";
        case RES_LIGHT: return "light";
        case RES_FAN: return "fan";
        case RES_LOCK: return "lock";
        case RES_VALVE: return "valve";
        case RES_WATER_LEAK: return "water_leak";
        case RES_SMOKE: return "smoke";
        case RES_PRESSURE_SENSOR: return "pressure";
        case RES_HUMIDITY_SENSOR: return "humidity";
        case RES_CO2_SENSOR: return "co2";
        case RES_AIR_QUALITY: return "air_quality";
        case RES_GPS: return "gps";
        case RES_VIBRATION_SENSOR: return "vibration";
        case RES_VOLTAGE_SENSOR: return "voltage";
        case RES_CURRENT_SENSOR: return "current";
        case RES_FREQUENCY_SENSOR: return "frequency";
        case RES_ENERGY_SENSOR: return "energy";
        case RES_BATTERY: return "battery";
        case RES_CONFIG: return "config";
        case RES_SYSTEM: return "system";
        default: return "custom";
    }
}

void writeUint16BE(uint8_t *dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[1] = static_cast<uint8_t>(value & 0xFF);
}

void writeUint32BE(uint8_t *dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(value & 0xFF);
}

void writeFloatBE(uint8_t *dst, float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    writeUint32BE(dst, bits);
}

uint16_t readUint16BE(const uint8_t *src) {
    return static_cast<uint16_t>((static_cast<uint16_t>(src[0]) << 8) | src[1]);
}

uint32_t readUint32BE(const uint8_t *src) {
    return (static_cast<uint32_t>(src[0]) << 24) | (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8) | static_cast<uint32_t>(src[3]);
}

float readFloatBE(const uint8_t *src) {
    uint32_t bits = readUint32BE(src);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static CanFrame makeFrame(uint8_t priority, uint8_t msg_type, uint16_t source, uint8_t target,
                          uint8_t resource, uint8_t channel) {
    CanFrame frame;
    frame.id = encodeId(priority, msg_type, target, source);
    frame.dlc = kPayloadSize;
    frame.data[0] = resource;
    frame.data[1] = channel;
    return frame;
}

CanFrame makeCommand(uint16_t source, uint8_t target, uint8_t resource, uint8_t channel,
                     uint8_t action, uint32_t param) {
    CanFrame frame = makeFrame(PRIO_REALTIME, MSG_EVENT, source, target, resource, channel);
    frame.data[2] = action;
    writeUint32BE(&frame.data[3], param);
    return frame;
}

CanFrame makeStateBroadcast(uint16_t source, uint8_t resource, uint8_t channel, float value,
                            uint16_t flags) {
    CanFrame frame =
        makeFrame(PRIO_REALTIME, MSG_STATE, source, kBroadcastTarget, resource, channel);
    writeFloatBE(&frame.data[2], value);
    writeUint16BE(&frame.data[6], flags);
    return frame;
}

CanFrame makeTelemetry(uint16_t source, uint8_t resource, uint8_t channel, float value,
                       uint16_t flags) {
    CanFrame frame =
        makeFrame(PRIO_TELEMETRY, MSG_STATE, source, kBroadcastTarget, resource, channel);
    writeFloatBE(&frame.data[2], value);
    writeUint16BE(&frame.data[6], flags);
    return frame;
}

CanFrame makeInputEvent(uint16_t source, uint8_t channel, uint8_t event) {
    CanFrame frame = makeFrame(PRIO_REALTIME, MSG_EVENT, source, kBroadcastTarget,
                               RES_DIGITAL_INPUT, channel);
    frame.data[2] = event;
    return frame;
}

CanFrame makeHeartbeat(uint16_t source, uint32_t uptime_s, uint8_t health) {
    CanFrame frame =
        makeFrame(PRIO_TELEMETRY, MSG_HEARTBEAT, source, kBroadcastTarget, RES_SYSTEM, 0x00);
    frame.data[2] = health;
    writeUint32BE(&frame.data[3], uptime_s);
    return frame;
}

CanFrame makeDiscoveryRequest(uint16_t source, uint8_t target) {
    CanFrame frame = makeFrame(PRIO_CONFIG, MSG_DISCOVERY, source, target, RES_SYSTEM, 0);
    frame.data[2] = DISCOVERY_REQUEST;
    return frame;
}

CanFrame makeDiscoveryAnnounce(uint16_t source, uint8_t id_mode, uint8_t resource_count) {
    CanFrame frame = makeFrame(PRIO_CONFIG, MSG_DISCOVERY, source, kBroadcastTarget,
                               RES_SYSTEM, 0);
    frame.data[2] = DISCOVERY_ANNOUNCE;
    frame.data[3] = id_mode;
    frame.data[4] = resource_count;
    return frame;
}

CanFrame makeDiscoveryResource(uint16_t source, uint8_t resource, uint8_t channel) {
    CanFrame frame = makeFrame(PRIO_CONFIG, MSG_DISCOVERY, source, kBroadcastTarget,
                               resource, channel);
    frame.data[2] = DISCOVERY_RESOURCE;
    return frame;
}

CanFrame makeConfig(uint16_t source, uint8_t target, uint8_t sub_command, const uint8_t *payload,
                    uint8_t payload_len) {
    CanFrame frame = makeFrame(PRIO_CONFIG, MSG_CONFIG, source, target, RES_CONFIG, sub_command);
    const uint8_t capacity = kPayloadSize - 2;
    if (payload != NULL && payload_len > 0) {
        const uint8_t copied = payload_len > capacity ? capacity : payload_len;
        memcpy(&frame.data[2], payload, copied);
    }
    return frame;
}

CanFrame makeOtaData(uint16_t source, uint8_t target, uint8_t sequence, const uint8_t *chunk,
                     uint8_t chunk_len) {
    CanFrame frame;
    frame.id = encodeId(PRIO_BACKGROUND, MSG_OTA, target, source);
    frame.data[0] = sequence;
    const uint8_t capacity = kPayloadSize - 1;
    const uint8_t copied = (chunk == NULL) ? 0 : (chunk_len > capacity ? capacity : chunk_len);
    if (copied > 0) {
        memcpy(&frame.data[1], chunk, copied);
    }
    frame.dlc = static_cast<uint8_t>(1 + copied);
    return frame;
}

float frameValue(const CanFrame &frame) {
    return readFloatBE(&frame.data[2]);
}

uint16_t frameFlags(const CanFrame &frame) {
    return readUint16BE(&frame.data[6]);
}

uint32_t frameParam(const CanFrame &frame) {
    return readUint32BE(&frame.data[3]);
}

}  // namespace pcd
