#pragma once

/*
 * Autenticacion HMAC-SHA256 de datagramas de tunel CAN-sobre-IP.
 *
 * Implementa el contrato `ITunnelAuthenticator` que consume la aplicacion del
 * gateway, de modo que el tunel pueda dejar de ser "confianza ciega en la LAN"
 * y pasar a exigir un tag de integridad por datagrama. Tambien sirve para
 * firmar comandos SDO de configuracion y campanas OTA.
 *
 *   tag = HMAC-SHA256(clave, datagrama)[0..tag_len)
 *
 * Detalles de diseno:
 *
 *   - la clave se normaliza al construirse: si supera 64 bytes se reemplaza por
 *     su SHA-256 (regla de RFC 2104),
 *   - el tag se compara en tiempo constante para no filtrar informacion,
 *   - el truncamiento recomendado es de 16 bytes (128 bits); se admiten 8..32,
 *   - la clave nunca se copia a memoria no volatil por la libreria.
 *
 * La implementacion se valida con los vectores oficiales de RFC 4231 en
 * `test/test_security`.
 */

#include <stddef.h>
#include <stdint.h>

#include "tunnel/tunnel_transport.h"

namespace pcd {

#ifndef CAN_HMAC_KEY_BYTES
#define CAN_HMAC_KEY_BYTES 64
#endif

static const size_t kHmacMinTagBytes = 8;
static const size_t kHmacFullTagBytes = 32;
static const size_t kHmacDefaultTagBytes = 16;

class HmacAuthenticator : public ITunnelAuthenticator {
  public:
    /*
     * key/key_len: secreto compartido (no se guarda en claro mas alla de la
     * instancia). tag_len: 8..32; valores fuera de rango se ajustan al default.
     */
    HmacAuthenticator(const uint8_t *key, size_t key_len,
                      size_t tag_len = kHmacDefaultTagBytes);

    /* ITunnelAuthenticator: firma/verifica un bloque de bytes. */
    bool sign(const uint8_t *data, size_t len, uint8_t *tag, size_t tag_len) override;
    bool verify(const uint8_t *data, size_t len, const uint8_t *tag, size_t tag_len) override;

    /* HMAC completo de 32 bytes. */
    void computeFull(const uint8_t *data, size_t len, uint8_t out[kHmacFullTagBytes]) const;

    /*
     * Variante para datagramas de tunel: firma los len bytes y escribe el
     * datagrama original seguido del tag en out. Devuelve los bytes escritos
     * (0 si no cabe en out_len).
     */
    size_t signDatagram(const uint8_t *datagram, size_t len, uint8_t *out,
                        size_t out_len) const;

    /*
     * Verifica un datagrama extendido. Si el tag es valido devuelve true y
     * `datagram_len` recibe el tamano del datagrama original (len - tag_len).
     */
    bool verifyDatagram(const uint8_t *in, size_t len, size_t &datagram_len) const;

    size_t tagBytes() const { return tag_len_; }
    size_t keyBytes() const { return key_len_; }
    bool ready() const { return key_len_ > 0; }

    /* Comparacion en tiempo constante. */
    static bool constantTimeEquals(const uint8_t *a, const uint8_t *b, size_t len);

  private:
    uint8_t key_[CAN_HMAC_KEY_BYTES];
    size_t key_len_;
    size_t tag_len_;
};

}  // namespace pcd
