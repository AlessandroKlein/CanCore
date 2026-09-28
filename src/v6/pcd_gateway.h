#pragma once

/*
 * PCD v6.5 - Gateway y API de extensiones.
 *
 * PCD_Gateway orquesta transporte (router multi-CAN) y extensiones de
 * protocolo (DALI, Modbus, KNX, MQTT...). Las extensiones se registran sin
 * modificar el nucleo PCD: terceros implementan PCD_Extension y lo registran.
 *
 * La implementacion fisica de cada protocolo esta desacoplada (contratos
 * inyectables); el gateway solo las registra y les da el ciclo de vida.
 */

#include <stdint.h>

#include "v6/pcd_router.h"

namespace pcd {

/* Contrato de una extension de protocolo. */
class PCD_Extension {
  public:
    virtual ~PCD_Extension() {}

    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual const char *name() const = 0;
    virtual uint16_t version() const = 0;
};

class PCD_Gateway {
  public:
    static const uint8_t kMaxExtensions = 8;

    explicit PCD_Gateway(PCD_CanRouter &router);

    bool registerExtension(PCD_Extension *extension);
    PCD_Error registerRoute(const PCD_RoutingRule &rule);

    void begin();
    void poll(); /* llama update() a cada extension */

    PCD_CanRouter &router() { return router_; }
    uint8_t extensionCount() const { return extension_count_; }

  private:
    PCD_CanRouter &router_;
    PCD_Extension *extensions_[kMaxExtensions];
    uint8_t extension_count_;
};

}  // namespace pcd
