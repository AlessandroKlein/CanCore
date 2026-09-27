#pragma once

/*
 * SHA-256 propio (FIPS 180-4), sin dependencias externas.
 *
 * La libreria no incluye ningun stack criptografico de terceros: si el proyecto
 * necesita autenticar el tunel CAN-sobre-IP o firmar comandos de configuracion,
 * puede usar esta implementacion (o inyectar la suya a traves del contrato
 * ITunnelAuthenticator). El tamano del digest y de los bloques es el estandar:
 *
 *   digests:  32 bytes
 *   bloque:  64 bytes
 *
 * La implementacion se valida con los vectores oficiales de FIPS 180-4 y de
 * RFC 4231 en `test/test_security`.
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

static const size_t kSha256DigestBytes = 32;
static const size_t kSha256BlockBytes = 64;

class Sha256 {
  public:
    Sha256();

    /* Reinicia el estado para una nueva operacion. */
    void begin();
    void update(const uint8_t *data, size_t len);
    void final(uint8_t digest[kSha256DigestBytes]);

    /* Utilidad de una sola pasada. */
    static void hash(const uint8_t *data, size_t len, uint8_t digest[kSha256DigestBytes]);

  private:
    void absorb(const uint8_t *data, size_t len); /* no modifica bit_length_ */
    void transform(const uint8_t block[kSha256BlockBytes]);

    uint32_t state_[8];
    uint64_t bit_length_;
    uint8_t buffer_[kSha256BlockBytes];
    size_t buffer_len_;
};

}  // namespace pcd
