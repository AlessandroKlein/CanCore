#pragma once

/*
 * PCD v1 - Protocolo de Comunicacion Descentralizado sobre CAN 2.0B (29 bits).
 *
 * Distribucion del identificador extendido:
 *
 *   bits 28..26 (3)  Prioridad   0 = critico ... 7 = trafico de fondo
 *   bits 25..21 (5)  Tipo        heartbeat, evento, SDO, OTA, escena, estado
 *   bits 20..14 (7)  Destino     0x01..0x7E, 0x00 = broadcast / grupo
 *   bits 13..0  (14) Origen      0x0001..0x3FFF
 *
 * Payload estandar de 8 bytes:
 *
 *   byte 0      Tipo de recurso (rele, dimmer, entrada, sensor, ...)
 *   byte 1      Indice de canal dentro del nodo
 *   bytes 2..7  Comando (accion + parametros) o telemetria (float BE + flags)
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

/* ------------------------------------------------------------------ */
/* Campos del identificador de 29 bits                                  */
/* ------------------------------------------------------------------ */

static const uint8_t kPriorityShift = 26;
static const uint8_t kMsgTypeShift = 21;
static const uint8_t kTargetShift = 14;
static const uint8_t kSourceShift = 0;

static const uint32_t kPriorityMask = 0x07UL;   /* 3 bits  */
static const uint32_t kMsgTypeMask = 0x1FUL;    /* 5 bits  */
static const uint32_t kTargetMask = 0x7FUL;     /* 7 bits  */
static const uint32_t kSourceMask = 0x3FFFUL;   /* 14 bits */
static const uint32_t kExtendedIdMask = 0x1FFFFFFFUL;

static const uint8_t kBroadcastTarget = 0x00;
static const uint8_t kMaxTarget = 0x7E;
static const uint16_t kMaxSource = 0x3FFF;

static const uint8_t kPayloadSize = 8;

enum Priority : uint8_t {
    PRIO_CRITICAL = 0,    /* paro de emergencia, alarma termica o de fuego */
    PRIO_REALTIME = 2,    /* control en tiempo real                        */
    PRIO_CONFIG = 4,      /* transacciones SDO / configuracion             */
    PRIO_TELEMETRY = 6,   /* estado y telemetria periodica                 */
    PRIO_BACKGROUND = 7   /* bloques de datos OTA y trafico de fondo       */
};

enum MsgType : uint8_t {
    MSG_HEARTBEAT = 0x01,
    MSG_EVENT = 0x02,      /* PDO: comando / accion            */
    MSG_CONFIG = 0x03,     /* SDO: configuracion y vinculacion */
    MSG_OTA = 0x04,
    MSG_GROUP = 0x05,      /* broadcast de grupo / escenas     */
    MSG_STATE = 0x06       /* respuesta de estado / ACK        */
};

enum ResourceType : uint8_t {
    RES_RELAY = 0x10,
    RES_DIMMER = 0x20,
    RES_DIGITAL_INPUT = 0x30,
    RES_ENV_SENSOR = 0x40,
    RES_GAS_SENSOR = 0x50,
    RES_POWER_SENSOR = 0x60,
    RES_COVER = 0x70,
    RES_CONFIG = 0xE0,
    RES_SYSTEM = 0xF0
};

enum Action : uint8_t {
    ACT_OFF = 0x00,
    ACT_ON = 0x01,
    ACT_TOGGLE = 0x02,
    ACT_SET_VALUE = 0x03
};

/* Eventos generados por entradas digitales (byte 2 de una trama MSG_EVENT). */
enum InputEvent : uint8_t {
    EVT_RELEASED = 0x10,
    EVT_SHORT_CLICK = 0x11,
    EVT_DOUBLE_CLICK = 0x12,
    EVT_LONG_PRESS = 0x13
};

/* Sub-comandos de la sesion OTA (byte 2 de una trama MSG_OTA). */
enum OtaCommand : uint8_t {
    OTA_START = 0x01,
    OTA_READY_ACK = 0x02,
    OTA_DATA = 0x03,
    OTA_WINDOW_ACK = 0x04,
    OTA_FINISH = 0x05,
    OTA_ABORT = 0x06
};

/* Banderas de diagnostico transportadas en los bytes 6..7 de telemetria. */
enum DiagFlag : uint16_t {
    DIAG_NONE = 0x0000,
    DIAG_SENSOR_ERROR = 0x0001,
    DIAG_OVERTEMPERATURE = 0x0002,
    DIAG_UNDERVOLTAGE = 0x0004,
    DIAG_OVERCURRENT = 0x0008,
    DIAG_CALIBRATING = 0x0010
};

/* ------------------------------------------------------------------ */
/* Identificador                                                        */
/* ------------------------------------------------------------------ */

struct CanId {
    uint8_t priority;
    uint8_t msg_type;
    uint8_t target;
    uint16_t source;

    CanId() : priority(PRIO_TELEMETRY), msg_type(MSG_STATE), target(kBroadcastTarget), source(0) {}
    CanId(uint8_t prio, uint8_t type, uint8_t dst, uint16_t src)
        : priority(prio), msg_type(type), target(dst), source(src) {}
};

/* Empaqueta los cuatro campos en un identificador extendido de 29 bits. */
uint32_t encodeId(const CanId &id);
uint32_t encodeId(uint8_t priority, uint8_t msg_type, uint8_t target, uint16_t source);

/* Descompone un identificador extendido en sus campos. */
CanId decodeId(uint32_t raw_id);

/* Valida rangos de nodo (destino <= 0x7E, origen 0x0001..0x3FFF). */
bool isValidTarget(uint8_t target);
bool isValidSource(uint16_t source);

/* ------------------------------------------------------------------ */
/* Trama completa                                                       */
/* ------------------------------------------------------------------ */

struct CanFrame {
    uint32_t id;                  /* identificador extendido de 29 bits */
    uint8_t dlc;                  /* bytes validos en data (0..8)       */
    uint8_t data[kPayloadSize];

    CanFrame() : id(0), dlc(0) {
        for (uint8_t i = 0; i < kPayloadSize; ++i) {
            data[i] = 0;
        }
    }

    CanId fields() const { return decodeId(id); }
    uint8_t resource() const { return data[0]; }
    uint8_t channel() const { return data[1]; }
};

/* ------------------------------------------------------------------ */
/* Constructores de tramas                                              */
/* ------------------------------------------------------------------ */

/* Comando dirigido (MSG_EVENT). param se envia en los bytes 3..7. */
CanFrame makeCommand(uint16_t source, uint8_t target, uint8_t resource, uint8_t channel,
                     uint8_t action, uint32_t param = 0);

/* Difusion de estado tras la conmutacion efectiva (MSG_STATE, broadcast). */
CanFrame makeStateBroadcast(uint16_t source, uint8_t resource, uint8_t channel, float value,
                            uint16_t flags = DIAG_NONE);

/* Telemetria periodica de un sensor (MSG_STATE, broadcast, prioridad baja). */
CanFrame makeTelemetry(uint16_t source, uint8_t resource, uint8_t channel, float value,
                       uint16_t flags = DIAG_NONE);

/* Evento local de una entrada digital (MSG_EVENT en broadcast). */
CanFrame makeInputEvent(uint16_t source, uint8_t channel, uint8_t event);

/* Heartbeat de nodo: uptime en segundos y estado de salud. */
CanFrame makeHeartbeat(uint16_t source, uint32_t uptime_s, uint8_t health = 0);

/* Trama SDO de configuracion / vinculacion logica. */
CanFrame makeConfig(uint16_t source, uint8_t target, uint8_t sub_command, const uint8_t *payload,
                    uint8_t payload_len);

/* Bloque de datos OTA: indice de secuencia + 7 bytes de firmware. */
CanFrame makeOtaData(uint16_t source, uint8_t target, uint8_t sequence, const uint8_t *chunk,
                     uint8_t chunk_len);

/* ------------------------------------------------------------------ */
/* Codificacion de la carga util                                        */
/* ------------------------------------------------------------------ */

void writeUint16BE(uint8_t *dst, uint16_t value);
void writeUint32BE(uint8_t *dst, uint32_t value);
void writeFloatBE(uint8_t *dst, float value);

uint16_t readUint16BE(const uint8_t *src);
uint32_t readUint32BE(const uint8_t *src);
float readFloatBE(const uint8_t *src);

/* Valor y banderas de una trama de estado o telemetria. */
float frameValue(const CanFrame &frame);
uint16_t frameFlags(const CanFrame &frame);

/* Parametro (bytes 3..6) de una trama de comando. */
uint32_t frameParam(const CanFrame &frame);

}  // namespace pcd
