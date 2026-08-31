# Wiki del ecosistema CAN descentralizado (PCD v1)

Documentacion completa del proyecto `canbus_ecosistema_v5`.

| # | Seccion | Contenido |
|---|---------|-----------|
| 1 | [Inicio y arquitectura](01-inicio-y-arquitectura.md) | Filosofia peer-to-peer, protocolo de 29 bits, payload, recursos |
| 2 | [Conexiones fisicas y hardware](02-hardware-y-conexiones.md) | ESP32 + transceptor, MCP2515, topologia, terminacion, proteccion |
| 3 | [Entorno de desarrollo](03-entorno-de-desarrollo.md) | Arduino IDE, PlatformIO, estructura del repo, pruebas |
| 4 | [Referencia de API](04-referencia-api.md) | `CanNode`, `DeviceManager`, `ConfigStore`, `RuleEngine`, HAL |
| 5 | [Enrutamiento, gateways y puentes](05-gateways-y-puentes.md) | MQTT/Home Assistant, tunel UDP/TCP, Modbus (diseno) |
| 6 | [Actualizaciones OTA por CAN](06-ota-por-can.md) | Manifiesto, segmentacion, ventanas, ACK (diseno) |
| 7 | [Diagnostico y FAQ](07-diagnostico-y-faq.md) | 60 ohm, bus-off, error frames, herramientas, preguntas frecuentes |

## Estado de la implementacion

Lo que ya funciona y esta cubierto por pruebas automaticas:

- `can_protocol`: identificador de 29 bits, payload de 8 bytes, constructores de tramas.
- `hal_can`: TWAI (ESP32), MCP2515 propio por SPI (AVR), bus virtual (escritorio).
- `can_node`: lazo comando -> ejecucion -> difusion, suscripciones, escenas, heartbeat.
- `device_manager`: canales heterogeneos, temporizadores y rampas no bloqueantes.
- `config_storage`: EEPROM / NVS / memoria con CRC-16 y recuperacion ante corrupcion.
- `rule_engine`: reglas de vinculacion persistentes y su programacion por SDO segmentado.

Todavia **no** implementado (las secciones 5 y 6 documentan el diseno acordado, no
codigo existente): puentes MQTT, tunel UDP/TCP, Modbus, gestor OTA, bootloader CAN,
servidor web del gateway, backend STM32 y transporte multi-trama de proposito general.
