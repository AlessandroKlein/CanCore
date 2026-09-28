#pragma once

/*
 * PCD v6.2 - Identidad criptografica del dispositivo.
 *
 * Separa el Node-ID (asignable, puede cambiar) de la identidad permanente del
 * dispositivo (UUID + clave). Permite renumerar Node ID 12 -> 35 sin cambiar la
 * identidad criptografica, util en instalaciones grandes.
 */

#include <stdint.h>
#include <string.h>

namespace pcd {

static const uint8_t PCD_DEVICE_UUID_BYTES = 16;
static const uint8_t PCD_DEVICE_KEY_BYTES = 32;

struct PCD_DeviceIdentity {
    uint8_t uuid[PCD_DEVICE_UUID_BYTES];
    uint8_t key[PCD_DEVICE_KEY_BYTES];
    bool hasUuid;
    bool hasKey;

    PCD_DeviceIdentity() : hasUuid(false), hasKey(false) {
        memset(uuid, 0, sizeof(uuid));
        memset(key, 0, sizeof(key));
    }

    void setUuid(const uint8_t *bytes) {
        memcpy(uuid, bytes, PCD_DEVICE_UUID_BYTES);
        hasUuid = true;
    }

    void setKey(const uint8_t *bytes) {
        memcpy(key, bytes, PCD_DEVICE_KEY_BYTES);
        hasKey = true;
    }

    void clear() {
        memset(uuid, 0, sizeof(uuid));
        memset(key, 0, sizeof(key));
        hasUuid = false;
        hasKey = false;
    }
};

/* Compara dos identidades (UUID y clave). */
inline bool pcdIdentityEquals(const PCD_DeviceIdentity &a, const PCD_DeviceIdentity &b) {
    if (a.hasUuid != b.hasUuid || a.hasKey != b.hasKey) {
        return false;
    }
    if (a.hasUuid && memcmp(a.uuid, b.uuid, PCD_DEVICE_UUID_BYTES) != 0) {
        return false;
    }
    if (a.hasKey && memcmp(a.key, b.key, PCD_DEVICE_KEY_BYTES) != 0) {
        return false;
    }
    return true;
}

}  // namespace pcd
