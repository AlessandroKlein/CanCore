/*
 * Gateway CAN <-> CAN por Ethernet LAN, ejecutable en dos ESP32.
 *
 * Flashear el mismo sketch en dos placas y cambiar kSegment/kLocalIp/
 * kPeerIp. Cada instancia tiene sus paginas /, /config y /simulate:
 *   /       estado del bridge e inventario local
 *   /config configura peer LAN, puertos y segmento
 *   /simulate simula un dispositivo de la otra red enviando un comando CAN
 *
 * Dependencias SOLO del ejemplo: Ethernet.h/EthernetUDP.h o WiFi.h/WiFiUDP.h
 * y WebServer.h. Ninguna se agrega a PCD_CAN.
 */
#include <PCD_CAN.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>

namespace {

const uint8_t kSegment = 1;       // En la segunda placa usar 2.
const char *kLocalIp = "192.168.10.21";
const char *kPeerIp = "192.168.10.22";
const uint16_t kTunnelPort = 17890;
const uint16_t kPeerPort = 17890;

class LanTransport : public pcd::ITunnelTransport {
  public:
    LanTransport() : peer_(1), port_(kTunnelPort), peer_port_(kPeerPort) {}

    void begin() { udp_.begin(port_); }

    bool send(uint16_t, const uint8_t *data, size_t len) override {
        if (udp_.beginPacket(peer_ip_, peer_port_) != 1) return false;
        if (udp_.write(data, len) != len) return false;
        return udp_.endPacket() == 1;
    }

    bool receive(uint16_t &peer, uint8_t *data, size_t max_len,
                 size_t &len) override {
        const int packet_size = udp_.parsePacket();
        if (packet_size <= 0) return false;
        len = udp_.read(data, max_len);
        peer = peer_;
        return len > 0;
    }

    bool available() const override {
        return const_cast<WiFiUDP &>(udp_).parsePacket() > 0;
    }

    void setPeer(const IPAddress &address, uint16_t port) {
        peer_ip_ = address;
        peer_port_ = port;
    }

    IPAddress peer_ip_;
    WiFiUDP udp_;
    uint16_t peer_;
    uint16_t port_;
    uint16_t peer_port_;
};

#if defined(ARDUINO_ARCH_ESP32)
pcd::Esp32TwaiBus can_bus(5, 4);
#else
#error "Este ejemplo LAN requiere ESP32 TWAI"
#endif
pcd::CanNode node(can_bus, kSegment == 1 ? 0x0001 : 0x0002);
LanTransport transport;
pcd::TunnelEngine tunnel(transport);
pcd::TunnelRelayGuard relay_guard(kSegment);
WebServer server(80);
uint32_t forwarded = 0;
uint32_t received = 0;
uint8_t simulated_target = kSegment == 1 ? 0x16 : 0x17;
bool injecting_remote = false;

void forwardFrame(const pcd::CanFrame &frame, void *) {
    if (!injecting_remote && relay_guard.accept(0xFF, frame, millis())) {
        tunnel.sendFrame(1, frame);
        ++forwarded;
    }
}

void pageHeader(const char *title) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>PCD LAN Bridge</title><style>body{font:16px sans-serif;max-width:760px;margin:30px auto;padding:0 18px;background:#f3f0e8;color:#17212b}nav{display:flex;gap:12px;flex-wrap:wrap;margin:18px 0}a,button{padding:10px 14px;background:#0c7c86;color:white;border:0;text-decoration:none}section{background:white;padding:20px;margin:14px 0;border:1px solid #d8d1c5}input{padding:9px;margin:5px;width:220px}</style></head><body><h1>PCD LAN Bridge</h1><p>Segmento local: ");
    server.sendContent(String(kSegment).c_str());
    server.sendContent("</p><nav><a href='/'>Estado</a><a href='/config'>Configurar</a><a href='/simulate'>Simular dispositivo</a></nav><section><h2>");
    server.sendContent(title);
    server.sendContent("</h2>");
}

void pageEnd() { server.sendContent("</section></body></html>"); }

void handleRoot() {
    pageHeader("Estado bidireccional");
    server.sendContent("<p>Peer LAN: " + transport.peer_ip_.toString() + ":" + String(transport.peer_port_) + "</p>");
    server.sendContent("<p>Datagramas CAN -> LAN: " + String(forwarded) + "</p>");
    server.sendContent("<p>Datagramas LAN -> CAN: " + String(received) + "</p>");
    server.sendContent("<p>Anti-loop: activo, ventana 5 segundos.</p>");
    server.sendContent("<p>El control local sigue funcionando si el peer se desconecta.</p>");
    pageEnd();
}

void handleConfig() {
    if (server.hasArg("peer")) {
        IPAddress address;
        if (address.fromString(server.arg("peer"))) {
            transport.setPeer(address, server.arg("port").toInt());
        }
    }
    pageHeader("Configurar peer");
    server.sendContent("<form method='post'><label>IP peer <input name='peer' value='" + transport.peer_ip_.toString() + "'></label><br><label>Puerto <input name='port' value='" + String(transport.peer_port_) + "'></label><br><button>Guardar</button></form>");
    pageEnd();
}

void handleSimulate() {
    if (server.hasArg("target")) simulated_target = server.arg("target").toInt();
    if (server.method() == HTTP_POST) {
        const uint8_t action = server.arg("action").toInt();
        node.sendCommand(simulated_target, pcd::RES_RELAY, 1, action);
    }
    pageHeader("Simular dispositivo remoto");
    server.sendContent("<p>Esto genera un comando CAN dirigido al Node-ID remoto configurado.</p><form method='post'><label>Node-ID <input name='target' value='" + String(simulated_target) + "'></label><br><label>Accion 0=OFF 1=ON 2=TOGGLE <input name='action' value='2'></label><br><button>Enviar por CAN</button></form>");
    pageEnd();
}

void setup() {
    IPAddress local_address;
    local_address.fromString(kLocalIp);
    WiFi.config(local_address, IPAddress(192,168,10,1), IPAddress(255,255,255,0));
    WiFi.begin("PCD-LAN", "cambiar-password");
    while (WiFi.status() != WL_CONNECTED) delay(250);

    IPAddress peer_address;
    peer_address.fromString(kPeerIp);
    transport.setPeer(peer_address, kPeerPort);
    transport.begin();
    node.onAnyFrame(forwardFrame, 0);
    node.begin(500000);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/config", HTTP_ANY, handleConfig);
    server.on("/simulate", HTTP_ANY, handleSimulate);
    server.begin();
}

void loop() {
    server.handleClient();
    while (tunnel.available()) {
        uint16_t peer = 0;
        pcd::CanFrame frame;
        if (!tunnel.receiveFrame(peer, frame)) continue;
        if (relay_guard.accept(kSegment == 1 ? 2 : 1, frame, millis())) {
            injecting_remote = true;
            node.handleFrame(frame);
            injecting_remote = false;
            ++received;
        }
    }
    node.poll(millis());
}
