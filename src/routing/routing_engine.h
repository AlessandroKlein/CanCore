#pragma once

/*
 * Motor de enrutamiento unificado (Core Routing Engine).
 *
 * Es el cerebro del gateway multi-protocolo. Recibe una CanonicalFrame (que
 * puede venir de CAN, ESP-NOW, Modbus, KNX, MQTT o IP), la evalua contra la
 * RouteTable y ejecuta la accion de destino:
 *
 *   - Si el destino es CAN: envia un comando al nodo (o lo aplica localmente).
 *   - Si el destino es otro protocolo: entrega la CanonicalFrame a un sink
 *     (un bridge/traductor externo).
 *
 * No depende de ninguna libreria de red: los puentes se conectan con un
 * callback (setSink()).
 */

#include <stdint.h>

#include "can_node.h"
#include "routing/canonical.h"
#include "routing/route_table.h"

namespace pcd {

class RoutingEngine {
  public:
    explicit RoutingEngine(RouteTable &table);

    /* Engancha el enrutador a un CanNode para observar el trafico CAN.
     * node_id es el Node-ID del propio gateway (para detectar auto-tramas). */
    void attach(CanNode &node, uint16_t gateway_node_id);

    /* Callback de salida hacia un puente (protocolo no CAN). */
    typedef void (*CanonicalSink)(const CanonicalFrame &frame, void *ctx);
    void setSink(CanonicalSink sink, void *ctx);

    /* Punto de entrada: una trama CAN entrante. */
    void onCanFrame(const CanFrame &frame);

    /* Punto de entrada: un evento normalizado (desde un bridge). */
    bool process(const CanonicalFrame &frame);

  private:
    void execute(const MappingRule &rule);

    RouteTable &table_;
    CanNode *node_;
    uint16_t gateway_node_id_;
    CanonicalSink sink_;
    void *sink_ctx_;
};

}  // namespace pcd
