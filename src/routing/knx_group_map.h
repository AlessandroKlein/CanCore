#pragma once

/*
 * Extension KNX (puente de interoperabilidad).
 *
 * Traduce entre el modelo PCD (Node-ID, recurso, canal) y los objetos de grupo
 * KNX (una direccion de grupo de 16 bits mas un DPT). La capa fisica (TP1 con
 * NCN5120/TP-UART o KNXnet/IP por software) la aporta el proyecto: esta libreria
 * resuelve la tabla de mapeo, la codificacion de los DPT mas usados y su
 * persistencia, que es la parte que debe ser identica en cada instalacion.
 *
 * Ejemplo de la tabla:
 *
 *   grupo 1/1/5 (DPT 9.001) <-> Node-ID 0x0016, RES_ENV_SENSOR, canal 2
 *   grupo 1/1/10 (DPT 1.001) -> Node-ID 0x0016, RES_RELAY, canal 1
 *
 * DPT soportados: 1 (bool), 5 (escalado 0..100 %), 6 (entero 1 byte), 7
 * (entero 2 bytes), 8 (entero 2 bytes con signo), 9 (float 2 bytes), 12 (entero
 * 4 bytes), 14 (float 4 bytes) y 17 (escena). Cualquier otro DPT se rechaza de
 * forma explicita para que el proyecto lo implemente y documente.
 */

#include <stddef.h>
#include <stdint.h>

#include "routing/canonical.h"

namespace pcd {

#ifndef CAN_MAX_KNX_MAPPINGS
#define CAN_MAX_KNX_MAPPINGS 24
#endif

enum KnxDpt : uint8_t {
    KNX_DPT_NONE = 0,
    KNX_DPT_BOOL = 1,       /* 1.xxx  conmutacion (1 bit)                */
    KNX_DPT_SCALING = 5,    /* 5.001  porcentaje 0..100 % (1 byte)       */
    KNX_DPT_VALUE_1 = 6,    /* 6.xxx  entero 8 bits con signo            */
    KNX_DPT_VALUE_2 = 7,    /* 7.xxx  entero 16 bits                     */
    KNX_DPT_VALUE_2F = 8,   /* 8.xxx  entero 16 bits con signo           */
    KNX_DPT_FLOAT_2 = 9,    /* 9.xxx  float 2 bytes (temperatura, lux)   */
    KNX_DPT_VALUE_4 = 12,   /* 12.xxx entero 32 bits                     */
    KNX_DPT_FLOAT_4 = 14,   /* 14.xxx float 4 bytes                      */
    KNX_DPT_SCENE = 17      /* 17.001 numero de escena                   */
};

/* ------------------------------------------------------------------ */
/* Direcciones de grupo                                                 */
/* ------------------------------------------------------------------ */

/* Direccion de 3 niveles: principal (0..15), intermedio (0..7), sub (0..255). */
uint16_t knxGroupAddress(uint8_t main, uint8_t middle, uint8_t sub);
bool knxSplitGroupAddress(uint16_t address, uint8_t &main, uint8_t &middle, uint8_t &sub);

/* Escritura "1/1/5" en un buffer del llamador (sin stdio). */
bool knxFormatGroupAddress(uint16_t address, char *out, size_t out_len);
bool knxParseGroupAddress(const char *text, uint16_t &address_out);

/* ------------------------------------------------------------------ */
/* DPT                                                                  */
/* ------------------------------------------------------------------ */

/* Tamano en bytes de un DPT soportado; 0 si no se soporta. */
uint8_t knxDptSize(uint8_t dpt);

/* Valor numerico -> bytes KNX. Devuelve false si el DPT no se soporta. */
bool knxEncodeDpt(uint8_t dpt, float value, uint32_t scene_param, uint8_t *out, size_t out_len,
                  size_t &len_out);

/* Bytes KNX -> valor numerico. */
bool knxDecodeDpt(uint8_t dpt, const uint8_t *data, size_t len, float &value_out,
                  uint32_t &param_out);

const char *knxDptName(uint8_t dpt);

/* ------------------------------------------------------------------ */
/* Tabla de mapeo de objetos de grupo                                   */
/* ------------------------------------------------------------------ */

struct KnxGroupMapping {
    uint16_t group_address;
    uint8_t dpt;
    uint16_t node_id;
    uint8_t resource;
    uint8_t channel;

    KnxGroupMapping()
        : group_address(0),
          dpt(KNX_DPT_BOOL),
          node_id(0),
          resource(0),
          channel(0) {}
};

static const uint8_t kKnxMappingBytes = 8;

class KnxGroupMap {
  public:
    KnxGroupMap();

    bool add(const KnxGroupMapping &mapping);
    bool remove(uint8_t index);
    void clear();

    uint8_t count() const { return count_; }
    const KnxGroupMapping *at(uint8_t index) const;

    const KnxGroupMapping *findByGroup(uint16_t group_address) const;
    const KnxGroupMapping *findByTarget(uint16_t node_id, uint8_t resource,
                                        uint8_t channel) const;

    /* Telegrama KNX -> canonico (entrada al bus PCD). */
    bool toCanonical(uint16_t group_address, const uint8_t *data, size_t len,
                     CanonicalFrame &out) const;

    /* Estado canonico -> telegrama KNX (salida del bus PCD). */
    bool fromCanonical(const CanonicalFrame &frame, uint16_t &group_address_out, uint8_t *data_out,
                       size_t out_len, size_t &len_out, uint8_t &dpt_out) const;

    /* Persistencia binaria: magia + version + cantidad + entradas de 8 bytes. */
    bool serialize(uint8_t *out, uint16_t out_len) const;
    bool deserialize(const uint8_t *in, uint16_t in_len);

    static const uint8_t kMagic = 0xB7;
    static const uint8_t kVersion = 1;

  private:
    KnxGroupMapping mappings_[CAN_MAX_KNX_MAPPINGS];
    uint8_t count_;
};

}  // namespace pcd
