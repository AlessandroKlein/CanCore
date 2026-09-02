#pragma once

/*
 * Motor de tunel CAN-sobre-IP.
 *
 * Empaqueta/desempaqueta tramas del protocolo PCD en datagramas de 16 bytes
 * aptos para UDP/TCP, de modo que la libreria pueda enrutar tramas entre buses
 * CAN remotos sin conocer el stack IP.
 *
 * Formato del datagrama (little-endian):
 *
 *   byte  0      magia 0xA5
 *   bytes 1..4   ID extendido de 29 bits, LSB primero
 *   byte  5      DLC (0..8)
 *   bytes 6..13  payload (8 bytes)
 *   bytes 14..15 CRC-16/CCITT de los bytes 0..13
 *
 * El receptor descarta datagramas con magia incorrecta, DLC invalido o CRC
 * erroneo, evitando que una corrupcion de red se propague al bus CAN.
 */

#include <stddef.h>
#include <stdint.h>

#include "can_protocol.h"
#include "tunnel/tunnel_transport.h"

namespace pcd {

static const uint8_t kTunnelMagic = 0xA5;
static const size_t kTunnelDatagramBytes = 16;  /* 1 + 4 + 1 + 8 + 2 */

/* Codifica una trama CAN en un datagrama de kTunnelDatagramBytes bytes.
 * Devuelve false si out_len es menor que el tamano del datagrama. */
bool encodeTunnelFrame(const CanFrame &frame, uint8_t *out, size_t out_len);

/* Decodifica un datagrama (que debe medir exactamente kTunnelDatagramBytes)
 * en una trama CAN. Valida magia, DLC y CRC. */
bool decodeTunnelFrame(const uint8_t *in, size_t in_len, CanFrame &frame);

class TunnelEngine {
  public:
    explicit TunnelEngine(ITunnelTransport &transport);

    /* Envia una trama al peer remoto (o broadcast si peer == kBroadcastPeer). */
    bool sendFrame(uint16_t peer, const CanFrame &frame);

    /* Recolecta un datagrama pendiente, lo decodifica y devuelve la trama con
     * el peer emisor. Devuelve false si no hay datos validos. */
    bool receiveFrame(uint16_t &peer_out, CanFrame &frame);

    /* true cuando hay al menos un datagrama pendiente. */
    bool available() const;

  private:
    ITunnelTransport &transport_;
};

}  // namespace pcd
