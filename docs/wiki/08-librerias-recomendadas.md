# 8. Librerías compañeras recomendadas y límites del núcleo

> **Regla de oro del repositorio.** `PCD_CAN` es una **librería de protocolo**, no
> una solución integrada con una red concreta. El núcleo **no** incluye ni
> `WiFiUdp.h`, ni `PubSubClient.h`, ni `esp_now.h`, ni `knx.h`, ni
> `ModbusMaster.h`, ni `AsyncWebServer.h`. Todas esas dependencias son **de la
> aplicación/gateway que usa** la librería. Este documento lista qué librerías
> se recomiendan para cada puente y cómo conectarlas.

---

## 8.1 ¿Qué hace PCD_CAN?

- Protocolo PCD v1 sobre CAN 2.0B (ID extendido de 29 bits + payload de 8 bytes).
- Capa de nodo (`CanNode`), gestor de recursos (`DeviceManager`), persistencia
  (`ConfigStore`) y motor de reglas (`RuleEngine`).
- HAL agnóstica de hardware: `ICanBus` con implementaciones para **TWAI
  (ESP32)**, **MCP2515 (AVR)** y **bus virtual (nativo)**.
- Capa de servicios inyectable (`SystemApi`): la aplicación provee
  `millis()`, `delay()`, `log()`, `lock()/unlock()`. Esto hace al núcleo 100 %
  portátil a ESP-IDF, FreeRTOS, STM32Cube, o un programa de escritorio.
- Motor de túnel CAN-sobre-IP **agnóstico** (`TunnelEngine`): serializa tramas
  a datagramas de 16 bytes sin conocer el stack de red. El transporte
  (UDP/TCP/WiFi/Ethernet) lo implementa la aplicación.

## 8.2 ¿Qué NO hace PCD_CAN?

- **No implementa WiFi ni Ethernet.** Solo entrega/recibe bytes crudos a través
  de `ITunnelTransport`.
- **No incluye ningún driver de red de terceros.** Para el túnel UDP, la
  aplicación debe implementar `ITunnelTransport` usando su librería de red
  preferida.
- **No es un servidor HTTP ni un broker MQTT.** El gateway aporta el servidor,
  autenticacion, filesystem y cliente de red.
- Incluye contratos y bridges MQTT/Modbus agnosticos, pero no incluye
  `PubSubClient`, `ArduinoModbus`, ESP-NOW ni KNX. Esas dependencias y sus
  adaptadores concretos viven en el firmware del gateway.

---

## 8.3 Instalación en Arduino IDE y PlatformIO

### Arduino IDE

1. **No** instalar este repositorio como una librería que arrastra dependencias
   de red. Solo instalar `PCD_CAN`.
2. En el **Library Manager** (o copiando `src/` a
   `Documents/Arduino/libraries/PCD_CAN/`).
3. Incluir `#include <PCD_CAN.h>` en el sketch.
4. Para CAN físico:
   - ESP32 → `Esp32TwaiBus(txPin, rxPin)`.
   - AVR + MCP2515 → `Mcp2515Bus(csPin, MCP_CLOCK_16MHZ, intPin)`.
5. Configurar el `SystemApi` en `setup()` (ver `docs/wiki/01-inicio-y-arquitectura.md`).

### PlatformIO

En `platformio.ini`:

```ini
lib_deps =
    https://github.com/AlessandroKlein/canbus_ecosistema_v5.git
```

O si se instala localmente:

```ini
lib_deps =
    file://../canbus_ecosistema_v5
```

Entornos típicos:

- `esp32_gateway` — plataforma `espressif32`, framework `arduino`.
- `atmega328p_node` — plataforma `atmelavr`, framework `arduino`.
- `native` — plataforma `native`, para pruebas unitarias con Unity.

---

## 8.4 Librerías recomendadas por puente

| Puente / Necesidad | Librería recomendada | Cómo se conecta |
| ------------------ | -------------------- | --------------- |
| **Túnel UDP (P2P Gateways)** | `WiFiUdp.h` (viene con Arduino ESP32 core) o `AsyncUDP` | Implementar `ITunnelTransport`: `send()` hace `udp.beginPacket()/write()/endPacket()`; `receive()` hace `udp.parsePacket()/read()`. |
| **Túnel TCP** | `WiFiClient.h`, `AsyncTCP` | Mismo contrato `ITunnelTransport`, pero con stream TCP. |
| **MQTT / Home Assistant** | `PubSubClient` (Nick O'Leary) o `AsyncMqttClient` | En `onAnyFrame()` publicar `MSG_STATE` a `pcd/<node>/<res>/<ch>/state`; al recibir `.../set`, llamar `sendCommand()`. |
| **ESP-NOW** | `esp_now.h` (viene con el core) + `esp_wifi.h` | En `esp_now_send_cb/esp_now_recv_cb` encapsular la trama PCD v1 (ID + payload) en el paquete de 250 bytes. |
| **Modbus RTU** | `ModbusMaster` (por SPI/serial) o `SimpleModbus` | Mantener imagen de proceso con `MSG_STATE`; escribir coil/reg → `sendCommand()`. |
| **Modbus TCP** | `ArduinoModbus` (Arduino) o `ModbusIP_ESP32` | Igual que RTU pero sobre TCP. |
| **KNX TP1** | `eremmel/knx` (TP-UART) o manejo directo del UART | Traducir `GroupValueWrite` ↔ trama CAN (`MSG_STATE` / `MSG_EVENT`). |
| **KNX IP** | `knx-ip` o datagramas UDP multicast manuales | Mismo mapeo de Direcciones de Grupo. |
| **Web GUI + Sniffer** | `AsyncWebServer` + `WebSockets` (ESP32Async) o `WebServer.h` | Servir `pcd::webAppHtml()` y exponer la API de `docs/web-gateway.md`. |
| **Almacenamiento de rutas** | `LittleFS` (ESP32) | Guardar la tabla de rutas/mapeo como JSON en el sistema de archivos. |
| **Persistencia de configuración** | `Preferences` (NVS) — ya usado por `NvsStorage` | `ConfigStore` + `NvsStorage` ya viene en la librería. |
| **Autenticación de la GUI** | `SimpleOTA` o credenciales en `AsyncWebServer` | Recomendado para no exponer la LAN. |
| **OTA del gateway (WiFi)** | `Update.h` (core ESP32) | Independiente del OTA por CAN de `docs/wiki/06-ota-por-can.md`. |

---

## 8.5 Ejemplo: implementar `ITunnelTransport` con `WiFiUdp.h`

La librería **no** trae `tunnel_udp_esp32.h` (eso era un acoplamiento indebido).
En su lugar, el usuario escribe su propio transporte —son ~50 líneas—:

```cpp
#include <PCD_CAN.h>
#include <WiFiUdp.h>

class MyUdpTransport : public pcd::ITunnelTransport {
public:
    MyUdpTransport(uint16_t listenPort) : udp_(), port_(listenPort),
        peerId_(1), peerIp_(IPAddress(192, 168, 1, 50)), peerPort_(8888) {}

    bool begin()  { return udp_.begin(port_) != 0; }

    bool send(uint16_t peer, const uint8_t *data, size_t len) override {
        if (peer == pcd::kBroadcastPeer) {
            bool ok = true;
            ok &= udp_.beginPacket(peerIp_, peerPort_) != 0;
            ok &= udp_.write(data, len) == (int)len;
            ok &= udp_.endPacket() != 0;
            return ok;
        }
        return false;
    }

    bool receive(uint16_t &peerOut, uint8_t *data, size_t maxLen, size_t &len) override {
        if (udp_.parsePacket() <= 0) return false;
        len = udp_.read(data, maxLen);
        peerOut = peerId_;
        return true;
    }

    bool available() const override { return udp_.available() > 0; }

private:
    WiFiUDP udp_;
    uint16_t port_;
    uint16_t peerId_;
    IPAddress peerIp_;
    uint16_t peerPort_;
};

MyUdpTransport transport(8888);
pcd::TunnelEngine tunnel(transport);

// en setup():
transport.begin();
// en loop():
node.poll(millis());

pcd::CanFrame frame;
uint16_t from;
if (tunnel.receiveFrame(from, frame)) {
    // Inyectar al bus local
    node.handleFrame(frame);
}
```

El **códec** (`TunnelEngine`) está en `src/tunnel/tunnel_engine.h` y es 100 %
agnóstico: no necesita `WiFiUdp.h`. Solo se conecta a la interfaz
`ITunnelTransport`.

---

## 8.6 Contra-acoplamiento: por qué la librería NO incluye drivers de red

- **Separación de responsabilidades**: `PCD_CAN` se ocupa del protocolo y del
  hardware CAN; la red (WiFi/Ethernet/ESP-NOW/KNX/Modbus) es un problema de la
  aplicación.
- **Cada usuario elige su stack preferido**: alguien con ENC28J60 +
  Ethernet no quiere arrastrar `WiFi.h`; alguien con LwIP no quiere depender de
  `Arduino.h` en el núcleo.
- **Pruebas de escritorio**: `ITunnelTransport` se puede implementar con un
  `MockTransport` (como en `test/test_system_tunnel`) y correr en `native`.
- **Sin `malloc` ni dependencias dinámicas**: el núcleo usa solo `stdint.h`,
  `stddef.h`, `stdarg.h` y `string.h` en los `.cpp`, con `-std=c++11`.

---

## 8.7 Roadmap de extensiones de proyecto (no viven en esta librería)

Según el PRD v0.3.0, los siguientes módulos se implementan en **repositorios o
proyectos de firmware separados** que **usan** `PCD_CAN`:

- `bridge_espnow` — CAN ↔ ESP-NOW (extensión inalámbrica del bus).
- `bridge_knx` — CAN ↔ KNX (TP1 / IP), con mapeo DPT.
- Adaptador Modbus concreto — conecta `bridge_modbus` con RTU/TCP.
- Adaptador MQTT concreto — conecta `bridge_mqtt` con el cliente MQTT elegido.
- `bridge_ip_tunnel` — conecta `TunnelEngine` con UDP/TCP P2P.
- `web_server` — sirve `webAppHtml()` y sus endpoints con el servidor HTTP elegido.

Todos comparten el mismo punto de anclaje: `CanNode::onAnyFrame()` para observar
tramas, y `CanNode::sendCommand()` / `applyLocal()` para inyectar comandos.
