/*
 * CAN <-> Ethernet bidireccional.
 * Dependencia: Ethernet.h/EthernetUDP o W5500 del proyecto.
 * Es el mismo ITunnelTransport que WiFi: el protocolo PCD no cambia.
 */
#include <PCD_CAN.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

EthernetUDP udp;
byte mac[] = {0x02, 0x50, 0x43, 0x44, 0x00, 0x01};
void setup() {
    Ethernet.begin(mac);
    udp.begin(8888);
    // Implementar ITunnelTransport sobre udp y conectar TunnelEngine.
}
void loop() {
    // CAN -> datagrama Ethernet y datagrama Ethernet -> node.handleFrame().
}
