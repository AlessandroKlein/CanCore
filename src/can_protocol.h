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
    MSG_STATE = 0x06,      /* respuesta de estado / ACK        */
    MSG_DISCOVERY = 0x07   /* identidad y capacidades del nodo */
};

enum ResourceType : uint8_t {
    RES_RELAY = 0x10,
    RES_DIMMER = 0x20,
    RES_DIGITAL_INPUT = 0x30,
    RES_ENV_SENSOR = 0x40,
    RES_GAS_SENSOR = 0x50,
    RES_POWER_SENSOR = 0x60,
    RES_COVER = 0x70,
    RES_BUTTON = 0x71,
    RES_BINARY_SENSOR = 0x72,
    RES_LIGHT = 0x73,
    RES_FAN = 0x74,
    RES_LOCK = 0x75,
    RES_VALVE = 0x76,
    RES_WATER_LEAK = 0x77,
    RES_SMOKE = 0x78,
    RES_PRESSURE_SENSOR = 0x80,
    RES_HUMIDITY_SENSOR = 0x81,
    RES_CO2_SENSOR = 0x82,
    RES_AIR_QUALITY = 0x83,
    RES_GPS = 0x84,
    RES_VIBRATION_SENSOR = 0x85,
    RES_VOLTAGE_SENSOR = 0x90,
    RES_CURRENT_SENSOR = 0x91,
    RES_FREQUENCY_SENSOR = 0x92,
    RES_ENERGY_SENSOR = 0x93,
    RES_BATTERY = 0x94,
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

/*
 * Sub-comandos SDO (byte 1 de una trama MSG_CONFIG).
 *
 * Una regla de vinculacion no entra en los 6 bytes utiles de una trama, por lo
 * que se transfiere segmentada: BEGIN, cuatro CHUNK de 5 bytes y COMMIT con el
 * CRC-16 del conjunto. Hasta que el CRC valida, la regla no se aplica ni se
 * persiste.
 */
enum ConfigCommand : uint8_t {
    CFG_SET_NODE_ID = 0x01,
    CFG_RULE_BEGIN = 0x02,
    CFG_RULE_CHUNK = 0x03,
    CFG_RULE_COMMIT = 0x04,
    CFG_RULE_DELETE = 0x05,
    CFG_RULE_CLEAR = 0x06,
    CFG_SAVE = 0x07,
    CFG_RULE_COUNT = 0x08,
    CFG_ACK = 0x7F,
    CFG_SET_NODE_MODE = 0x09,
    CFG_SUBSCRIBE = 0x0A,
    CFG_UNSUBSCRIBE = 0x0B,
    CFG_CLEAR_SUBSCRIPTIONS = 0x0C
};

enum NodeIdMode : uint8_t {
    NODE_ID_MANUAL = 0x00,
    NODE_ID_AUTOMATIC = 0x01
};

enum DiscoveryCommand : uint8_t {
    DISCOVERY_REQUEST = 0x01,
    DISCOVERY_ANNOUNCE = 0x02,
    DISCOVERY_RESOURCE = 0x03
};

/* Codigos de resultado devueltos en un CFG_ACK. */
enum ConfigStatus : uint8_t {
    CFG_STATUS_OK = 0x00,
    CFG_STATUS_BAD_REQUEST = 0x01,
    CFG_STATUS_CRC_ERROR = 0x02,
    CFG_STATUS_FULL = 0x03,
    CFG_STATUS_STORAGE_ERROR = 0x04
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
uint16_t deriveAutomaticNodeId(uint32_t unique_value, uint16_t salt = 0x51A7);
const char *resourceName(uint8_t resource);

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

CanFrame makeDiscoveryRequest(uint16_t source, uint8_t target = kBroadcastTarget);
CanFrame makeDiscoveryAnnounce(uint16_t source, uint8_t id_mode, uint8_t resource_count);
CanFrame makeDiscoveryResource(uint16_t source, uint8_t resource, uint8_t channel);

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
