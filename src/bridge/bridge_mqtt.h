#pragma once

/*
 * Puente CAN <-> MQTT (HOME ASSISTANT).
 *
 * Es un ejemplo completo del patron: la libreria define el contrato
 * (`IMqttTransport` con callbacks), y la aplicacion conecta su libreria MQTT
 * preferida (`PubSubClient`, `AsyncMqttClient`...). El puente no importa
 * ninguna libreria MQTT.
 *
 * Topologia:
 *
 *   CAN (MSG_STATE)   ->  pcd/<node_id>/<recurso>/<canal>/state
 *   pcd/<id>/<res>/<ch>/set  ->  CAN (MSG_EVENT dirigido)
 *   pcd/<id>/status   ->  "online"/"offline" segun heartbeat
 *
 * Descubrimiento de Home Assistant: publica en
 *   homeassistant/<componente>/<unique_id>/config
 * donde <componente> se deriva del tipo de recurso.
 */

#include <stddef.h>
#include <stdint.h>

#include "bridge/bridge.h"

namespace pcd {

static const uint8_t kMqttQueueDepth = 8;

class MqttBridge : public IBridge {
  public:
    explicit MqttBridge(IMqttTransport &transport);

    /* Punto de entrada de un mensaje MQTT recibido (llamado por la aplicacion
     * desde su callback del cliente MQTT). */
    void onIncomingMessage(const char *topic, const uint8_t *data, size_t len);

    /* Publica el discovery de Home Assistant para un recurso. */
    bool publishDiscovery(uint16_t node_id, uint8_t resource, uint8_t channel,
                          const char *name);

    /* Publica el estado online/offline de un nodo. */
    bool publishStatus(uint16_t node_id, bool online);

    /* IBridge */
    bool process(const CanonicalFrame &frame) override;
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

  private:
    bool parseTopic(const char *topic, CanonicalFrame &out) const;
    void flushIncoming(CanonicalFrame &out);

    IMqttTransport &transport_;
    CanonicalFrame pending_[kMqttQueueDepth];
    uint8_t pending_count_;
};

}  // namespace pcd
