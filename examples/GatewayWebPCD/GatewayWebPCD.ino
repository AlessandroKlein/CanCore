/*
 * Gateway ESP32: UI web de referencia y resumen del bus.
 *
 * Instalar el core ESP32 y la libreria WebServer incluida en Arduino-ESP32.
 * En un gateway real, reemplazar el JSON de ejemplo por el inventario que
 * construya el callback onAnyFrame() a partir de MSG_HEARTBEAT y MSG_DISCOVERY.
 */
#if defined(ARDUINO_ARCH_ESP32)
#include <PCD_CAN.h>
#include <WiFi.h>
#include <WebServer.h>

const char *kSsid = "CAMBIAR_SSID";
const char *kPassword = "CAMBIAR_PASSWORD";
WebServer server(80);

void setup() {
    Serial.begin(115200);
    WiFi.begin(kSsid, kPassword);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html; charset=utf-8", pcd::webAppHtml());
    });
    server.on("/api/summary", HTTP_GET, []() {
        server.send(200, "application/json",
                    "{\"nodes\":0,\"frames_per_minute\":0,\"can_status\":\"ok\"}");
    });
    server.on("/api/nodes", HTTP_GET, []() {
        server.send(200, "application/json", "{\"nodes\":[]}");
    });
    server.begin();
}

void loop() {
    server.handleClient();
}
#else
#error "Este ejemplo requiere una placa ESP32"
#endif
