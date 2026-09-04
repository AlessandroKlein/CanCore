# canbus_ecosistema_v5

Version actual: **0.4.0**.

Ecosistema domótico e industrial **descentralizado** sobre CAN Bus 2.0B (29 bits), según el PRD v5.0 "MASTER".

Este repositorio contiene la **librería de comunicación** (PCD v1): protocolo, capa de abstracción de hardware, capa de nodo, gestión de recursos, persistencia y reglas descentralizadas. Los puentes (MQTT, túnel UDP, Modbus), el servidor web y el OTA se construyen sobre esta base.

Documentación completa en [`docs/wiki`](docs/wiki/README.md), con la guía de
implementación en [`docs/guia-implementacion-proyectos.md`](docs/guia-implementacion-proyectos.md)
y la integración web en [`docs/web-gateway.md`](docs/web-gateway.md).

## Estado

| Módulo | Estado |
| --- | --- |
| `can_protocol` — ID de 29 bits + payload de 8 bytes | implementado y testeado |
| `hal_can` — TWAI (ESP32), MCP2515 (AVR), bus virtual (native) | implementado |
| `can_node` — feedback loop, suscripciones, heartbeat, SDO | implementado y testeado |
| `device_manager` — canales, temporizadores y rampas no bloqueantes | implementado y testeado |
| `config_storage` — EEPROM/NVS/memoria con CRC-16 | implementado y testeado |
| `rule_engine` — reglas de vinculación persistentes + SDO segmentado | implementado y testeado |
| `system_api` — capa de servicios inyectable (reloj, bloqueo, logger) | implementado |
| `system_logger` — motor de registro con sumidero configurable | implementado |
| `system_lock` — LockGuard (cli/sei, RTOS o inyectado) | implementado |
| `core/ring_buffer` — búfer SPSC lock-free para ISR | implementado |
| `core/event_loop` — desacople ISR → main-loop | implementado |
| `tunnel` — códec CAN-sobre-IP agnóstico (`TunnelEngine` + `ITunnelTransport`) | implementado |
| `can_node_tmpl` — plantilla con búfer de RX en tiempo de compilación | implementado |
| `routing` / `gateway` — tabla y composición de bridges | implementado y testeado |
| `bridge_mqtt` / `bridge_modbus` | implementado y testeado |
| `udp_tunnel_transport` | implementado y testeado |
| `ota_manager` — emisor OTA por CAN | implementado y testeado |
| `web/web_pages` — UI HTML agnóstica para gateway | implementado y testeado |
| identidad manual/automática, descubrimiento y filtros CAN | implementado y testeado |
| `gateway/node_registry` — inventario de nodos y recursos | implementado y testeado |
| bootloader OTA receptor, HTTP concreto y backend STM32 | pendientes en proyectos de firmware |

## Protocolo PCD v1

Identificador extendido de 29 bits:

```
 bits 28..26 (3)   Prioridad   0 crítico · 2 tiempo real · 4 SDO · 6 telemetría · 7 OTA/fondo
 bits 25..21 (5)   Tipo        0x01 heartbeat · 0x02 evento · 0x03 SDO · 0x04 OTA · 0x05 escena · 0x06 estado
 bits 20..14 (7)   Destino     0x01..0x7E · 0x00 broadcast/grupo
 bits 13..0  (14)  Origen      0x0001..0x3FFF
```

Los nodos anuncian su identidad y recursos con `MSG_DISCOVERY` al comenzar a
procesar el bus. El gateway puede pedir el anuncio nuevamente y mostrar, por
ejemplo, `RES_RELAY / canal 1` o `RES_ENV_SENSOR / canal 2`. El ID puede ser
manual o derivarse automáticamente con `deriveAutomaticNodeId()`; el modo se
persiste y también se puede cambiar por SDO.

Payload de 8 bytes:

```
 byte 0      Tipo de recurso  0x10 relé · 0x20 dimmer · 0x30 entrada · 0x40 ambiental ·
                              0x50 gas · 0x60 eléctrico · 0x70 cortina · 0xE0 config · 0xF0 sistema
 byte 1      Índice de canal dentro del nodo
 bytes 2..7  Comando: acción (byte 2) + parámetro BE (bytes 3..6)
             Estado : float IEEE-754 big-endian (bytes 2..5) + flags de diagnóstico (bytes 6..7)
```

El lazo determinista **comando → ejecución → difusión de estado** lo implementa `CanNode`: al recibir un
comando dirigido ejecuta el recurso local y difunde de inmediato una trama `0x06` en broadcast, de modo que
pantallas, gateways y registradores quedan sincronizados. Una pulsación de una tecla cableada al nodo produce
exactamente la misma difusión (`applyLocal`).

## Uso

La librería es instalable tanto desde el Arduino IDE (`library.properties`) como desde PlatformIO
(`library.json`); en ambos casos alcanza con incluir `<PCD_CAN.h>`.

```cpp
#include <PCD_CAN.h>

pcd::Esp32TwaiBus bus(/*tx=*/5, /*rx=*/4);   // en AVR: pcd::Mcp2515Bus bus(10);
pcd::CanNode node(bus, 0x0016);

bool relay(uint8_t ch, uint8_t action, uint32_t param, float &value, void *ctx) {
    // conmuta el hardware y devuelve el estado resultante
    value = (action == pcd::ACT_ON) ? 1.0f : 0.0f;
    return true;
}

void setup() {
    node.begin(500000);
    node.registerResource(pcd::RES_RELAY, 0x01, relay);
}

void loop() {
    node.poll(millis());       // recibe, despacha y emite heartbeat
}
```

Una pantalla o gateway se suscribe a estados remotos:

```cpp
node.subscribe(0x0016, pcd::RES_RELAY, 0x01, onRelayState);
node.sendCommand(0x16, pcd::RES_RELAY, 0x01, pcd::ACT_TOGGLE);
```

## Entornos de compilación

```bash
pio test -e native            # suite unitaria completa (53 casos)
pio run -e esp32_gateway      # gateway / puente multiprotocolo (TWAI)
pio run -e esp32_hmi          # pantalla táctil CAN
pio run -e atmega2560_node    # nodo de campo de alta densidad (MCP2515)
pio run -e atmega328p_node    # nodo de campo compacto (MCP2515)
```

Ejemplos incluidos:

- `examples/NodoReleBoton`: nodo mínimo con relé y pulsador.
- `examples/ReglasPersistentes`: reglas locales persistentes.
- `examples/IdentidadManualAutomatica`: selección de ID manual/automático.
- `examples/DescubrimientoYFiltros`: anuncio de recursos y filtros de escucha.
- `examples/GatewayWebPCD`: servidor web ESP32 y UI de referencia.

El entorno `native` usa `VirtualBus`, un bus CAN en memoria que conecta varios `NativeCanBus`, lo que permite
validar el ecosistema completo sin hardware.

## Arquitectura agnóstica al framework

El núcleo de la librería **no incluye `<Arduino.h>`**. Cada nodo, driver y el protocolo obtienen el reloj,
el bloqueo y el diagnóstico desde un `SystemApi` inyectado en `setup()`:

```cpp
#include <PCD_CAN.h>

void logSink(uint8_t level, const char *format, va_list args) {
    char buf[128];
    vsnprintf(buf, sizeof(buf), format, args);
    Serial.println(buf);          // UART, WebSocket, SD, red...
}

static pcd::SystemApi api;

void setup() {
    api.millis = millis;
    api.delay_ms = [](uint32_t ms) { delay(ms); };
    api.log = logSink;
    api.lock = []() { portENTER_CRITICAL(&spinlock); };  // opcional en RTOS
    api.unlock = []() { portEXIT_CRITICAL(&spinlock); };
    pcd::setSystemApi(&api);
}
```

Con esto el mismo código compila en **Arduino, ESP-IDF, STM32Cube, FreeRTOS o un simulador nativo**. Los macros
`LOG_ERROR`/`LOG_INFO`/`LOG_DEBUG` se enrutan al sumidero configurado y desaparecen por completo con
`CAN_LOG_LEVEL=0`.

### Bucle de eventos asíncrono

Separación estricta entre la ISR y el procesamiento:

```
ISR     -> eventLoop.enqueue(frame)       (~µs, solo copia a un ring buffer SPSC)
Main    -> eventLoop.tick()  o CanNodeTmpl::poll(now_ms)   (despacha fuera de interrupción)
```

`CanNodeTmpl<RxDepth>` usa una plantilla para dimensionar el búfer de RX en tiempo de compilación:

```cpp
CanNodeTmpl<4>  node(bus, 0x0016);   // ATmega328P: 4 tramas de RX
CanNodeTmpl<64> gateway(bus, 0x0001); // ESP32 gateway: 64 tramas de RX
```

### Túnel CAN sobre IP (Data Stream Transport)

La librería **no compila ningún stack IP**. `ITunnelTransport` entrega/recibe datagramas crudos; el usuario
conecta su red (WiFi, Ethernet W5500, LwIP...) y el `TunnelEngine` empaqueta cada trama en 16 bytes:

```
Red del Usuario --bytes--> TunnelEngine --> Bus CAN
```

La librería incluye `UdpTunnelTransport` como cola de referencia para pruebas,
pero no acopla `WiFiUdp.h` ni otro driver de red. El proyecto de gateway adapta
el stack elegido a `ITunnelTransport` (ver `docs/wiki/08-librerias-recomendadas.md`):

```cpp
#include <PCD_CAN.h>

// ... implementar pcd::ITunnelTransport con WiFiUdp, Ethernet o TCP ...
MyUdpTransport transport;
pcd::TunnelEngine tunnel(transport);

transport.begin();
tunnel.sendFrame(1, frame);   // enruta al bus remoto
```

### OTA y web del gateway

`OtaManager` genera bloques compatibles con `makeOtaData()` y calcula el CRC-16
global. `webAppHtml()` entrega una UI de referencia con vistas de resumen,
nodos, rutas, OTA, túnel, diagnóstico y ajustes. El servidor HTTP, el filesystem
y la autenticación son responsabilidad del proyecto ESP32; ver
`docs/web-gateway.md`.

### Configuración

`src/system_config.h` concentra las banderas de compilación. Con `-D` en `platformio.ini` se activan
`FEATURE_OTA_MANAGER`, `FEATURE_WEB_SERVER`, `FEATURE_MQTT_BRIDGE`, `FEATURE_MODBUS_BRIDGE` y
`FEATURE_TUNNEL_BRIDGE`. `CAN_LOG_LEVEL=0` elimina de la Flash todas las cadenas de trazas, crítico en AVR.

## Hardware

- Transceptores: SN65HVD230 (3.3 V), MCP2551/TJA1050 (5 V), MAX485/ST485 (RS-485).
- Terminación de 120 Ω conmutable por DIP, habilitada **solo en los dos extremos** del bus.
- TVS bidireccional (PESD1CAN) entre `CAN_H`/`CAN_L` y GND; optoacoplamiento en las etapas de potencia.
