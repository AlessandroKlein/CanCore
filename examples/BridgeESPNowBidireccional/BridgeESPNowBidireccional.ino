/*
 * CAN <-> ESP-NOW bidireccional.
 * Dependencia: esp_now.h (core ESP32). PCD_CAN solo define el contrato.
 * La trama se transporta como ID CAN (4 bytes) + DLC (1) + payload (8).
 */
#include <PCD_CAN.h>
#include <esp_now.h>
#include <string.h>

uint8_t peerMac[6] = {0x24, 0x6F, 0x28, 0, 0, 1};

bool espNowSend(const uint8_t *mac, const uint8_t *data, size_t len) {
    return esp_now_send(mac, data, len) == ESP_OK;
}
void espNowReceive(const uint8_t *mac, const uint8_t *data, size_t len, void *ctx) {
    // Adaptar el paquete al bridge/router del proyecto y responder al peer.
    (void)mac; (void)data; (void)len; (void)ctx;
}
pcd::IEspNowTransport transport = {espNowSend, espNowReceive, 0};

void setup() {
    esp_now_init();
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, sizeof(peerMac));
    esp_now_add_peer(&peer);
    // Conectar transport a un bridge bidireccional del proyecto.
}
void loop() {
    // El callback remoto inyecta comandos; el callback CAN publica estados.
}
