#pragma once

/*
 * Receptor OTA por CAN (lado nodo / bootloader).
 *
 * Implementa la maquina de estados de la wiki 6 sin conocer la plataforma: la
 * escritura real de la flash la aporta la aplicacion mediante IOtaTarget, de
 * modo que el mismo codigo sirve para:
 *
 *   - un bootloader AVR (seccion NRWW + escritura por paginas),
 *   - la particion OTA de un ESP32 (esp_ota_write/esp_ota_end),
 *   - un archivo en LittleFS o un arreglo en RAM para pruebas nativas.
 *
 * El receptor acumula una ventana de bloques en RAM (CAN_OTA_WINDOW_SLOTS
 * bloques de 7 bytes), la escribe en orden estricto cuando esta completa y
 * responde OTA_WINDOW_ACK con el mapa de bits faltante para que el emisor
 * retransmita solo lo perdido. La imagen solo se entrega a flash si el CRC-16
 * global coincide con el anunciado en OTA_META.
 */

#include <stddef.h>
#include <stdint.h>

#include "can_protocol.h"

namespace pcd {

/* Bloques por ventana que el receptor puede retener en RAM (7 bytes cada uno). */
#ifndef CAN_OTA_WINDOW_SLOTS
#define CAN_OTA_WINDOW_SLOTS 16
#endif

/* Timeouts consecutivos sin datos antes de abortar la campana. */
#ifndef CAN_OTA_MAX_TIMEOUTS
#define CAN_OTA_MAX_TIMEOUTS 3
#endif

enum OtaReceiverState : uint8_t {
    OTA_RX_IDLE = 0,     /* sin campana activa                            */
    OTA_RX_VALIDATING,   /* se recibio OTA_START, falta OTA_META          */
    OTA_RX_RECEIVING,    /* descargando bloques por ventanas              */
    OTA_RX_VERIFYING,    /* CRC correcto; se evalua el resultado del target */
    OTA_RX_DONE,         /* imagen aceptada y confirmada al emisor        */
    OTA_RX_FAILED        /* campana abortada (hw, CRC, flash o timeout)   */
};

/*
 * Destino de la imagen. La aplicacion lo implementa sobre su medio:
 *
 *   bool begin(hw_id, image_size)   prepara el medio (borra paginas/particion)
 *   bool writeBlock(seq, data, len) escribe el bloque `seq` en orden creciente
 *   bool finish()                   confirma la imagen (esp_ota_end, etc.)
 *   void abort()                    descarta lo escrito y libera recursos
 */
class IOtaTarget {
  public:
    virtual ~IOtaTarget() {}
    virtual bool begin(uint8_t hardware_id, size_t image_size) = 0;
    virtual bool writeBlock(uint16_t sequence, const uint8_t *data, uint8_t len) = 0;
    virtual bool finish() = 0;
    virtual void abort() = 0;
};

class OtaReceiver {
  public:
    OtaReceiver(uint16_t node_id, uint8_t hardware_id, IOtaTarget *target = 0);

    /* Instala o reemplaza el medio de escritura. */
    void setTarget(IOtaTarget *target) { target_ = target; }

    /* Timeout de inactividad por ventana (por defecto 2000 ms). */
    void setTimeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

    /* Cancela la campana en curso y vuelve a IDLE. */
    void reset();

    /*
     * Procesa una trama MSG_OTA. Devuelve true cuando la trama pertenecia a
     * esta campana. `needs_response` indica que el llamador debe transmitir
     * `response` de vuelta al gateway.
     */
    bool handleFrame(const CanFrame &frame, uint32_t now_ms, CanFrame &response,
                     bool &needs_response);

    /* Vigila el timeout de la ventana. Devuelve true si genero una respuesta. */
    bool poll(uint32_t now_ms, CanFrame &response);

    OtaReceiverState state() const { return state_; }
    uint16_t blocksReceived() const { return blocks_received_; }
    uint16_t totalBlocks() const { return total_blocks_; }
    uint16_t lastWindow() const { return window_index_; }
    uint16_t missingMask() const;
    uint8_t lastStatus() const { return last_status_; }
    uint8_t windowSize() const { return window_size_; }
    uint8_t timeouts() const { return timeouts_; }
    bool imageVerified() const { return image_verified_; }
    size_t imageSize() const { return image_size_; }
    size_t bytesWritten() const { return bytes_written_; }
    uint16_t expectedCrc() const { return expected_crc_; }
    uint16_t computedCrc() const { return computed_crc_; }
    uint8_t versionMajor() const { return version_major_; }
    uint8_t versionMinor() const { return version_minor_; }
    uint8_t hardwareId() const { return hardware_id_; }
    const char *stateName() const;

  private:
    uint8_t address() const { return nodeIdAddress(node_id_); }
    uint16_t fullWindowMask() const;
    uint16_t expectedSlotCount() const;
    bool handleControl(const CanFrame &frame, uint8_t command, const uint8_t *payload,
                       uint32_t now_ms, CanFrame &response, bool &needs_response);
    bool handleFinishOrAbort(uint8_t command, const uint8_t *payload, CanFrame &response,
                             bool &needs_response);
    bool acceptBlock(uint16_t sequence, const uint8_t *data, uint8_t len, uint32_t now_ms,
                     CanFrame &response, bool &needs_response);
    bool flushSlots(uint16_t count);
    void fail(uint8_t status);

    uint16_t node_id_;
    uint8_t hardware_id_;
    IOtaTarget *target_;

    OtaReceiverState state_;
    uint32_t timeout_ms_;
    uint8_t sender_address_;

    uint8_t version_major_;
    uint8_t version_minor_;
    size_t image_size_;
    uint16_t total_blocks_;
    uint16_t expected_crc_;
    uint8_t window_size_;

    uint16_t window_index_;
    uint16_t window_base_;
    uint16_t window_mask_;
    uint16_t blocks_received_;
    size_t bytes_written_;
    uint16_t computed_crc_;

    uint32_t last_activity_ms_;
    uint8_t timeouts_;
    uint8_t last_status_;
    bool image_verified_;

    uint8_t slots_[CAN_OTA_WINDOW_SLOTS][kPayloadSize - 1];
    uint8_t slot_len_[CAN_OTA_WINDOW_SLOTS];
};

}  // namespace pcd
