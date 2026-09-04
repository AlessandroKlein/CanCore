/*
 * CAN <-> Zigbee bidireccional.
 * Dependencia elegida por el proyecto: Zigbee Arduino core, ZBOSS o modulo
 * Zigbee externo. No se agrega ninguna a PCD_CAN.
 *
 * Mapear cluster/atributo Zigbee a CanonicalFrame y viceversa. El filtro CAN
 * evita escuchar todo el bus cuando solo interesa un grupo de dispositivos.
 */
#include <PCD_CAN.h>

struct ZigbeeAdapter {
    bool publish(uint16_t node, uint8_t resource, uint8_t channel, float value) {
        // zigbee.writeAttribute(node, clusterFor(resource), channel, value);
        (void)node; (void)resource; (void)channel; (void)value;
        return true;
    }
    void onAttribute(uint16_t node, uint8_t resource, uint8_t channel, float value) {
        // Convertir atributo recibido a bridge.queueIncoming(...).
        (void)node; (void)resource; (void)channel; (void)value;
    }
};

ZigbeeAdapter zigbee;
void setup() {
    // gateway.attachBridge(zigbeeBridge);
    // gateway.nodes() presenta los recursos descubiertos por CAN.
}
void loop() {
    // zigbee.loop(); gateway.poll(millis());
}
