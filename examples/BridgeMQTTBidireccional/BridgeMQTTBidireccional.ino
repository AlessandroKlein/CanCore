/*
 * CAN <-> MQTT bidireccional.
 * Dependencia del ejemplo: PubSubClient (NO forma parte de PCD_CAN).
 * El gateway escucha estados CAN y publica MQTT; los comandos MQTT vuelven
 * al bus CAN. Las reglas de escucha se pueden cambiar por CFG_SUBSCRIBE.
 */
#include <PCD_CAN.h>
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient net;
PubSubClient mqtt(net);
// Adaptar estas funciones al cliente MQTT elegido.
bool mqttPublish(const char *topic, const uint8_t *data, size_t len, bool retain) {
    return mqtt.publish(topic, data, len, retain);
}
bool mqttSubscribe(const char *topic) { return mqtt.subscribe(topic); }
void mqttMessage(const char *topic, const uint8_t *data, size_t len, void *) {
    // En una aplicacion real, pasar topic/data al MqttBridge.
    (void)topic; (void)data; (void)len;
}

pcd::IMqttTransport mqttTransport;
pcd::MqttBridge bridge(mqttTransport);
// Crear CanNode + RouteTable + Gateway segun los ejemplos base.

void setup() {
    mqttTransport.publish = mqttPublish;
    mqttTransport.subscribe = mqttSubscribe;
    mqttTransport.onMessage = mqttMessage;
    bridge.publishDiscovery(0x0016, pcd::RES_RELAY, 1, "rele_pasillo");
    bridge.publishStatus(0x0001, true);
    // Suscribir pcd/+/+/+/set y configurar filtros CAN del gateway.
}
void loop() {
    mqtt.loop();
    // gateway.poll(millis()) procesa CAN -> MQTT y MQTT -> CAN.
}
