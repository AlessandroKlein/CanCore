#include "v6/pcd_datatypes.h"

namespace pcd {

size_t pcdTypeSize(PCD_DataType type) {
    switch (type) {
        case PCD_TYPE_BOOL:
        case PCD_TYPE_UINT8:
        case PCD_TYPE_INT8:
        case PCD_TYPE_ENUM:
        case PCD_TYPE_BITFIELD:
            return 1;
        case PCD_TYPE_UINT16:
        case PCD_TYPE_INT16:
            return 2;
        case PCD_TYPE_UINT32:
        case PCD_TYPE_INT32:
        case PCD_TYPE_FLOAT32:
            return 4;
        case PCD_TYPE_UINT64:
        case PCD_TYPE_INT64:
        case PCD_TYPE_FLOAT64:
            return 8;
        case PCD_TYPE_STRING:
        case PCD_TYPE_BYTES:
            return 0;
        default:
            return 0;
    }
}

bool pcdTypeIsIntegral(PCD_DataType type) {
    return type >= PCD_TYPE_BOOL && type <= PCD_TYPE_BITFIELD;
}

bool pcdTypeIsFloat(PCD_DataType type) {
    return type == PCD_TYPE_FLOAT32 || type == PCD_TYPE_FLOAT64;
}

bool pcdTypeIsFixed(PCD_DataType type) {
    return type != PCD_TYPE_STRING && type != PCD_TYPE_BYTES && type < PCD_TYPE_COUNT;
}

const char *pcdTypeName(PCD_DataType type) {
    switch (type) {
        case PCD_TYPE_BOOL: return "BOOL";
        case PCD_TYPE_UINT8: return "UINT8";
        case PCD_TYPE_INT8: return "INT8";
        case PCD_TYPE_UINT16: return "UINT16";
        case PCD_TYPE_INT16: return "INT16";
        case PCD_TYPE_UINT32: return "UINT32";
        case PCD_TYPE_INT32: return "INT32";
        case PCD_TYPE_UINT64: return "UINT64";
        case PCD_TYPE_INT64: return "INT64";
        case PCD_TYPE_FLOAT32: return "FLOAT32";
        case PCD_TYPE_FLOAT64: return "FLOAT64";
        case PCD_TYPE_ENUM: return "ENUM";
        case PCD_TYPE_BITFIELD: return "BITFIELD";
        case PCD_TYPE_STRING: return "STRING";
        case PCD_TYPE_BYTES: return "BYTES";
        default: return "UNKNOWN";
    }
}

void pcdWriteUint16LE(uint8_t *dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xFFu);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void pcdWriteUint32LE(uint8_t *dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value & 0xFFu);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    dst[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    dst[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

void pcdWriteUint64LE(uint8_t *dst, uint64_t value) {
    for (uint8_t i = 0; i < 8; ++i) {
        dst[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFFu);
    }
}

uint16_t pcdReadUint16LE(const uint8_t *src) {
    return static_cast<uint16_t>(src[0]) | static_cast<uint16_t>(src[1] << 8);
}

uint32_t pcdReadUint32LE(const uint8_t *src) {
    return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}

uint64_t pcdReadUint64LE(const uint8_t *src) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(src[i]) << (i * 8);
    }
    return value;
}

void pcdWriteFloat32LE(uint8_t *dst, float value) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = value;
    pcdWriteUint32LE(dst, conv.u);
}

void pcdWriteFloat64LE(uint8_t *dst, double value) {
    union {
        double d;
        uint64_t u;
    } conv;
    conv.d = value;
    pcdWriteUint64LE(dst, conv.u);
}

float pcdReadFloat32LE(const uint8_t *src) {
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = pcdReadUint32LE(src);
    return conv.f;
}

double pcdReadFloat64LE(const uint8_t *src) {
    union {
        uint64_t u;
        double d;
    } conv;
    conv.u = pcdReadUint64LE(src);
    return conv.d;
}
PCD_Error pcdWriteValue(uint8_t *dst, size_t capacity, PCD_DataType type, const void *value) {
    if (dst == 0 || value == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    const size_t size = pcdTypeSize(type);
    if (size == 0 || capacity < size) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    const uint8_t *v = static_cast<const uint8_t *>(value);
    switch (type) {
        case PCD_TYPE_BOOL:
            dst[0] = v[0] ? 1u : 0u;
            break;
        case PCD_TYPE_UINT8:
        case PCD_TYPE_INT8:
        case PCD_TYPE_ENUM:
        case PCD_TYPE_BITFIELD:
            dst[0] = v[0];
            break;
        case PCD_TYPE_UINT16:
        case PCD_TYPE_INT16:
            pcdWriteUint16LE(dst, pcdReadUint16LE(v));
            break;
        case PCD_TYPE_UINT32:
        case PCD_TYPE_INT32:
            pcdWriteUint32LE(dst, pcdReadUint32LE(v));
            break;
        case PCD_TYPE_UINT64:
        case PCD_TYPE_INT64:
            pcdWriteUint64LE(dst, pcdReadUint64LE(v));
            break;
        case PCD_TYPE_FLOAT32:
            pcdWriteFloat32LE(dst, *reinterpret_cast<const float *>(v));
            break;
        case PCD_TYPE_FLOAT64:
            pcdWriteFloat64LE(dst, *reinterpret_cast<const double *>(v));
            break;
        default:
            return PCD_ERR_INVALID_ARGUMENT;
    }
    return PCD_OK;
}

PCD_Error pcdReadValue(const uint8_t *src, size_t len, PCD_DataType type, void *value) {
    if (src == 0 || value == 0) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    const size_t size = pcdTypeSize(type);
    if (size == 0 || len < size) {
        return PCD_ERR_INVALID_ARGUMENT;
    }
    uint8_t *v = static_cast<uint8_t *>(value);
    switch (type) {
        case PCD_TYPE_BOOL:
            v[0] = src[0] ? 1u : 0u;
            break;
        case PCD_TYPE_UINT8:
        case PCD_TYPE_INT8:
        case PCD_TYPE_ENUM:
        case PCD_TYPE_BITFIELD:
            v[0] = src[0];
            break;
        case PCD_TYPE_UINT16:
        case PCD_TYPE_INT16:
            v[0] = src[0];
            v[1] = src[1];
            break;
        case PCD_TYPE_UINT32:
        case PCD_TYPE_INT32:
            v[0] = src[0];
            v[1] = src[1];
            v[2] = src[2];
            v[3] = src[3];
            break;
        case PCD_TYPE_UINT64:
        case PCD_TYPE_INT64:
            for (uint8_t i = 0; i < 8; ++i) {
                v[i] = src[i];
            }
            break;
        case PCD_TYPE_FLOAT32:
            *reinterpret_cast<float *>(v) = pcdReadFloat32LE(src);
            break;
        case PCD_TYPE_FLOAT64:
            *reinterpret_cast<double *>(v) = pcdReadFloat64LE(src);
            break;
        default:
            return PCD_ERR_INVALID_ARGUMENT;
    }
    return PCD_OK;
}

}  // namespace pcd

