#include "routing/route_table.h"

#include <string.h>

#include "config_storage.h"  /* crc16() */

namespace pcd {

void MappingRule::serialize(const MappingRule &rule, uint8_t *out) {
    memset(out, 0, kMappingRuleBytes);
    out[0] = rule.src_protocol;
    writeUint16BE(&out[1], rule.src_id);
    out[3] = rule.src_resource;
    out[4] = rule.src_channel;
    out[5] = rule.dst_protocol;
    writeUint16BE(&out[6], rule.dst_id);
    out[8] = rule.dst_resource;
    out[9] = rule.dst_channel;
    out[10] = rule.dst_action;
    writeUint32BE(&out[11], rule.dst_param);
}

void MappingRule::deserialize(const uint8_t *in, MappingRule &out) {
    out.src_protocol = in[0];
    out.src_id = readUint16BE(&in[1]);
    out.src_resource = in[3];
    out.src_channel = in[4];
    out.dst_protocol = in[5];
    out.dst_id = readUint16BE(&in[6]);
    out.dst_resource = in[8];
    out.dst_channel = in[9];
    out.dst_action = in[10];
    out.dst_param = readUint32BE(&in[11]);
}

RouteTable::RouteTable() : count_(0) {
    for (uint8_t i = 0; i < CAN_MAX_ROUTES; ++i) {
        rules_[i] = MappingRule();
    }
}

bool RouteTable::addRule(const MappingRule &rule) {
    if (count_ >= CAN_MAX_ROUTES) {
        return false;
    }
    rules_[count_++] = rule;
    return true;
}

bool RouteTable::removeRule(uint8_t index) {
    if (index >= count_) {
        return false;
    }
    for (uint8_t i = index; i + 1 < count_; ++i) {
        rules_[i] = rules_[i + 1];
    }
    --count_;
    return true;
}

bool RouteTable::match(const CanonicalFrame &frame, MappingRule &out) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (rules_[i].matches(frame)) {
            out = rules_[i];
            return true;
        }
    }
    return false;
}

bool RouteTable::serialize(uint8_t *out, uint16_t out_len) const {
    const uint16_t total = static_cast<uint16_t>(2 + 1 + 1 + kMappingRuleBytes * count_ + 2);
    if (out == 0 || out_len < total) {
        return false;
    }
    uint8_t header[4];
    header[0] = kMagic;
    header[1] = kVersion;
    header[2] = count_;
    header[3] = 0;
    memcpy(out, header, 4);

    uint16_t crc = crc16(header, 4);
    uint8_t raw[kMappingRuleBytes];
    for (uint8_t i = 0; i < count_; ++i) {
        MappingRule::serialize(rules_[i], raw);
        memcpy(&out[4 + kMappingRuleBytes * i], raw, kMappingRuleBytes);
        crc = crc16(raw, kMappingRuleBytes, crc);
    }
    writeUint16BE(&out[total - 2], crc);
    return true;
}

bool RouteTable::deserialize(const uint8_t *in, uint16_t in_len) {
    if (in == 0 || in_len < 6) {
        return false;
    }
    if (in[0] != kMagic || in[1] != kVersion) {
        return false;
    }
    const uint8_t count = in[2];
    if (count > CAN_MAX_ROUTES) {
        return false;
    }
    const uint16_t total = static_cast<uint16_t>(4 + kMappingRuleBytes * count + 2);
    if (in_len < total) {
        return false;
    }
    if (readUint16BE(&in[total - 2]) != crc16(in, total - 2)) {
        return false;
    }
    count_ = 0;
    for (uint8_t i = 0; i < count; ++i) {
        MappingRule rule;
        MappingRule::deserialize(&in[4 + kMappingRuleBytes * i], rule);
        rules_[count_++] = rule;
    }
    return true;
}

}  // namespace pcd
