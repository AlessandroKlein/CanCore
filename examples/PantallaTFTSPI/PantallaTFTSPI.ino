/*
 * Pantalla TFT/SPI tactil como nodo CAN.
 * Dependencia del ejemplo: TFT_eSPI, LovyanGFX o driver SPI elegido.
 * La pantalla observa estados CAN y envia comandos al tocar controles.
 */
#include <PCD_CAN.h>

// TFT_eSPI tft; TouchDriver touch;
pcd::CanNode *node = 0;
void onRemoteState(const pcd::CanFrame &frame, float value, void *) {
    // Dibujar nombre de recurso, canal, valor y Node-ID en la pantalla.
    (void)frame; (void)value;
}
void setup() {
    // inicializar TFT/SPI/tactil y crear node con el HAL CAN;
    // node->subscribe(pcd::kAnySource, pcd::RES_RELAY, pcd::kAnyChannel,
    //                 onRemoteState);
    // node->addListenFilter(pcd::kAnySource, pcd::RES_RELAY, pcd::kAnyChannel);
}
void loop() {
    // leer toque y llamar node->sendCommand(...); luego node->poll(millis()).
}
