#pragma once

/*
 * Orquestador OTA por CAN para el lado emisor (gateway).
 *
 * El manager conserva la imagen en memoria del llamador, calcula el CRC
 * global y produce bloques compatibles con makeOtaData(). La confirmacion
 * OTA_START/OTA_WINDOW_ACK/OTA_FINISH sigue siendo responsabilidad del
 * protocolo de control del nodo receptor.
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
    static const size_t kMaxFrames = 256;
    static const size_t kMaxImageBytes = kBytesPerFrame * kMaxFrames;

    OtaManager(uint16_t source, uint8_t target);

    /* Inicia una campaña sobre una imagen que permanece propiedad del llamador. */
    bool begin(const uint8_t *image, size_t image_size);

    /* Produce el siguiente OTA_DATA. Devuelve false si no hay más bloques. */
    bool nextFrame(CanFrame &out);

    /* Cancela la campaña y evita emitir bloques restantes. */
    void abort();

    bool active() const;
    bool complete() const;
    OtaState state() const;
    size_t imageSize() const;
    size_t bytesSent() const;
    uint16_t imageCrc() const;
    uint16_t frameCount() const;

  private:
    uint16_t source_;
    uint8_t target_;
    const uint8_t *image_;
    size_t image_size_;
    size_t offset_;
    uint16_t crc_;
    OtaState state_;
};

}  // namespace pcd
