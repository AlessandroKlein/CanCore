#pragma once

/*
 * Tabla de mapeo dinamico entre protocolos (Cross-Protocol Mapping Table).
 *
 * Define los enlaces:
 *
 *   (Protocolo A, Origen, Recurso, Canal)  ->  (Protocolo B, Destino, Recurso, Canal, Accion, Param)
 *
 * Ejemplo: "Cuando el teclado KNX 1/1/10 informa pulsacion, accionar el rele
 * del Nodo CAN 0x0016, canal 2".
 *
 * La tabla es agnostica y serializable: la aplicacion puede guardarla en NVS
 * (via ConfigStore), EEPROM o LittleFS. Se usa CRC-16 (el mismo de la
 * persistencia de configuracion) para detectar corrupcion.
 */

#include <stdint.h>

#include "routing/canonical.h"

namespace pcd {

#ifndef CAN_MAX_ROUTES
#define CAN_MAX_ROUTES 16
#endif

/* Anchura de cada regla serializada (2+2+1+1 + 2+1+1+1+1+4 = 20). */
static const uint8_t kMappingRuleBytes = 20;

/* Una regla de mapeo: origen -> destino. */
struct MappingRule {
    /* Origen */
    uint8_t src_protocol;    /* ProtocolSource */
    uint16_t src_id;         /* id segun el protocolo de origen */
    uint8_t src_resource;    /* ResourceType */
    uint8_t src_channel;     /* kAnyChannel = comodin */

    /* Destino */
    uint8_t dst_protocol;    /* ProtocolSource */
    uint16_t dst_id;         /* id segun el protocolo de destino */
    uint8_t dst_resource;    /* ResourceType */
    uint8_t dst_channel;
    uint8_t dst_action;      /* Action */
    uint32_t dst_param;      /* parametro (temporizador, fade, consigna) */

    MappingRule()
        : src_protocol(PROTO_CAN), src_id(0), src_resource(0), src_channel(0),
          dst_protocol(PROTO_CAN), dst_id(0), dst_resource(0), dst_channel(0),
          dst_action(ACT_TOGGLE), dst_param(0) {}

    /* true cuando un CanonicalFrame entrante coincide con este origen. */
    bool matches(const CanonicalFrame &c) const {
        if (src_protocol != c.protocol) return false;
        if (src_id != 0 && src_id != c.source_id) return false;
        if (src_resource != 0 && src_resource != c.resource) return false;
        if (src_channel != 0xFF && src_channel != c.channel) return false;
        return true;
    }

    static void serialize(const MappingRule &rule, uint8_t *out);
    static void deserialize(const uint8_t *in, MappingRule &out);
};

class RouteTable {
  public:
    RouteTable();

    /* Gestion de reglas. Devuelve false si la tabla esta llena. */
    bool addRule(const MappingRule &rule);
    bool removeRule(uint8_t index);
    void clear() { count_ = 0; }

    uint8_t count() const { return count_; }
    const MappingRule &rule(uint8_t index) const { return rules_[index]; }

    /* Busca la primera regla cuyo origen coincide con `frame`. */
    bool match(const CanonicalFrame &frame, MappingRule &out) const;

    /* Serializacion para persistencia. Devuelve los bytes escritos en out. */
    bool serialize(uint8_t *out, uint16_t out_len) const;
    bool deserialize(const uint8_t *in, uint16_t in_len);

    static const uint8_t kMagic = 0xA5;
    static const uint8_t kVersion = 1;

  private:
    MappingRule rules_[CAN_MAX_ROUTES];
    uint8_t count_;
};

}  // namespace pcd
