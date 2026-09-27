#pragma once

/*
 * PCD - Protocolo de Comunicacion Descentralizado sobre CAN 2.0B (29 bits).
 *
 * Distribucion del identificador extendido (layout A, por defecto, PCD v1):
 *
 *   bits 28..26 (3)  Prioridad   0 = critico ... 7 = trafico de fondo
 *   bits 25..21 (5)  Tipo        heartbeat, evento, SDO, OTA, escena, estado
 *   bits 20..14 (7)  Destino     0x01..0x7E, 0x00 = broadcast / grupo
 *   bits 13..0  (14) Origen      0x0001..0x3FFF
 *
 * Layout B (opt-in, -DPCD_ID_LAYOUT_V2=1, PCD v2 para topologias en arbol):
 *
 *   bits 28..26 (3)  Prioridad   0 = critico ... 7 = trafico de fondo
 *   bits 25..21 (5)  Tipo
 *   bits 20..13 (8)  Destino     0x00 = broadcast, 0x01..0xFF direccion de nodo
 *   bits 12..5  (8)  Origen      0x01..0xFF direccion dentro del segmento
 *   bits 4..0   (5)  Segmento    0..31 (ramal / sub-bus de un hub en arbol)
 *
 * En el layout B el Node-ID publico sigue siendo de 16 bits:
 *
 *   node_id = (segmento << 8) | direccion        -> segmento: 5 bits, dir: 8 bits
 *
 * de modo que encodeId()/decodeId() aceptan la misma firma en ambos layouts y
 * el enrutamiento por ramal se resuelve con el campo segmento sin aprender la
 * topologia. Un nodo de la rama 3 con direccion 0x16 es el Node-ID 0x0316.
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
/* Layout del identificador de 29 bits                                  */
/* ------------------------------------------------------------------ */

#ifndef PCD_ID_LAYOUT_V2
#define PCD_ID_LAYOUT_V2 0
#endif

static const uint8_t kPriorityShift = 26;
static const uint8_t kMsgTypeShift = 21;

static const uint32_t kPriorityMask = 0x07UL;   /* 3 bits  */
static const uint32_t kMsgTypeMask = 0x1FUL;    /* 5 bits  */
static const uint32_t kExtendedIdMask = 0x1FFFFFFFUL;

static const uint8_t kBroadcastTarget = 0x00;
static const uint8_t kPayloadSize = 8;

#if PCD_ID_LAYOUT_V2

static const uint8_t kTargetShift = 13;         /* 8 bits: 20..13 */
static const uint8_t kSourceShift = 5;          /* 8 bits: 12..5  */
static const uint8_t kSegmentShift = 0;         /* 5 bits: 4..0   */

static const uint32_t kTargetMask = 0xFFUL;
static const uint32_t kSourceMask = 0xFFUL;
static const uint32_t kSegmentMask = 0x1FUL;

static const uint8_t kMaxTarget = 0xFF;
static const uint8_t kMaxSegment = 0x1F;

/* node_id = (segmento << 8) | direccion. */
static const uint16_t kMaxSource = static_cast<uint16_t>((kSegmentMask << 8) | kSourceMask);

/* Segmento por defecto del tronco (backbone) de un hub en arbol. */
static const uint8_t kBackboneSegment = 0x00;

#else

static const uint8_t kTargetShift = 14;         /* 7 bits: 20..14 */
static const uint8_t kSourceShift = 0;          /* 14 bits: 13..0 */

static const uint32_t kTargetMask = 0x7FUL;
static const uint32_t kSourceMask = 0x3FFFUL;
static const uint32_t kSegmentMask = 0x00UL;

static const uint8_t kMaxTarget = 0x7E;
static const uint8_t kMaxSegment = 0x00;

static const uint16_t kMaxSource = 0x3FFF;

static const uint8_t kBackboneSegment = 0x00;

#endif

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
    /* Bus externos encapsulados por una Extension (0x86..0x8F libre).
     * El canal transporta la direccion corta del dispositivo externo. */
    RES_DALI_LIGHT = 0x86,
    RES_KNX_GROUP = 0x87,
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
    ACT_SET_VALUE = 0x03,
    /* Escena: `param` lleva el numero de escena. Se usa en MSG_GROUP y en los
     * recursos de bus externos que tienen escenas propias (DALI, KNX). */
    ACT_SCENE = 0x10
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

/*
 * Sesion OTA (MSG_OTA).
 *
 * Una trama de datos lleva el numero de secuencia en el byte 0 y hasta 7 bytes
 * de firmware. Un numero de secuencia 0xFF se reserva como marca de control:
 *
 *   byte 0      kOtaControlMarker (0xFF)
 *   byte 1      OtaCommand (START, META, READY_ACK, WINDOW_ACK, FINISH, ...)
 *   bytes 2..7  campos del sub-comando (documentados en la wiki 6)
 *
 * Por eso el numero maximo de bloques de datos es kOtaMaxDataBlocks (254).
 */
static const uint8_t kOtaControlMarker = 0xFF;
static const uint16_t kOtaMaxDataBlocks = 0x00FE;

enum OtaCommand : uint8_t {
    OTA_START = 0x01,      /* version + tamano de la imagen            */
    OTA_READY_ACK = 0x02,  /* el receptor acepta la campana            */
    OTA_DATA = 0x03,       /* (solo en la documentacion: sin cabecera) */
    OTA_WINDOW_ACK = 0x04, /* numero de ventana + mapa de faltantes    */
    OTA_FINISH = 0x05,     /* fin de transferencia: el nodo verifica   */
    OTA_ABORT = 0x06,      /* cancela la campana                       */
    OTA_STATUS = 0x07,     /* progreso o diagnostico del receptor      */
    OTA_META = 0x08        /* familia de hardware, CRC-16 y bloques    */
};

/* Resultado devuelto por el receptor en READY_ACK, STATUS, FINISH y ABORT. */
enum OtaStatus : uint8_t {
    OTA_STATUS_OK = 0x00,
    OTA_STATUS_BAD_REQUEST = 0x01,
    OTA_STATUS_WRONG_TARGET = 0x02,
    OTA_STATUS_HW_MISMATCH = 0x03,
    OTA_STATUS_NO_SPACE = 0x04,
    OTA_STATUS_CRC_ERROR = 0x05,
    OTA_STATUS_FLASH_ERROR = 0x06,
    OTA_STATUS_SEQUENCE_ERROR = 0x07,
    OTA_STATUS_TIMEOUT = 0x08,
    OTA_STATUS_NOT_READY = 0x09
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

/*
 * Banderas de salud del nodo (byte 2 de MSG_HEARTBEAT y byte 2 del reporte de
 * salud). Se mantienen enmascaradas para que el mapa de bits sea estable.
 */
enum HealthFlag : uint8_t {
    HEALTH_OK = 0x00,
    HEALTH_BUS_OFF = 0x01,
    HEALTH_LOW_HEAP = 0x02,
    HEALTH_OVER_TEMP = 0x04,
    HEALTH_BROWNOUT = 0x08,
    HEALTH_BUS_ERRORS = 0x10,
    HEALTH_SENSOR_FAULT = 0x20,
    HEALTH_CONFIG_LOST = 0x40,
    HEALTH_SERVICE_MODE = 0x80
};

/* Canal del reporte de salud dentro de RES_SYSTEM (MSG_STATE). */
static const uint8_t kHealthChannelReport = 0x02;

/*
 * Estado de salud extendido de un nodo. El heartbeat de 8 bytes solo puede
 * transportar uptime y las banderas de salud, por lo que el resto de las
 * metricas viaja en una trama MSG_STATE con RES_SYSTEM y canal
 * kHealthChannelReport (makeHealthReport()).
 */
struct NodeHealth {
    uint8_t flags;          /* HealthFlag                */
    uint8_t heap_percent;   /* 0..100, heap/RAM libre    */
    int8_t temperature_c;   /* -40..125 grados celsius   */
    uint8_t brownout_count; /* saturado en 255           */
    uint8_t tec;            /* transmit error counter    */
    uint8_t rec;            /* receive error counter     */

    NodeHealth()
        : flags(HEALTH_OK),
          heap_percent(100),
          temperature_c(25),
          brownout_count(0),
          tec(0),
          rec(0) {}
};

/* ------------------------------------------------------------------ */
/* Identificador                                                        */
/* ------------------------------------------------------------------ */

struct CanId {
    uint8_t priority;
    uint8_t msg_type;
    uint8_t target;
    uint16_t source;
    uint8_t segment;   /* layout B: ramal del Node-ID (0 en el layout A) */

    CanId()
        : priority(PRIO_TELEMETRY),
          msg_type(MSG_STATE),
          target(kBroadcastTarget),
          source(0),
          segment(0) {}

    CanId(uint8_t prio, uint8_t type, uint8_t dst, uint16_t src)
        : priority(prio),
          msg_type(type),
          target(dst),
          source(src),
          segment(static_cast<uint8_t>((src >> 8) & kSegmentMask)) {}

    /* Direccion dentro del segmento (los 8 bits bajos del Node-ID). */
    uint8_t address() const { return static_cast<uint8_t>(source & kSourceMask); }

    /* Node-ID completo: segmento y direccion. */
    uint16_t nodeId() const {
        return static_cast<uint16_t>(((segment & kSegmentMask) << 8) | address());
    }
};

/* Empaqueta los cuatro campos en un identificador extendido de 29 bits. */
uint32_t encodeId(const CanId &id);
uint32_t encodeId(uint8_t priority, uint8_t msg_type, uint8_t target, uint16_t source);

/* Descompone un identificador extendido en sus campos. */
CanId decodeId(uint32_t raw_id);

/* Valida rangos de nodo (destino <= kMaxTarget, origen 0x0001..kMaxSource). */
bool isValidTarget(uint8_t target);
bool isValidSource(uint16_t source);
uint16_t deriveAutomaticNodeId(uint32_t unique_value, uint16_t salt = 0x51A7);
const char *resourceName(uint8_t resource);

/* Compone y descompone un Node-ID de 16 bits (segmento << 8 | direccion). */
uint16_t makeNodeId(uint8_t segment, uint8_t address);
uint8_t nodeIdSegment(uint16_t node_id);
uint8_t nodeIdAddress(uint16_t node_id);

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

/*
 * Reporte de salud extendido (MSG_STATE, RES_SYSTEM, canal
 * kHealthChannelReport). Acompana al heartbeat y transporta las metricas que
 * no caben en sus 8 bytes: heap libre, temperatura, brown-outs y contadores
 * de error TEC/REC.
 */
CanFrame makeHealthReport(uint16_t source, const NodeHealth &health);

/* Lectura de las tramas de salud. Devuelven false si el formato no coincide. */
bool decodeHeartbeat(const CanFrame &frame, uint32_t &uptime_s, uint8_t &health_flags);
bool decodeHealthReport(const CanFrame &frame, NodeHealth &out);

CanFrame makeDiscoveryRequest(uint16_t source, uint8_t target = kBroadcastTarget);
CanFrame makeDiscoveryAnnounce(uint16_t source, uint8_t id_mode, uint8_t resource_count);
CanFrame makeDiscoveryResource(uint16_t source, uint8_t resource, uint8_t channel);

/* Trama SDO de configuracion / vinculacion logica. */
CanFrame makeConfig(uint16_t source, uint8_t target, uint8_t sub_command, const uint8_t *payload,
                    uint8_t payload_len);

/* Bloque de datos OTA: indice de secuencia + 7 bytes de firmware. */
CanFrame makeOtaData(uint16_t source, uint8_t target, uint8_t sequence, const uint8_t *chunk,
                     uint8_t chunk_len);

/*
 * Trama de control OTA: byte 0 = kOtaControlMarker, byte 1 = OtaCommand y los
 * 6 bytes restantes con los campos del sub-comando (hasta 6 bytes de payload).
 */
CanFrame makeOtaControl(uint16_t source, uint8_t target, uint8_t command, const uint8_t *payload,
                        uint8_t payload_len);

/* Extrae el sub-comando y copia los 6 bytes de campos en payload_out (puede ser 0). */
bool decodeOtaControl(const CanFrame &frame, uint8_t &command, uint8_t *payload_out = 0);

/* true cuando la trama MSG_OTA es de control (no un bloque de firmware). */
bool isOtaControlFrame(const CanFrame &frame);

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
