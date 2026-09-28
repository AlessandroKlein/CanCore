#pragma once

/*
 * PCD v6.2 - Capa de seguridad extensible y anti-replay.
 *
 * CRC != seguridad: el CRC detecta corrupcion accidental, no impide que un
 * atacante genere un mensaje valido. Esta capa define:
 *
 *   - PCD_SecurityProvider: contrato abstracto (sign/verify/authenticate/
 *     encrypt/decrypt). La implementacion criptografica concreta queda marcada
 *     como HARDWARE/CRYPTOGRAPHIC_VALIDATION_REQUIRED y la inyecta la aplicacion
 *     (por ejemplo HMAC-SHA256 sobre la base ya existente).
 *   - PCD_ReplayGuard: proteccion anti-replay por contador con ventana
 *     deslizante, para rechazar mensajes capturados y retransmitidos.
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

enum PCD_SecurityMode : uint8_t {
    PCD_SECURITY_NONE = 0,          /* sin seguridad                         */
    PCD_SECURITY_CRC_ONLY = 1,      /* integridad por CRC (no criptografica) */
    PCD_SECURITY_AUTHENTICATION_HOOK = 2, /* autenticacion/verificacion inyectada */
    PCD_SECURITY_ENCRYPTED = 3      /* cifrado (VALIDATION REQUIRED)         */
};

class PCD_SecurityProvider {
  public:
    virtual ~PCD_SecurityProvider() {}

    virtual PCD_SecurityMode mode() const = 0;

    /* Firma/verifica la integridad de un mensaje (tag de longitud tagLen). */
    virtual bool sign(const uint8_t *data, size_t len, uint8_t *tag, size_t tag_len) = 0;
    virtual bool verify(const uint8_t *data, size_t len, const uint8_t *tag,
                        size_t tag_len) = 0;

    /* Autentica un desafio (challenge/response). */
    virtual bool authenticate(const uint8_t *challenge, size_t challenge_len, uint8_t *response,
                              size_t response_len) = 0;

    /* Cifra/descifra. out_len es entrada/salida. false si no soportado. */
    virtual bool encrypt(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) = 0;
    virtual bool decrypt(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) = 0;
};

/* Proveedor sin seguridad: integridad trivial, cifrado passthrough. */
class PCD_NoSecurityProvider : public PCD_SecurityProvider {
  public:
    PCD_SecurityMode mode() const override { return PCD_SECURITY_NONE; }
    bool sign(const uint8_t *, size_t, uint8_t *, size_t) override { return true; }
    bool verify(const uint8_t *, size_t, const uint8_t *, size_t) override { return true; }
    bool authenticate(const uint8_t *, size_t, uint8_t *, size_t) override { return true; }
    bool encrypt(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) override;
    bool decrypt(const uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) override;
};

/*
 * Proveedor de integridad por CRC32 (no criptografico). sign() escribe 4 bytes
 * de CRC32; verify() los recalcula y compara. encrypt/decrypt no soportados.
 */
class PCD_CrcOnlyProvider : public PCD_SecurityProvider {
  public:
    PCD_SecurityMode mode() const override { return PCD_SECURITY_CRC_ONLY; }
    bool sign(const uint8_t *data, size_t len, uint8_t *tag, size_t tag_len) override;
    bool verify(const uint8_t *data, size_t len, const uint8_t *tag, size_t tag_len) override;
    bool authenticate(const uint8_t *, size_t, uint8_t *, size_t) override { return true; }
    bool encrypt(const uint8_t *, size_t, uint8_t *, size_t *) override { return false; }
    bool decrypt(const uint8_t *, size_t, uint8_t *, size_t *) override { return false; }
};

/* ------------------------------------------------------------------ */
/* Anti-replay por contador con ventana deslizante                      */
/* ------------------------------------------------------------------ */

class PCD_ReplayGuard {
  public:
    static const uint8_t kMaxPeers = 16;
    static const uint32_t kWindowBits = 32; /* contadores aceptados hacia atras */

    PCD_ReplayGuard();

    /* true si `counter` no es una repeticion (y lo registra como visto). */
    bool accept(uint16_t nodeId, uint32_t counter);

    /* Consulta sin registrar. */
    bool isReplay(uint16_t nodeId, uint32_t counter) const;

    void reset(uint16_t nodeId);
    void clear();

  private:
    struct Peer {
        uint16_t nodeId;
        uint32_t highest;
        uint32_t seen;
        bool valid;
    };

    Peer *find(uint16_t nodeId);
    const Peer *find(uint16_t nodeId) const;

    Peer peers_[kMaxPeers];
    uint8_t count_;
};

}  // namespace pcd
