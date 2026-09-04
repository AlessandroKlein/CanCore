#pragma once

/*
 * Interfaz de red agnostica (Data Stream Transport).
 *
 * La libreria PCD NO compila ningun stack IP. El usuario elige su libreria de
 * red (WiFi nativo, Ethernet ENC28J60, W5500, LwIP, AsyncTCP, etc.) y conecta
 * la recepcion de paquetes con el puerto de entrada del tunel:
 *
 *   Red del Usuario (WiFi/Eth)  --bytes crudos-->  TunnelEngine  --> CAN Bus
 *
 * Cada peer remoto se identifica por un escalar de 16 bits (peer id) que la
 * aplicacion asigna al configurar la lista de IPs/puertos. El tunel no
 * conoce IPs ni puertos; solo sabe entregar y recibir datagramas de bytes.
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

static const uint8_t kTunnelProtocolVersion = 1;

struct ITunnelAuthenticator {
  virtual ~ITunnelAuthenticator() {}
  virtual bool sign(const uint8_t *data, size_t len, uint8_t *tag,
            size_t tag_len) = 0;
  virtual bool verify(const uint8_t *data, size_t len, const uint8_t *tag,
            size_t tag_len) = 0;
};

static const uint16_t kBroadcastPeer = 0x0000;

class ITunnelTransport {
  public:
    virtual ~ITunnelTransport() {}

    /* Envia un datagrama de bytes crudos al peer indicado.
     * kBroadcastPeer significa "todos los peers configurados". */
    virtual bool send(uint16_t peer, const uint8_t *data, size_t len) = 0;

    /* Recibe el siguiente datagrama pendiente. Devuelve false si no hay.
     * peer_out recibe el id del emisor. */
    virtual bool receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                         size_t &len) = 0;

    /* true cuando hay al menos un datagrama por procesar. */
    virtual bool available() const = 0;
};

}  // namespace pcd
