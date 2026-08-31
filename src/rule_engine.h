#pragma once

/*
 * Motor de reglas de vinculacion logica.
 *
 * Es lo que hace al ecosistema realmente descentralizado: las reglas viven en
 * el nodo que ejecuta la accion, se guardan en su memoria no volatil y se
 * evaluan con cada trama recibida. Si el gateway se apaga, la tecla de la pared
 * sigue encendiendo la luz.
 *
 * El motor tambien atiende las tramas SDO (MSG_CONFIG) que permiten programar,
 * borrar y persistir esas reglas desde cualquier punto del bus.
 */

#include <stdint.h>

#include "can_node.h"
#include "config_storage.h"

namespace pcd {

class RuleEngine {
  public:
    RuleEngine(CanNode &node, ConfigStore &config);

    /* Engancha el motor a las tramas entrantes y a los SDO del nodo. */
    void begin();

    /* Evalua una trama contra la tabla de reglas y ejecuta las coincidencias. */
    void evaluate(const CanFrame &frame);

    /* Procesa una trama SDO de configuracion dirigida a este nodo. */
    bool handleConfig(const CanFrame &frame);

    uint8_t pendingSegments() const { return staged_mask_; }

  private:
    static void frameTrampoline(const CanFrame &frame, void *ctx);
    static void configTrampoline(const CanFrame &frame, void *ctx);

    bool matches(const BindingRule &rule, const CanFrame &frame, const CanId &id) const;
    void execute(const BindingRule &rule);
    void sendAck(uint8_t sub_command, uint8_t status);

    CanNode &node_;
    ConfigStore &config_;

    /* Buffer de recepcion segmentada de una regla (4 tramas de 5 bytes). */
    uint8_t staged_[kBindingRuleBytes];
    uint8_t staged_mask_;
    uint8_t staged_index_;
};

}  // namespace pcd
