#pragma once

/*
 * Persistencia de la "fuente de la verdad" local de cada nodo.
 *
 *   AVR     -> EEPROM interna (<EEPROM.h>)
 *   ESP32   -> NVS a traves de Preferences (blob unico)
 *   native  -> arreglo en RAM para pruebas
 *
 * StorageAdapter expone un acceso por bytes uniforme; ConfigStore construye
 * sobre el un bloque con cabecera, version, Node-ID, reglas de vinculacion y
 * CRC-16, de modo que un corte de energia nunca deja al nodo con reglas
 * parcialmente escritas: si el CRC no valida se cargan los valores por defecto.
 */

#include <stdint.h>

#include "can_protocol.h"
#include "system_config.h"

namespace pcd {

#ifndef CAN_MAX_RULES
#define CAN_MAX_RULES 12
#endif

class StorageAdapter {
  public:
    virtual ~StorageAdapter() {}

    virtual bool begin() = 0;
    virtual bool read(uint16_t offset, uint8_t *data, uint16_t length) = 0;
    virtual bool write(uint16_t offset, const uint8_t *data, uint16_t length) = 0;
    /* Vuelca a memoria no volatil lo escrito (no-op donde la escritura es directa). */
    virtual bool commit() = 0;
    virtual uint16_t capacity() const = 0;
};

/* Operador de disparo de una regla de vinculacion. */
enum RuleTrigger : uint8_t {
    TRIG_EVENT = 0x00,       /* el evento/accion recibido coincide con `event`   */
    TRIG_VALUE_GREATER = 0x01,
    TRIG_VALUE_LESS = 0x02,
    TRIG_VALUE_EQUAL = 0x03,
    TRIG_ANY = 0x04          /* cualquier trama del origen indicado              */
};

/*
 * Regla de vinculacion logica almacenada en el nodo.
 *
 *   "Si el nodo 0x05 informa el evento pulsacion corta en su entrada 1,
 *    conmutar el rele del canal 2 de este nodo"
 */
struct BindingRule {
    uint16_t source_node;     /* 0x0000 = cualquiera            */
    uint8_t source_resource;
    uint8_t source_channel;   /* kAnyChannel = cualquiera       */
    uint8_t trigger;          /* RuleTrigger                    */
    uint8_t event;            /* evento esperado en TRIG_EVENT  */
    float threshold;          /* umbral en los disparos por valor */
    uint8_t target_resource;  /* recurso local a accionar       */
    uint8_t target_channel;
    uint8_t action;           /* ACT_ON / ACT_OFF / ACT_TOGGLE / ACT_SET_VALUE */
    uint32_t param;           /* temporizador o valor + fade    */
};

static const uint8_t kBindingRuleBytes = 20;
static const uint16_t kConfigMagic = 0x5043; /* 'P','C' */
static const uint8_t kConfigVersion = 2;

/* Cabecera + reglas + CRC. Determina el tamano minimo del adaptador. */
static const uint16_t kConfigHeaderBytes = 7;
static const uint16_t kConfigTotalBytes =
    kConfigHeaderBytes + static_cast<uint16_t>(kBindingRuleBytes) * CAN_MAX_RULES + 2;

class ConfigStore {
  public:
    explicit ConfigStore(StorageAdapter &storage);

    /* Inicializa el medio y carga la configuracion. Devuelve false y aplica los
     * valores por defecto si el bloque esta vacio o el CRC no valida. */
    bool begin(uint16_t default_node_id = 0);

    bool load();
    bool save();
    void reset(uint16_t node_id);

    uint16_t nodeId() const { return node_id_; }
    void setNodeId(uint16_t node_id) { node_id_ = node_id; }
    NodeIdMode nodeIdMode() const { return node_id_mode_; }
    void setNodeIdMode(NodeIdMode mode) { node_id_mode_ = mode; }

    uint8_t ruleCount() const { return rule_count_; }
    const BindingRule &rule(uint8_t index) const { return rules_[index]; }
    bool addRule(const BindingRule &rule);
    bool replaceRule(uint8_t index, const BindingRule &rule);
    bool removeRule(uint8_t index);
    void clearRules() { rule_count_ = 0; }

    /* Serializacion explicita en Big-Endian, identica en AVR, ESP32 y STM32. */
    static void serializeRule(const BindingRule &rule, uint8_t *out);
    static void deserializeRule(const uint8_t *in, BindingRule &out);

  private:
    StorageAdapter &storage_;
    uint16_t node_id_;
    NodeIdMode node_id_mode_;
    uint8_t rule_count_;
    BindingRule rules_[CAN_MAX_RULES];
};

uint16_t crc16(const uint8_t *data, uint16_t length, uint16_t seed = 0xFFFF);

/* ------------------------------------------------------------------ */
/* Adaptadores por plataforma                                           */
/* ------------------------------------------------------------------ */

/* Adaptador en RAM: pruebas de escritorio y nodos sin memoria no volatil. */
class MemoryStorage : public StorageAdapter {
  public:
    MemoryStorage();
    bool begin() override;
    bool read(uint16_t offset, uint8_t *data, uint16_t length) override;
    bool write(uint16_t offset, const uint8_t *data, uint16_t length) override;
    bool commit() override;
    uint16_t capacity() const override { return kConfigTotalBytes; }

    /* Simula un corte de energia corrompiendo un byte del bloque. */
    void corrupt(uint16_t offset);

  private:
    uint8_t buffer_[kConfigTotalBytes];
};

#if defined(__AVR__)
class EepromStorage : public StorageAdapter {
  public:
    explicit EepromStorage(uint16_t base_address = 0);
    bool begin() override;
    bool read(uint16_t offset, uint8_t *data, uint16_t length) override;
    bool write(uint16_t offset, const uint8_t *data, uint16_t length) override;
    bool commit() override;
    uint16_t capacity() const override;

  private:
    uint16_t base_address_;
};
#endif

#if defined(ARDUINO_ARCH_ESP32)
class NvsStorage : public StorageAdapter {
  public:
    explicit NvsStorage(const char *nvs_namespace = "pcd", const char *key = "cfg");
    bool begin() override;
    bool read(uint16_t offset, uint8_t *data, uint16_t length) override;
    bool write(uint16_t offset, const uint8_t *data, uint16_t length) override;
    bool commit() override;
    uint16_t capacity() const override { return kConfigTotalBytes; }

  private:
    const char *namespace_;
    const char *key_;
    uint8_t buffer_[kConfigTotalBytes];
    bool dirty_;
};
#endif

}  // namespace pcd
