/*
 * Puente CAN entre dos redes usando TunnelEngine.
 * Dependencia: WiFiUDP, EthernetUDP o socket TCP del proyecto.
 * Cada extremo conserva su CAN local y reenvia tramas sin centralizar el
 * control de los nodos.
 */
#include <PCD_CAN.h>
#include <WiFiUdp.h>

class UdpAdapter : public pcd::ITunnelTransport {
  public:
    bool send(uint16_t peer, const uint8_t *data, size_t len) override {
        (void)peer; udp.beginPacket(remote, remotePort); udp.write(data, len);
        return udp.endPacket() == 1;
    }
    bool receive(uint16_t &peer, uint8_t *data, size_t max, size_t &len) override {
        int packet = udp.parsePacket(); if (packet <= 0) return false;
        len = udp.read(data, max); peer = 1; return true;
    }
    bool available() const override { return const_cast<WiFiUDP &>(udp).parsePacket() > 0; }
    WiFiUDP udp; IPAddress remote; uint16_t remotePort;
};

UdpAdapter transport;
pcd::TunnelEngine tunnel(transport);
void loop() {
    // Enviar las tramas CAN desde onAnyFrame y recibir con tunnel.receiveFrame.
    // node.handleFrame(frame) mantiene el comportamiento descentralizado.
}
