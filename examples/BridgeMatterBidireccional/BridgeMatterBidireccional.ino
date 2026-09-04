/*
 * CAN <-> Matter bidireccional.
 * Dependencia: Matter SDK/core elegido por el firmware ESP32.
 * El nucleo PCD_CAN permanece independiente de Matter.
 */
#include <PCD_CAN.h>

struct MatterAdapter {
    bool publishState(uint16_t node, uint8_t resource, uint8_t channel, float value) {
        // Matter endpoint/cluster attribute write.
        (void)node; (void)resource; (void)channel; (void)value;
        return true;
    }
    void onCommand(uint16_t node, uint8_t resource, uint8_t channel, uint8_t action,
                   uint32_t param) {
        // Matter command -> CanonicalFrame -> RoutingEngine.
        (void)node; (void)resource; (void)channel; (void)action; (void)param;
    }
};

MatterAdapter matter;
void setup() {
    // Registrar endpoints Matter a partir de Gateway::nodes().
    // El gateway puede enviar CFG_SUBSCRIBE para limitar los estados CAN.
}
void loop() {
    // matter.loop(); gateway.poll(millis());
}
