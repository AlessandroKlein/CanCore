#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ota_manager.h"

namespace pcd {

/*
 * Control local de ventanas OTA: prepara el emisor para ACK/retransmision.
 *
 * Secuencia de la campana:
 *
 *   next()          -> emite los bloques de la ventana hasta completarla
 *   acknowledge()   -> consume el OTA_WINDOW_ACK del receptor; con mascara 0 la
 *                      ventana queda confirmada, con mascara != 0 solo esos
 *                      bloques se retransmiten
 *   retryNext()     -> emite los bloques pendientes de la mascara
 */
class OtaWindowController {
  public:
    explicit OtaWindowController(OtaManager &manager, uint8_t window_size = 16);

    bool next(CanFrame &out);
    bool acknowledge(uint8_t window, uint16_t missing_mask);

    /* Emite el siguiente bloque pendiente de retransmision. Al agotar la
     * mascara la ventana se confirma y avanza a la siguiente. */
    bool retryNext(CanFrame &out);

    void reset();
    uint8_t window() const { return window_index_; }
    uint16_t pendingMask() const { return pending_mask_; }
    bool waitingAck() const { return waiting_ack_; }

  private:
    OtaManager &manager_;
    uint8_t window_size_;
    uint8_t window_index_;
    uint8_t frames_in_window_;
    uint16_t pending_mask_;
    bool waiting_ack_;
};

}  // namespace pcd
