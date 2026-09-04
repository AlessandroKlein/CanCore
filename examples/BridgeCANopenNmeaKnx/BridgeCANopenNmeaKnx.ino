/*
 * Ejemplo de configuracion de los tres bridges estandar.
 * Los SDK CANopen, NMEA2000 y KNX se conectan mediante callbacks de aplicacion.
 */
#include <PCD_CAN.h>

bool sendPdo(uint16_t, const uint8_t *, size_t, void *) { return true; }
bool sendPgn(uint32_t, uint8_t, const uint8_t *, size_t, void *) { return true; }
bool sendGroup(const uint8_t *, const uint8_t *, size_t, uint8_t) { return true; }

pcd::ICanopenTransport canopen_transport = {sendPdo, 0};
pcd::INmea2000Transport nmea_transport = {sendPgn, 0};
pcd::IKnxTransport knx_transport = {sendGroup, 0, 0};
pcd::CanopenBridge canopen(canopen_transport);
pcd::Nmea2000Bridge nmea(nmea_transport);
pcd::KnxBridge knx(knx_transport);

void setup() {
    // gateway.attachBridge(canopen);
    // gateway.attachBridge(nmea);
    // gateway.attachBridge(knx);
    // Registrar los callbacks del SDK externo para llamar queueIncoming().
}

void loop() {
    // gateway.poll(millis());
}