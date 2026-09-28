#pragma once

/*
 * PCD v6.3 - Eventos estructurados y almacen temporal.
 *
 * El nucleo genera eventos estructurados (PCD_Event) que el gateway puede
 * persistir en SQLite/InfluxDB/PostgreSQL sin que el Core PCD conozca ninguna
 * base de datos. PCD_EventStore es un buffer circular acotado que retiene los
 * ultimos N eventos (para el dashboard y la API).
 */

#include <stdint.h>

namespace pcd {

enum PCD_EventType : uint8_t {
    PCD_EVENT_STATE = 0,   /* cambio de valor de un recurso         */
    PCD_EVENT_ONLINE,      /* nodo detectado online                 */
    PCD_EVENT_OFFLINE,     /* nodo detectado offline                */
    PCD_EVENT_REBOOT,      /* reinicio del nodo (reason = causa)    */
    PCD_EVENT_ALARM,       /* alarma disparada (reason = id)        */
    PCD_EVENT_CONFIG,      /* configuracion cambiada                */
    PCD_EVENT_OTA,         /* actualizacion de firmware             */
    PCD_EVENT_DIAGNOSTIC   /* diagnostico / salud                   */
};

struct PCD_Event {
    PCD_EventType type;
    uint16_t nodeId;
    uint8_t resourceId;
    uint8_t reason;      /* causa de reboot / id de alarma */
    uint32_t timestampMs;
    float value;

    PCD_Event()
        : type(PCD_EVENT_STATE), nodeId(0), resourceId(0), reason(0), timestampMs(0), value(0.0f) {}
};

class PCD_EventStore {
  public:
    static const uint16_t kCapacity = 64;

    PCD_EventStore();

    /* Agrega un evento (sobrescribe el mas antiguo si esta lleno). */
    void push(const PCD_Event &event);

    uint16_t count() const { return count_; }

    /* Acceso por indice (0 = mas antiguo). */
    const PCD_Event &at(uint16_t index) const;

    void clear();

  private:
    PCD_Event events_[kCapacity];
    uint16_t head_;    /* siguiente posicion de escritura */
    uint16_t count_;
};

}  // namespace pcd
