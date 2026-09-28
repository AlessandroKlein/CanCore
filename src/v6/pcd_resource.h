#pragma once

/*
 * PCD v6 - Sistema de recursos generico y descriptor de recurso.
 *
 * Cada nodo publica recursos tipados. El espacio de tipos esta particionado
 * para que el nucleo y los fabricantes no colisionen:
 *
 *   0x00          reservado (NONE)
 *   0x01..0x7F    recursos estandar PCD
 *   0x80..0xEF    extensiones de bus (DALI, Modbus, KNX, ...)
 *   0xF0..0xFE    fabricante / usuario
 *   0xFF          reservado
 *
 * Un recurso no se limita al tipo: se describe con PCD_ResourceDescriptor
 * (tipo de dato, unidad, rango, banderas de acceso y comportamiento) para que
 * un gateway o un cliente pueda interpretarlo sin conocimiento previo.
 */

#include <stdint.h>

#include "v6/pcd_datatypes.h"

namespace pcd {

/* ------------------------------------------------------------------ */
/* Tipos de recurso                                                    */
/* ------------------------------------------------------------------ */

enum class PCD_ResourceType : uint8_t {
    NONE = 0x00,

    /* Estandar (0x01..0x7F). */
    DIGITAL_INPUT = 0x01,
    DIGITAL_OUTPUT = 0x02,
    ANALOG_INPUT = 0x03,
    ANALOG_OUTPUT = 0x04,
    RELAY = 0x05,
    PWM = 0x06,
    TEMPERATURE = 0x07,
    HUMIDITY = 0x08,
    PRESSURE = 0x09,
    LIGHT = 0x0A,
    MOTION = 0x0B,
    SWITCH = 0x0C,
    BUTTON = 0x0D,
    ENCODER = 0x0E,
    COUNTER = 0x0F,
    ENERGY = 0x10,
    CONTACT = 0x11,
    ALARM = 0x12,
    SCENE = 0x13,
    HVAC = 0x14,
    CUSTOM = 0x7F,

    /* Extensiones de bus (0x80..0xEF). */
    DALI = 0x80,
    MODBUS = 0x81,
    KNX = 0x82,

    /* Fabricante / usuario (0xF0..0xFE). */
    MANUFACTURER_BASE = 0xF0,

    RESERVED = 0xFF
};

/* Limites de particion del espacio de tipos. */
static const uint8_t PCD_RESOURCE_EXTENSION_BASE = 0x80;
static const uint8_t PCD_RESOURCE_MANUFACTURER_BASE = 0xF0;

/* Nombre legible de un tipo de recurso. */
const char *pcdResourceTypeName(PCD_ResourceType type);

/* Particiones del espacio de tipos. */
bool pcdIsStandardResource(uint8_t code);
bool pcdIsExtensionResource(uint8_t code);
bool pcdIsManufacturerResource(uint8_t code);

/* ------------------------------------------------------------------ */
/* Banderas de acceso / comportamiento                                 */
/* ------------------------------------------------------------------ */

enum PCD_ResourceFlag : uint8_t {
    PCD_RES_FLAG_READABLE = 0x01,   /* se puede leer (polling)          */
    PCD_RES_FLAG_WRITABLE = 0x02,   /* se puede escribir (comando)      */
    PCD_RES_FLAG_EVENT = 0x04,      /* emite eventos (cambio de valor)  */
    PCD_RES_FLAG_NOTIFY = 0x08,     /* notificacion push habilitable    */
    PCD_RES_FLAG_POLL = 0x10,       /* admite polling                   */
    PCD_RES_FLAG_PERSISTENT = 0x20  /* restaura ultimo estado conocido  */
};

/* ------------------------------------------------------------------ */
/* Descriptor de recurso                                               */
/* ------------------------------------------------------------------ */

struct PCD_ResourceDescriptor {
    uint8_t resourceId;    /* indice local del recurso dentro del nodo     */
    uint8_t resourceType;  /* PCD_ResourceType                             */
    uint8_t flags;         /* PCD_ResourceFlag                             */
    uint8_t dataType;      /* PCD_DataType                                 */
    uint16_t unit;         /* unidad (ver modulo de unidades; 0 = ninguna) */
    uint32_t minValue;     /* rango inferior (raw para enteros; bits IEEE-754 para float) */
    uint32_t maxValue;     /* rango superior                              */
    uint16_t precision;    /* decimales significativos (0 = entero)       */

    PCD_ResourceDescriptor()
        : resourceId(0), resourceType(0), flags(0), dataType(PCD_TYPE_FLOAT32), unit(0),
          minValue(0), maxValue(0), precision(0) {}

    bool readable() const { return (flags & PCD_RES_FLAG_READABLE) != 0; }
    bool writable() const { return (flags & PCD_RES_FLAG_WRITABLE) != 0; }
    bool emitsEvents() const { return (flags & PCD_RES_FLAG_EVENT) != 0; }
    bool notifiable() const { return (flags & PCD_RES_FLAG_NOTIFY) != 0; }
    bool pollable() const { return (flags & PCD_RES_FLAG_POLL) != 0; }
    bool persistent() const { return (flags & PCD_RES_FLAG_PERSISTENT) != 0; }

    /* Rango como float (valido si el tipo es FLOAT32). */
    float minFloat() const;
    float maxFloat() const;
};

/* Mensajes de recurso (tipo de mensaje dentro de MSG_RESOURCE). */
enum PCD_ResourceCommand : uint8_t {
    PCD_RES_CMD_DISCOVERY = 0x01,
    PCD_RES_CMD_DESCRIPTOR = 0x02,
    PCD_RES_CMD_READ = 0x03,
    PCD_RES_CMD_WRITE = 0x04,
    PCD_RES_CMD_RESPONSE = 0x05,
    PCD_RES_CMD_EVENT = 0x06
};

}  // namespace pcd
