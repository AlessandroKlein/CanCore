#pragma once

/*
 * PCD v6 - Manifiesto de firmware.
 *
 * Describe una imagen de firmware antes de iniciar una actualizacion OTA o de
 * bootloader. Permite verificar compatibilidad (deviceType + hardwareRevision)
 * e integridad (imageSize + imageCRC32) sin transferir la imagen completa.
 *
 * La transferencia se describe en pcd_firmware_transport (bloques con secuencia,
 * longitud y CRC). El bootloader fisico de AVR y el OTA de ESP32 se marcan como
 * HARDWARE_VALIDATION_REQUIRED.
 */

#include <stdint.h>

namespace pcd {

struct PCD_FirmwareManifest {
    uint16_t deviceType;       /* tipo de dispositivo destino              */
    uint16_t hardwareRevision; /* revision de hardware destino             */
    uint16_t protocolVersion;  /* version de protocolo requerida           */
    uint32_t firmwareVersion;  /* (mayor << 24) | (minor << 16) | (patch << 8) | build */
    uint32_t imageSize;        /* bytes de la imagen                       */
    uint32_t imageCRC32;       /* CRC-32 de la imagen completa             */

    PCD_FirmwareManifest()
        : deviceType(0), hardwareRevision(0), protocolVersion(0), firmwareVersion(0),
          imageSize(0), imageCRC32(0) {}
};

/* Verifica si un manifiesto es compatible con el dispositivo destino. */
inline bool pcdManifestCompatible(const PCD_FirmwareManifest &m, uint16_t deviceType,
                                  uint16_t hardwareRevision) {
    return m.deviceType == deviceType && m.hardwareRevision == hardwareRevision;
}

/* Comandos de transferencia de firmware (transporte comun ESP32/AVR). */
enum PCD_FirmwareCommand : uint8_t {
    PCD_FW_BEGIN = 0x01,
    PCD_FW_DATA = 0x02,
    PCD_FW_VERIFY = 0x03,
    PCD_FW_COMMIT = 0x04,
    PCD_FW_ABORT = 0x05
};

}  // namespace pcd
