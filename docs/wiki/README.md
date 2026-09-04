# Wiki del ecosistema CAN descentralizado (PCD v1)

Documentacion completa del proyecto `canbus_ecosistema_v5` (version 0.6.0).

| # | Seccion | Contenido |
|---|---------|-----------|
| 1 | [Inicio y arquitectura](01-inicio-y-arquitectura.md) | Filosofia peer-to-peer, protocolo de 29 bits, payload, recursos |
| 2 | [Conexiones fisicas y hardware](02-hardware-y-conexiones.md) | ESP32 + transceptor, MCP2515, topologia, terminacion, proteccion |
| 3 | [Entorno de desarrollo](03-entorno-de-desarrollo.md) | Arduino IDE, PlatformIO, estructura del repo, pruebas |
| 4 | [Referencia de API](04-referencia-api.md) | `CanNode`, `DeviceManager`, `ConfigStore`, `RuleEngine`, HAL |
| 5 | [Enrutamiento, gateways y puentes](05-gateways-y-puentes.md) | routing, MQTT, Modbus, tunel UDP/TCP y web |
| 6 | [Actualizaciones OTA por CAN](06-ota-por-can.md) | emisor OTA, manifiesto, segmentacion, ventanas y ACK |
| 7 | [Diagnostico y FAQ](07-diagnostico-y-faq.md) | 60 ohm, bus-off, error frames, herramientas, preguntas frecuentes |
| 8 | [Librerias companeras recomendadas](08-librerias-recomendadas.md) | Que hace / que NO hace la libreria, Arduino IDE + PlatformIO, WiFiUdp, ESP-NOW, KNX, Modbus, MQTT, Web GUI |
| 9 | [Implementacion web del gateway](../web-gateway.md) | paginas, API HTTP, seguridad y ejemplos |
| 10 | [Guia de implementacion en proyectos](../guia-implementacion-proyectos.md) | PlatformIO, Arduino IDE, nodos y gateways |
| 11 | [Puentes bidireccionales](../puentes-bidireccionales.md) | Matter, Zigbee, MQTT, ESPHome, ESP-NOW, Modbus, Ethernet y TFT |
| 12 | [Futuras mejoras](../../FUTURE_IMPROVEMENTS.md) | seguridad, OTA, interoperabilidad y criterio para 1.0.0 |

## Estado de la implementacion

Lo que ya funciona y esta cubierto por pruebas automaticas:

- `can_protocol`: identificador de 29 bits, payload de 8 bytes, constructores de tramas.
- `hal_can`: TWAI (ESP32), MCP2515 propio por SPI (AVR), bus virtual (escritorio).
- `can_node`: lazo comando -> ejecucion -> difusion, suscripciones, escenas, heartbeat.
- `device_manager`: canales heterogeneos, temporizadores y rampas no bloqueantes.
- `config_storage`: EEPROM / NVS / memoria con CRC-16 y recuperacion ante corrupcion.
- `rule_engine`: reglas de vinculacion persistentes y su programacion por SDO segmentado.

Lo que **si** existe hoy ademas del nucleo:

- `system_api` / `system_logger` / `system_lock`: capa de servicios inyectable
  (reloj, bloqueo, diagnostico) que hace al nucleo 100 % agnostico al framework.
- `core/ring_buffer` + `core/event_loop`: desacople ISR -> main-loop.
- `tunnel`: codificador CAN-sobre-IP (`TunnelEngine`) mas la interfaz
  `ITunnelTransport`; el transporte (UDP/TCP/WiFi/Ethernet) lo implementa la
  aplicacion (ver seccion 8).
- `can_node_tmpl`: plantilla para dimensionar el buffer de RX en compilacion.
- `routing` / `gateway`: tabla de rutas y composicion de bridges.
- `bridge_mqtt` / `bridge_modbus`: puentes con transportes inyectables.
- `udp_tunnel_transport`: cola de datagramas de referencia.
- `ota_manager`: emisor de imagenes OTA por CAN con CRC global.
- `web/web_pages`: UI HTML/CSS/JS agnostica para el gateway.
- `NodeRegistry`: inventario con expiracion por heartbeat.
- `TunnelRelayGuard`: anti-loop y deduplicacion por segmento.
- `MSG_DISCOVERY`: anuncio de identidad, modo de ID y recursos por canal.
- Identidad manual/automatica persistente y filtros de escucha configurables
  por CAN o por la API web del gateway.
- Ejemplos Arduino IDE en `examples/` para nodo, reglas, identidad,
  descubrimiento/filtros, gateway web, puentes bidireccionales y pantallas.
- `CanLanBridgeGateway`: dos redes CAN por LAN con configuracion web en cada gateway.
- `docs/puentes-bidireccionales.md`: matriz de integracion y futuras mejoras.

Todavia queda fuera del nucleo: adaptadores concretos WiFi/Ethernet/MQTT/KNX/
ESP-NOW, bootloader OTA CAN receptor, servidor HTTP y backend STM32. Se
implementan sobre la libreria en proyectos de firmware/gateway separados.
