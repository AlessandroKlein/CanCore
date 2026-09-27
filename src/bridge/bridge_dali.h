#pragma once

/*
 * Extension DALI (iluminacion profesional).
 *
 * El bus DALI es un bus de 16 bits hacia los balastos (forward frame) y 8 bits
 * de respuesta (backward frame), con velocidad fija de 1200 bps. Esta libreria
 * NO genera la senal electrica: define la codificacion de las tramas y un
 * puente agnostico que traduce comandos PCD a DALI y estados DALI a PCD, de modo
 * que el firmware solo tenga que aportar el driver de linea (optoacoplado) y la
 * fuente de alimentacion DALI.
 *
 * Mapeo acordado con el protocolo PCD:
 *
 *   ResourceType          RES_DALI_LIGHT (0x86)
 *   Canal                 0..63  -> direccion corta DALI
 *                         0x80..0x8F -> grupo DALI 0..15
 *                         0xFF  -> broadcast DALI
 *   ACT_ON / ACT_OFF      RECALL MAX LEVEL / OFF
 *   ACT_TOGGLE            conmutacion local del nivel recordado
 *   ACT_SET_VALUE         Direct Arc Power Control (0..100 % -> 0..254)
 *   MSG_GROUP + param     GO TO SCENE 0..15
 *
 * El nivel recordado se guarda por direccion corta en una tabla de 16 entradas
 * (suficiente para el uso tipico de un ramal; ampliable con CAN_DALI_TRACK_SLOTS).
 */

#include <stddef.h>
#include <stdint.h>

#include "bridge/bridge.h"
#include "can_protocol.h"

namespace pcd {

/* Direcciones y constantes DALI. */
static const uint8_t kDaliShortMax = 63;
static const uint8_t kDaliGroupBase = 0x80;
static const uint8_t kDaliGroupMax = 0x8F;
static const uint8_t kDaliBroadcastChannel = 0xFF;
static const uint8_t kDaliBroadcastDapc = 0xFF;
static const uint8_t kDaliBroadcastCommand = 0xFE;
static const uint8_t kDaliSpecialSetDtr = 0xA3;
static const uint8_t kDaliMaxLevel = 254;

/* Comandos DALI estandar (segundo byte del forward frame). */
enum DaliCommand : uint8_t {
    DALI_CMD_OFF = 0x00,
    DALI_CMD_UP = 0x01,
    DALI_CMD_DOWN = 0x02,
    DALI_CMD_STEP_UP = 0x03,
    DALI_CMD_STEP_DOWN = 0x04,
    DALI_CMD_RECALL_MAX_LEVEL = 0x05,
    DALI_CMD_RECALL_MIN_LEVEL = 0x06,
    DALI_CMD_ON_AND_STEP_UP = 0x08,
    DALI_CMD_GO_TO_SCENE_0 = 0x10,
    DALI_CMD_RESET = 0x20,
    DALI_CMD_STORE_MAX_LEVEL = 0x2A,
    DALI_CMD_STORE_MIN_LEVEL = 0x2B,
    DALI_CMD_STORE_FADE_TIME = 0x2E
};

#ifndef CAN_DALI_TRACK_SLOTS
#define CAN_DALI_TRACK_SLOTS 16
#endif

/* ------------------------------------------------------------------ */
/* Codificacion de tramas DALI                                          */
/* ------------------------------------------------------------------ */

uint16_t daliDirectArcPower(uint8_t short_address, uint8_t level);
uint16_t daliShortCommand(uint8_t short_address, uint8_t command);
uint16_t daliGroupArcPower(uint8_t group, uint8_t level);
uint16_t daliGroupCommand(uint8_t group, uint8_t command);
uint16_t daliBroadcastArcPower(uint8_t level);
uint16_t daliBroadcastCommand(uint8_t command);

/* Trama DALI descompuesta. */
struct DaliFrame {
    uint8_t address_byte;
    uint8_t data_byte;

    uint16_t raw() const {
        return static_cast<uint16_t>((static_cast<uint16_t>(address_byte) << 8) | data_byte);
    }
};

DaliFrame splitDaliFrame(uint16_t frame);
bool daliIsDirectArcPower(uint16_t frame);

/* Conversion entre porcentaje (0..100) y nivel DALI (0..254). */
uint8_t daliLevelFromPercent(uint8_t percent);
uint8_t daliPercentFromLevel(uint8_t level);

/* Codigo de fade time DALI mas cercano a un tiempo en milisegundos. */
uint8_t daliFadeTimeCode(uint32_t fade_ms);

/* ------------------------------------------------------------------ */
/* Transporte inyectable                                                */
/* ------------------------------------------------------------------ */

struct IDaliTransport {
    /* Enviar un forward frame de 16 bits al bus DALI. */
    bool (*sendFrame)(uint16_t frame, void *ctx);
    /* Callback de backward frame (nivel o estado) recibido del bus DALI. */
    void (*onBackward)(uint8_t short_address, uint8_t status, void *ctx);
    void *ctx;

    IDaliTransport() : sendFrame(0), onBackward(0), ctx(0) {}
};

/* ------------------------------------------------------------------ */
/* Puente DALI <-> PCD                                                  */
/* ------------------------------------------------------------------ */

class DaliBridge : public IBridge {
  public:
    explicit DaliBridge(IDaliTransport &transport);

    /* Comando canonico -> DALI. */
    bool process(const CanonicalFrame &frame) override;

    /* Evento DALI -> canonico (nivel confirmado de un balasto). */
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

    /* Encola un nivel leido del bus DALI para publicarlo como MSG_STATE. */
    bool queueLevel(uint8_t short_address, uint8_t level, uint16_t source_id,
                    uint8_t resource = RES_DALI_LIGHT);

    /* Ultimo nivel conocido de una direccion corta (0..254), 0 si no se sabe. */
    uint8_t trackedLevel(uint8_t short_address) const;
    uint32_t framesSent() const { return frames_sent_; }
    uint32_t backwardReceived() const { return backward_received_; }

    /* Notifica niveles recibidos por el transporte (llamar desde el callback). */
    void onBackwardFrame(uint8_t short_address, uint8_t status);

  private:
    bool sendFrame(uint16_t frame);
    bool sendLevel(uint8_t channel, uint8_t level);
    void rememberLevel(uint8_t short_address, uint8_t level);

    IDaliTransport &transport_;
    struct TrackedChannel {
        uint8_t address;
        uint8_t level;
        bool valid;
    };
    TrackedChannel tracked_[CAN_DALI_TRACK_SLOTS];
    uint8_t track_next_;

    CanonicalFrame pending_[8];
    uint8_t pending_count_;
    uint32_t frames_sent_;
    uint32_t backward_received_;
};

}  // namespace pcd
