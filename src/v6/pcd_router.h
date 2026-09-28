#pragma once

/*
 * PCD v6.5 - Router multi-CAN.
 *
 * Interfaz generica PCD_CanInterface (una red/segmento CAN) y PCD_CanRouter
 * que reenvia tramas entre interfaces segun reglas explicitas:
 *
 *   sourceNetwork + destinationNetwork + messageType -> FORWARD / DROP / MODIFY / BROADCAST
 *
 * No se modifican automaticamente los IDs de origen/destino salvo que exista
 * una regla explicita que lo indique (accion MODIFY).
 */

#include <stdint.h>

#include "v6/pcd_errors.h"
#include "v6/pcd_id.h"

namespace pcd {

/* Una interfaz CAN (segmento / red). La implementacion concreta usa la HAL. */
class PCD_CanInterface {
  public:
    virtual ~PCD_CanInterface() {}

    virtual uint8_t networkId() const = 0;

    /* Envia una trama a esta red. Devuelve true si se encolo. */
    virtual bool send(const PCD_CAN_ID &id, const uint8_t *payload, uint8_t len) = 0;
};

enum PCD_RouteAction : uint8_t {
    PCD_ROUTE_FORWARD = 0,  /* envia solo a la red destino                        */
    PCD_ROUTE_DROP = 1,     /* descarta                                           */
    PCD_ROUTE_MODIFY = 2,   /* envia a la red destino reetiquetando la red        */
    PCD_ROUTE_BROADCAST = 3 /* envia a todas las redes menos la de origen         */
};

/* Comodin para "cualquier red / cualquier tipo de mensaje". */
static const uint8_t PCD_ROUTE_ANY = 0xFF;

struct PCD_RoutingRule {
    uint8_t sourceNetwork;
    uint8_t destinationNetwork;
    uint8_t messageType;
    PCD_RouteAction action;
    bool enabled;

    PCD_RoutingRule()
        : sourceNetwork(0), destinationNetwork(0), messageType(PCD_ROUTE_ANY),
          action(PCD_ROUTE_FORWARD), enabled(true) {}

    bool matches(uint8_t sourceNetwork, uint8_t messageType) const {
        if (!enabled) {
            return false;
        }
        if (this->sourceNetwork != PCD_ROUTE_ANY && this->sourceNetwork != sourceNetwork) {
            return false;
        }
        if (this->messageType != PCD_ROUTE_ANY && this->messageType != messageType) {
            return false;
        }
        return true;
    }
};

class PCD_CanRouter {
  public:
    static const uint8_t kMaxInterfaces = 4;
    static const uint8_t kMaxRules = 16;

    PCD_CanRouter();

    PCD_Error addInterface(PCD_CanInterface *iface);
    PCD_Error addRule(const PCD_RoutingRule &rule);

    /* Enruta una trama entrante. Devuelve cuantas interfaces la recibieron. */
    uint8_t route(const PCD_CAN_ID &id, const uint8_t *payload, uint8_t len,
                  uint8_t sourceNetwork);

    uint8_t interfaceCount() const { return iface_count_; }
    PCD_CanInterface *interfaceForNetwork(uint8_t networkId) const;

  private:
    void sendTo(PCD_CanInterface *iface, const PCD_CAN_ID &id, const uint8_t *payload,
                uint8_t len);

    PCD_CanInterface *interfaces_[kMaxInterfaces];
    uint8_t iface_count_;
    PCD_RoutingRule rules_[kMaxRules];
    uint8_t rule_count_;
};

}  // namespace pcd
