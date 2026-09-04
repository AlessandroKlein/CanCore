/*
 * CAN <-> ESPHome API bidireccional.
 * Dependencia: API nativa de ESPHome o MQTT/API externa elegida.
 * No se incorpora protobuf, WiFi ni ESPHome al nucleo PCD_CAN.
 */
#include <PCD_CAN.h>

struct ESPHomeAdapter {
    void publishSensor(const char *name, float value) {
        // api.connection->send_state(name, value);
        (void)name; (void)value;
    }
    void onSwitchCommand(uint16_t node, uint8_t channel, bool on) {
        // node.sendCommand(node, RES_RELAY, channel, on ? ACT_ON : ACT_OFF);
        (void)node; (void)channel; (void)on;
    }
};

ESPHomeAdapter esphome;
void setup() {
    // Asociar cada MSG_STATE a una entidad ESPHome y cada comando a CAN.
}
void loop() {
    // esphome.loop(); gateway.poll(millis());
}
