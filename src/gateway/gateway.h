#pragma once

/*
 * Gateway orquestador de alto nivel.
 *
 * Une CanNode + RouteTable + RoutingEngine + puentes en un solo objeto, de
 * modo que la aplicacion solo llame a `gateway.begin()`, `gateway.poll()` y
 * configure las reglas por la tabla (o por Web GUI). El usuario no escribe
 * cientos de lineas: define una `GatewayConfig` y conecta sus librerias de red
 * mediante los callbacks de los puentes.
 *
 * Flujo:
 *
 *   CanNode::onAnyFrame() -> Gateway::onCanFrame() -> RoutingEngine::process()
 *     -> RouteTable.match() -> ejecutar destino (CAN via node, o bridge externo)
 *
 * Los bridges (MQTT, Modbus, KNX, ESP-NOW, IP) se conectan con `attachBridge()`.
 */

#include <stdint.h>

#include "bridge/bridge.h"
#include "can_node.h"
#include "routing/route_table.h"
#include "routing/routing_engine.h"
#include "gateway/node_registry.h"

namespace pcd {

struct GatewayConfig {
    uint16_t node_id;         /* Node-ID del gateway (por convencion 0x0001) */
    uint32_t bitrate;         /* velocidad del bus CAN */
    bool publish_discovery;   /* publicar discovery Home Assistant */

    GatewayConfig()
        : node_id(0x0001), bitrate(CAN_BUS_BITRATE), publish_discovery(false) {}
};

class Gateway {
  public:
    Gateway(CanNode &node, RouteTable &table);

    /* Configura el gateway y engancha el RoutingEngine al CanNode. */
    void begin(const GatewayConfig &config);

    /* Llamar en el loop principal. Procesa el bus y drena los bridges. */
    void poll(uint32_t now_ms);

    /* Procesa una trama CAN (ya enganchada via onAnyFrame). */
    void onCanFrame(const CanFrame &frame);

    /* Registra un puente externo (MQTT, Modbus, KNX, ESP-NOW). */
    bool attachBridge(IBridge &bridge);

    /* Registra el callback de salida de la tabla de rutas hacia bridges. */
    void setSink(RoutingEngine::CanonicalSink sink, void *ctx) {
        router_.setSink(sink, ctx);
    }

    RoutingEngine &router() { return router_; }
    const RouteTable &table() const { return table_; }
    NodeRegistry &nodes() { return nodes_; }
    const NodeRegistry &nodes() const { return nodes_; }

  private:
    static void frameTrampoline(const CanFrame &frame, void *ctx);

    CanNode &node_;
    RouteTable &table_;
    RoutingEngine router_;
    GatewayConfig config_;
    IBridge *bridges_[4];
    uint8_t bridge_count_;
    NodeRegistry nodes_;
    uint32_t now_ms_;
};

}  // namespace pcd
