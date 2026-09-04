#include "bridge/bridge_standard.h"

#include <string.h>

namespace pcd {

CanopenBridge::CanopenBridge(ICanopenTransport &transport)
    : transport_(transport), pending_count_(0) {}

bool CanopenBridge::process(const CanonicalFrame &frame) {
    if (transport_.sendPdo == 0) return false;
    const uint16_t cob_id = static_cast<uint16_t>(kCanopenPcdBaseCobId + (frame.source_id & 0x7F));
    uint8_t data[8] = {frame.resource, frame.channel, frame.action, 0, 0, 0, 0, 0};
    writeUint32BE(&data[3], frame.param);
    return transport_.sendPdo(cob_id, data, sizeof(data), transport_.ctx);
}

void CanopenBridge::queueIncoming(uint16_t node_id, uint8_t resource, uint8_t channel,
                                  uint8_t action, uint32_t param) {
    if (pending_count_ >= 8) return;
    CanonicalFrame &frame = pending_[pending_count_++];
    frame.protocol = PROTO_CANOPEN;
    frame.source_id = node_id;
    frame.resource = resource;
    frame.channel = channel;
    frame.action = action;
    frame.param = param;
    frame.is_command = 1;
}

bool CanopenBridge::buildCanonical(CanonicalFrame &out) {
    if (!pending_count_) return false;
    out = pending_[0];
    for (uint8_t i = 1; i < pending_count_; ++i) pending_[i - 1] = pending_[i];
    --pending_count_;
    return true;
}

bool CanopenBridge::available() const { return pending_count_ > 0; }

Nmea2000Bridge::Nmea2000Bridge(INmea2000Transport &transport)
    : transport_(transport), pending_count_(0) {}

bool Nmea2000Bridge::process(const CanonicalFrame &frame) {
    if (transport_.sendPgn == 0) return false;
    uint8_t data[8] = {frame.resource, frame.channel, 0, 0, 0, 0, 0, 0};
    writeFloatBE(&data[2], frame.value);
    writeUint16BE(&data[6], frame.flags);
    return transport_.sendPgn(kPgnPcdState, static_cast<uint8_t>(frame.source_id),
                              data, sizeof(data), transport_.ctx);
}

void Nmea2000Bridge::queueIncoming(uint16_t source_id, uint8_t resource, uint8_t channel,
                                   float value, uint16_t flags) {
    if (pending_count_ >= 8) return;
    CanonicalFrame &frame = pending_[pending_count_++];
    frame.protocol = PROTO_NMEA2000;
    frame.source_id = source_id;
    frame.resource = resource;
    frame.channel = channel;
    frame.value = value;
    frame.flags = flags;
    frame.is_command = 0;
}

bool Nmea2000Bridge::buildCanonical(CanonicalFrame &out) {
    if (!pending_count_) return false;
    out = pending_[0];
    for (uint8_t i = 1; i < pending_count_; ++i) pending_[i - 1] = pending_[i];
    --pending_count_;
    return true;
}

bool Nmea2000Bridge::available() const { return pending_count_ > 0; }

KnxBridge::KnxBridge(IKnxTransport &transport)
    : transport_(transport), pending_count_(0) {}

bool KnxBridge::process(const CanonicalFrame &frame) {
    if (transport_.sendGroupValue == 0) return false;
    const uint8_t address[2] = {frame.resource, frame.channel};
    uint8_t data[4];
    writeFloatBE(data, frame.value);
    return transport_.sendGroupValue(address, data, sizeof(data), 9);
}

void KnxBridge::queueIncoming(const uint8_t *address, const uint8_t *data, size_t len,
                              uint8_t dpt, uint16_t source_id, uint8_t resource,
                              uint8_t channel) {
    (void)address;
    if (pending_count_ >= 8 || data == 0 || len < 4 || dpt != 9) return;
    CanonicalFrame &frame = pending_[pending_count_++];
    frame.protocol = PROTO_KNX;
    frame.source_id = source_id;
    frame.resource = resource;
    frame.channel = channel;
    frame.value = readFloatBE(data);
    frame.is_command = 0;
}

bool KnxBridge::buildCanonical(CanonicalFrame &out) {
    if (!pending_count_) return false;
    out = pending_[0];
    for (uint8_t i = 1; i < pending_count_; ++i) pending_[i - 1] = pending_[i];
    --pending_count_;
    return true;
}

bool KnxBridge::available() const { return pending_count_ > 0; }

}  // namespace pcd
