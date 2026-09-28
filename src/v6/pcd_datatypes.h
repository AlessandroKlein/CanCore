#pragma once

/*
 * PCD v6 - Tipos de datos normalizados y codec explicito.
 *
 * El protocolo NUNCA serializa una estructura C/C++ con un cast de memoria
 * (padding, endianness y representacion dependen del compilador/plataforma).
 * En su lugar cada tipo se codifica byte a byte con:
 *
 *   - Byte order: Little Endian
 *   - Enteros:     representacion explicita con/sin signo
 *   - Punto flotante: IEEE-754 (float32 / float64)
 *
 * STRING y BYTES son de longitud variable: el codec de tamano fijo devuelve
 * error y es el emisor quien define su propia regla de longitud (prefijo de
 * 1 byte, NUL, o longitud implicita por el tipo de mensaje).
 */

#include <stddef.h>
#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

enum PCD_DataType : uint8_t {
    PCD_TYPE_BOOL = 0,
    PCD_TYPE_UINT8,
    PCD_TYPE_INT8,
    PCD_TYPE_UINT16,
    PCD_TYPE_INT16,
    PCD_TYPE_UINT32,
    PCD_TYPE_INT32,
    PCD_TYPE_UINT64,
    PCD_TYPE_INT64,
    PCD_TYPE_FLOAT32,
    PCD_TYPE_FLOAT64,
    PCD_TYPE_ENUM,      /* tratado como uint8                           */
    PCD_TYPE_BITFIELD,  /* tratado como uint8                           */
    PCD_TYPE_STRING,    /* longitud variable (no hay tamano fijo)       */
    PCD_TYPE_BYTES,     /* longitud variable (no hay tamano fijo)       */
    PCD_TYPE_COUNT
};

/* Tamano fijo en bytes; 0 para STRING/BYTES (longitud variable). */
size_t pcdTypeSize(PCD_DataType type);

/* true si el tipo es entero (o bool/enum/bitfield). */
bool pcdTypeIsIntegral(PCD_DataType type);

/* true si el tipo es punto flotante. */
bool pcdTypeIsFloat(PCD_DataType type);

/* true si el tipo tiene tamano fijo (encodable por pcdWriteValue/readValue). */
bool pcdTypeIsFixed(PCD_DataType type);

const char *pcdTypeName(PCD_DataType type);

/* ------------------------------------------------------------------ */
/* Codec Little Endian explicito                                       */
/* ------------------------------------------------------------------ */

void pcdWriteUint16LE(uint8_t *dst, uint16_t value);
void pcdWriteUint32LE(uint8_t *dst, uint32_t value);
void pcdWriteUint64LE(uint8_t *dst, uint64_t value);

uint16_t pcdReadUint16LE(const uint8_t *src);
uint32_t pcdReadUint32LE(const uint8_t *src);
uint64_t pcdReadUint64LE(const uint8_t *src);

/* Reinterpretacion explicita IEEE-754 -> bits -> LE. */
void pcdWriteFloat32LE(uint8_t *dst, float value);
void pcdWriteFloat64LE(uint8_t *dst, double value);
float pcdReadFloat32LE(const uint8_t *src);
double pcdReadFloat64LE(const uint8_t *src);

/*
 * Escribe `value` (puntero al tipo C correspondiente a `type`) en `dst` con
 * encoding Little Endian. Requiere pcdTypeIsFixed(type) y capacidad suficiente.
 * Devuelve PCD_OK o un error. Para STRING/BYTES devuelve PCD_ERR_INVALID_ARGUMENT.
 */
PCD_Error pcdWriteValue(uint8_t *dst, size_t capacity, PCD_DataType type, const void *value);

/*
 * Lee un valor de `src` (Little Endian) hacia `value` (puntero al tipo C
 * correspondiente). Devuelve PCD_OK o un error.
 */
PCD_Error pcdReadValue(const uint8_t *src, size_t len, PCD_DataType type, void *value);

}  // namespace pcd
