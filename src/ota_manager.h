#pragma once

/*
 * Orquestador OTA por CAN para el lado emisor (gateway).
 *
 * El manager conserva la imagen en memoria del llamador, calcula el CRC
 * global y produce bloques compatibles con makeOtaData(). Tambien construye
 * las tramas de control de la campana (START, META, FINISH, ABORT) segun la
 * especificacion de la wiki 6 y parsea las respuestas del receptor.
 */

#include <stddef.h>
#include <stdint.h>

#include "can_protocol.h"

namespace pcd {

enum OtaState : uint8_t {
    OTA_STATE_IDLE = 0,
    OTA_STATE_STREAMING = 1,
    OTA_STATE_COMPLETE = 2,
    OTA_STATE_ABORTED = 3
};

class OtaManager {
  public:
    static const size_t kBytesPerFrame = kPayloadSize - 1;
    /* El bloque 0xFF esta reservado como marca de control OTA. */
    static const size_t kMaxFrames = kOtaMaxDataBlocks;
    static const size_t kMaxImageBytes = kBytesPerFrame * kMaxFrames;

    OtaManager(uint16_t source, uint8_t target);

    /* Inicia una campana sobre una imagen que permanece propiedad del llamador. */
    bool begin(const uint8_t *image, size_t image_size);

    /* Version de firmware anunciada y familia de hardware de la imagen. */
    void setFirmwareInfo(uint8_t version_major, uint8_t version_minor, uint8_t hardware_id);

    /* Produce el siguiente OTA_DATA. Devuelve false si no hay más bloques. */
    bool nextFrame(CanFrame &out);

    /* Reconstruye un bloque concreto de la imagen (retransmision por ventana). */
    bool frameAt(uint16_t sequence, CanFrame &out) const;

    /* Tramas de control de la campana. */
    CanFrame startFrame() const;
    CanFrame metaFrame(uint8_t window_size = 16) const;
    CanFrame finishFrame() const;
    CanFrame abortFrame(uint8_t status = OTA_STATUS_OK) const;

    /* Parseo de las respuestas del receptor. */
    bool applyReadyAck(const CanFrame &frame, uint8_t &status_out,
                       uint8_t &window_size_out) const;
    bool applyWindowAck(const CanFrame &frame, uint8_t &window_out,
                        uint16_t &missing_mask_out) const;
    bool applyStatus(const CanFrame &frame, uint8_t &status_out,
                     uint16_t &blocks_received_out) const;

    /* Cancela la campana y evita emitir bloques restantes. */
    void abort();

    bool active() const;
    bool complete() const;
    OtaState state() const;
    size_t imageSize() const;
    size_t bytesSent() const;
    uint16_t imageCrc() const;
    uint16_t frameCount() const;
    uint8_t versionMajor() const { return version_major_; }
    uint8_t versionMinor() const { return version_minor_; }
    uint8_t hardwareId() const { return hardware_id_; }

  private:
    uint16_t source_;
    uint8_t target_;
    const uint8_t *image_;
    size_t image_size_;
    size_t offset_;
    uint16_t crc_;
    OtaState state_;
    uint8_t version_major_;
    uint8_t version_minor_;
    uint8_t hardware_id_;
};

}  // namespace pcd
