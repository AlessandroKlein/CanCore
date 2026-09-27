#include "routing/knx_group_map.h"

#include <string.h>

namespace pcd {

uint16_t knxGroupAddress(uint8_t main, uint8_t middle, uint8_t sub) {
    const uint16_t main_bits = static_cast<uint16_t>((main & 0x0F) << 11);
    const uint16_t middle_bits = static_cast<uint16_t>((middle & 0x07) << 8);
    return static_cast<uint16_t>(main_bits | middle_bits | sub);
}

bool knxSplitGroupAddress(uint16_t address, uint8_t &main, uint8_t &middle, uint8_t &sub) {
    main = static_cast<uint8_t>((address >> 11) & 0x0F);
    middle = static_cast<uint8_t>((address >> 8) & 0x07);
    sub = static_cast<uint8_t>(address & 0xFF);
    return true;
}

/* Escribe un entero sin signo en decimal dentro del buffer (sin stdio). */
static size_t writeDecimal(char *out, size_t out_len, uint16_t value) {
    char digits[6];
    uint8_t count = 0;
    if (value == 0) {
        digits[count++] = '0';
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = static_cast<char>('0' + (value % 10));
        value = static_cast<uint16_t>(value / 10);
    }
    size_t written = 0;
    while (count > 0 && written + 1 < out_len) {
        out[written++] = digits[--count];
    }
    return written;
}

bool knxFormatGroupAddress(uint16_t address, char *out, size_t out_len) {
    if (out == 0 || out_len < 8) {
        return false;
    }
    uint8_t main = 0;
    uint8_t middle = 0;
    uint8_t sub = 0;
    knxSplitGroupAddress(address, main, middle, sub);

    size_t pos = writeDecimal(out, out_len, main);
    if (pos + 1 < out_len) {
        out[pos++] = '/';
    }
    pos += writeDecimal(&out[pos], out_len - pos, middle);
    if (pos + 1 < out_len) {
        out[pos++] = '/';
    }
    pos += writeDecimal(&out[pos], out_len - pos, sub);
    out[pos] = '\0';
    return true;
}

/* Lee un entero decimal y consume un separador si lo encuentra. */
static bool readDecimal(const char *&text, uint16_t &value) {
    if (text == 0) {
        return false;
    }
    uint32_t acc = 0;
    uint8_t digits = 0;
    while (*text >= '0' && *text <= '9' && digits < 5) {
        acc = acc * 10UL + static_cast<uint32_t>(*text - '0');
        ++text;
        ++digits;
    }
    if (digits == 0) {
        return false;
    }
    value = static_cast<uint16_t>(acc);
    if (*text == '/' || *text == '.' || *text == '-') {
        ++text;
    }
    return true;
}

bool knxParseGroupAddress(const char *text, uint16_t &address_out) {
    uint16_t main = 0;
    uint16_t middle = 0;
    uint16_t sub = 0;
    if (!readDecimal(text, main) || !readDecimal(text, middle) || !readDecimal(text, sub)) {
        return false;
    }
    if (main > 15 || middle > 7 || sub > 255) {
        return false;
    }
    address_out = knxGroupAddress(static_cast<uint8_t>(main), static_cast<uint8_t>(middle),
                                  static_cast<uint8_t>(sub));
    return true;
}

uint8_t knxDptSize(uint8_t dpt) {
    switch (dpt) {
        case KNX_DPT_BOOL:
        case KNX_DPT_SCALING:
        case KNX_DPT_VALUE_1:
        case KNX_DPT_SCENE:
            return 1;
        case KNX_DPT_VALUE_2:
        case KNX_DPT_VALUE_2F:
        case KNX_DPT_FLOAT_2:
            return 2;
        case KNX_DPT_VALUE_4:
        case KNX_DPT_FLOAT_4:
            return 4;
        default:
            return 0;
    }
}

const char *knxDptName(uint8_t dpt) {
    switch (dpt) {
        case KNX_DPT_BOOL: return "1.xxx bool";
        case KNX_DPT_SCALING: return "5.001 escalado";
        case KNX_DPT_VALUE_1: return "6.xxx entero 8";
        case KNX_DPT_VALUE_2: return "7.xxx entero 16";
        case KNX_DPT_VALUE_2F: return "8.xxx entero 16 con signo";
        case KNX_DPT_FLOAT_2: return "9.xxx float 2";
        case KNX_DPT_VALUE_4: return "12.xxx entero 32";
        case KNX_DPT_FLOAT_4: return "14.xxx float 4";
        case KNX_DPT_SCENE: return "17.001 escena";
        default: return "no soportado";
    }
}

/* DPT 9.xxx: 0,01 * mantisa(11 bits) * 2^exponente(4 bits con signo). */
static bool encodeDpt9(float value, uint8_t *out, size_t out_len, size_t &len_out) {
    if (out_len < 2) {
        return false;
    }
    float mantissa_units = value * 100.0f;
    int8_t exponent = 0;
    while ((mantissa_units > 2047.0f || mantissa_units < -2048.0f) && exponent < 15) {
        mantissa_units /= 2.0f;
        ++exponent;
    }
    while ((mantissa_units < 100.0f && mantissa_units > -100.0f) && exponent > -8) {
        mantissa_units *= 2.0f;
        --exponent;
    }
    const float rounded = mantissa_units >= 0.0f ? mantissa_units + 0.5f : mantissa_units - 0.5f;
    int16_t mantissa = static_cast<int16_t>(rounded);
    if (mantissa > 2047) {
        mantissa = 2047;
    }
    if (mantissa < -2048) {
        mantissa = -2048;
    }
    const uint8_t sign = mantissa < 0 ? 0x80 : 0x00;
    const uint16_t magnitude = static_cast<uint16_t>(mantissa < 0 ? -mantissa : mantissa);
    out[0] = static_cast<uint8_t>(sign | ((static_cast<uint8_t>(exponent) & 0x0F) << 3) |
                                  ((magnitude >> 8) & 0x07));
    out[1] = static_cast<uint8_t>(magnitude & 0xFF);
    len_out = 2;
    return true;
}

static bool decodeDpt9(const uint8_t *data, size_t len, float &value_out) {
    if (len < 2) {
        return false;
    }
    const bool negative = (data[0] & 0x80) != 0;
    int8_t exponent = static_cast<int8_t>((data[0] >> 3) & 0x0F);
    if ((exponent & 0x08) != 0) {
        exponent = static_cast<int8_t>(exponent | 0xF0); /* extiende el signo */
    }
    const uint16_t mantissa = static_cast<uint16_t>(((data[0] & 0x07) << 8) | data[1]);
    float value = 0.01f * static_cast<float>(mantissa);
    while (exponent > 0) {
        value *= 2.0f;
        --exponent;
    }
    while (exponent < 0) {
        value /= 2.0f;
        ++exponent;
    }
    value_out = negative ? -value : value;
    return true;
}

bool knxEncodeDpt(uint8_t dpt, float value, uint32_t scene_param, uint8_t *out, size_t out_len,
                  size_t &len_out) {
    if (out == 0) {
        return false;
    }
    switch (dpt) {
        case KNX_DPT_BOOL: {
            if (out_len < 1) return false;
            out[0] = value > 0.5f ? 1 : 0;
            len_out = 1;
            return true;
        }
        case KNX_DPT_SCALING: {
            if (out_len < 1) return false;
            float percent = value;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 100.0f) percent = 100.0f;
            out[0] = static_cast<uint8_t>((percent * 255.0f + 50.0f) / 100.0f);
            len_out = 1;
            return true;
        }
        case KNX_DPT_VALUE_1:
        case KNX_DPT_SCENE: {
            if (out_len < 1) return false;
            if (dpt == KNX_DPT_SCENE) {
                out[0] = static_cast<uint8_t>(scene_param & 0x3F);
            } else {
                int16_t v = static_cast<int16_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
                if (v > 127) v = 127;
                if (v < -128) v = -128;
                out[0] = static_cast<uint8_t>(v & 0xFF);
            }
            len_out = 1;
            return true;
        }
        case KNX_DPT_VALUE_2:
        case KNX_DPT_VALUE_2F: {
            if (out_len < 2) return false;
            int32_t v = static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            out[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
            out[1] = static_cast<uint8_t>(v & 0xFF);
            len_out = 2;
            return true;
        }
        case KNX_DPT_FLOAT_2:
            return encodeDpt9(value, out, out_len, len_out);
        case KNX_DPT_VALUE_4: {
            if (out_len < 4) return false;
            const int32_t v = static_cast<int32_t>(value);
            out[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
            out[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
            out[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
            out[3] = static_cast<uint8_t>(v & 0xFF);
            len_out = 4;
            return true;
        }
        case KNX_DPT_FLOAT_4: {
            if (out_len < 4) return false;
            uint32_t bits = 0;
            memcpy(&bits, &value, sizeof(bits));
            out[0] = static_cast<uint8_t>((bits >> 24) & 0xFF);
            out[1] = static_cast<uint8_t>((bits >> 16) & 0xFF);
            out[2] = static_cast<uint8_t>((bits >> 8) & 0xFF);
            out[3] = static_cast<uint8_t>(bits & 0xFF);
            len_out = 4;
            return true;
        }
        default:
            return false;
    }
}

bool knxDecodeDpt(uint8_t dpt, const uint8_t *data, size_t len, float &value_out,
                  uint32_t &param_out) {
    if (data == 0) {
        return false;
    }
    const uint8_t size = knxDptSize(dpt);
    if (size == 0 || len < size) {
        return false;
    }
    param_out = 0;
    switch (dpt) {
        case KNX_DPT_BOOL:
            value_out = data[0] & 0x01 ? 1.0f : 0.0f;
            return true;
        case KNX_DPT_SCALING:
            value_out = (static_cast<float>(data[0]) * 100.0f) / 255.0f;
            return true;
        case KNX_DPT_VALUE_1:
            value_out = static_cast<float>(static_cast<int8_t>(data[0]));
            return true;
        case KNX_DPT_SCENE:
            param_out = data[0] & 0x3F;
            value_out = static_cast<float>(param_out);
            return true;
        case KNX_DPT_VALUE_2:
            value_out = static_cast<float>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
            return true;
        case KNX_DPT_VALUE_2F:
            value_out = static_cast<float>(static_cast<int16_t>((static_cast<uint16_t>(data[0])
                                                                 << 8) | data[1]));
            return true;
        case KNX_DPT_FLOAT_2:
            return decodeDpt9(data, len, value_out);
        case KNX_DPT_VALUE_4:
            value_out = static_cast<float>(static_cast<int32_t>(
                (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
                (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3])));
            return true;
        case KNX_DPT_FLOAT_4: {
            const uint32_t bits = (static_cast<uint32_t>(data[0]) << 24) |
                                  (static_cast<uint32_t>(data[1]) << 16) |
                                  (static_cast<uint32_t>(data[2]) << 8) |
                                  static_cast<uint32_t>(data[3]);
            memcpy(&value_out, &bits, sizeof(value_out));
            return true;
        }
        default:
            return false;
    }
}

KnxGroupMap::KnxGroupMap() : count_(0) {
    clear();
}

void KnxGroupMap::clear() {
    count_ = 0;
}

bool KnxGroupMap::add(const KnxGroupMapping &mapping) {
    if (mapping.node_id == 0 || !isValidSource(mapping.node_id) ||
        knxDptSize(mapping.dpt) == 0) {
        return false;
    }
    if (findByGroup(mapping.group_address) != 0) {
        return false; /* una direccion de grupo solo puede mapearse una vez */
    }
    if (count_ >= CAN_MAX_KNX_MAPPINGS) {
        return false;
    }
    mappings_[count_++] = mapping;
    return true;
}

bool KnxGroupMap::remove(uint8_t index) {
    if (index >= count_) {
        return false;
    }
    for (uint8_t i = index + 1; i < count_; ++i) {
        mappings_[i - 1] = mappings_[i];
    }
    --count_;
    return true;
}

const KnxGroupMapping *KnxGroupMap::at(uint8_t index) const {
    return index < count_ ? &mappings_[index] : 0;
}

const KnxGroupMapping *KnxGroupMap::findByGroup(uint16_t group_address) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (mappings_[i].group_address == group_address) {
            return &mappings_[i];
        }
    }
    return 0;
}

const KnxGroupMapping *KnxGroupMap::findByTarget(uint16_t node_id, uint8_t resource,
                                                 uint8_t channel) const {
    for (uint8_t i = 0; i < count_; ++i) {
        const KnxGroupMapping &entry = mappings_[i];
        if (entry.node_id == node_id && entry.resource == resource && entry.channel == channel) {
            return &entry;
        }
    }
    return 0;
}

bool KnxGroupMap::toCanonical(uint16_t group_address, const uint8_t *data, size_t len,
                              CanonicalFrame &out) const {
    const KnxGroupMapping *mapping = findByGroup(group_address);
    if (mapping == 0) {
        return false;
    }
    float value = 0.0f;
    uint32_t param = 0;
    if (!knxDecodeDpt(mapping->dpt, data, len, value, param)) {
        return false;
    }
    out = CanonicalFrame();
    out.protocol = PROTO_KNX;
    out.source_id = group_address;
    out.resource = mapping->resource;
    out.channel = mapping->channel;
    out.value = value;
    if (mapping->dpt == KNX_DPT_SCENE) {
        out.is_command = 1;
        out.action = ACT_SCENE;
        out.param = param;
    } else if (mapping->dpt == KNX_DPT_BOOL) {
        out.is_command = 1;
        out.action = value > 0.5f ? ACT_ON : ACT_OFF;
    } else {
        out.is_command = 0;
    }
    return true;
}

bool KnxGroupMap::fromCanonical(const CanonicalFrame &frame, uint16_t &group_address_out,
                                uint8_t *data_out, size_t out_len, size_t &len_out,
                                uint8_t &dpt_out) const {
    const KnxGroupMapping *mapping = findByTarget(frame.source_id, frame.resource, frame.channel);
    if (mapping == 0) {
        return false;
    }
    const float value = frame.is_command ? static_cast<float>(frame.action) : frame.value;
    if (!knxEncodeDpt(mapping->dpt, value, frame.param, data_out, out_len, len_out)) {
        return false;
    }
    group_address_out = mapping->group_address;
    dpt_out = mapping->dpt;
    return true;
}

bool KnxGroupMap::serialize(uint8_t *out, uint16_t out_len) const {
    const uint16_t needed = static_cast<uint16_t>(3 + kKnxMappingBytes * count_);
    if (out == 0 || out_len < needed) {
        return false;
    }
    out[0] = kMagic;
    out[1] = kVersion;
    out[2] = count_;
    uint16_t offset = 3;
    for (uint8_t i = 0; i < count_; ++i) {
        const KnxGroupMapping &entry = mappings_[i];
        writeUint16BE(&out[offset], entry.group_address);
        out[offset + 2] = entry.dpt;
        writeUint16BE(&out[offset + 3], entry.node_id);
        out[offset + 5] = entry.resource;
        out[offset + 6] = entry.channel;
        out[offset + 7] = 0; /* reservado */
        offset = static_cast<uint16_t>(offset + kKnxMappingBytes);
    }
    return true;
}

bool KnxGroupMap::deserialize(const uint8_t *in, uint16_t in_len) {
    if (in == 0 || in_len < 3 || in[0] != kMagic || in[1] != kVersion) {
        return false;
    }
    const uint8_t stored = in[2];
    if (stored > CAN_MAX_KNX_MAPPINGS ||
        in_len < static_cast<uint16_t>(3 + kKnxMappingBytes * stored)) {
        return false;
    }
    count_ = 0;
    uint16_t offset = 3;
    for (uint8_t i = 0; i < stored; ++i) {
        KnxGroupMapping entry;
        entry.group_address = readUint16BE(&in[offset]);
        entry.dpt = in[offset + 2];
        entry.node_id = readUint16BE(&in[offset + 3]);
        entry.resource = in[offset + 5];
        entry.channel = in[offset + 6];
        if (knxDptSize(entry.dpt) != 0) {
            mappings_[count_++] = entry;
        }
        offset = static_cast<uint16_t>(offset + kKnxMappingBytes);
    }
    return true;
}

}  // namespace pcd
