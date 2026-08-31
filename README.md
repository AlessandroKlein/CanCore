# canbus_ecosistema_v5

Ecosistema domótico e industrial **descentralizado** sobre CAN Bus 2.0B (29 bits), según el PRD v5.0 "MASTER".

Este repositorio contiene la **librería de comunicación** (PCD v1): protocolo, capa de abstracción de hardware, capa de nodo, gestión de recursos, persistencia y reglas descentralizadas. Los puentes (MQTT, túnel UDP, Modbus), el servidor web y el OTA se construyen sobre esta base.

Documentación completa en [`docs/wiki`](docs/wiki/README.md).

## Estado

| Módulo | Estado |
| --- | --- |
| `can_protocol` — ID de 29 bits + payload de 8 bytes | implementado y testeado |
| `hal_can` — TWAI (ESP32), MCP2515 (AVR), bus virtual (native) | implementado |
| `can_node` — feedback loop, suscripciones, heartbeat, SDO | implementado y testeado |
| `device_manager` — canales, temporizadores y rampas no bloqueantes | implementado y testeado |
| `config_storage` — EEPROM/NVS/memoria con CRC-16 | implementado y testeado |
| `rule_engine` — reglas de vinculación persistentes + SDO segmentado | implementado y testeado |
| `ota_manager`, servidor web, puentes MQTT/UDP/Modbus, backend STM32 | pendientes |

## Protocolo PCD v1

Identificador extendido de 29 bits:

```
 bits 28..26 (3)   Prioridad   0 crítico · 2 tiempo real · 4 SDO · 6 telemetría · 7 OTA/fondo
 bits 25..21 (5)   Tipo        0x01 heartbeat · 0x02 evento · 0x03 SDO · 0x04 OTA · 0x05 escena · 0x06 estado
 bits 20..14 (7)   Destino     0x01..0x7E · 0x00 broadcast/grupo
 bits 13..0  (14)  Origen      0x0001..0x3FFF
```

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
pio test -e native            # 32 pruebas unitarias del protocolo, del nodo y de las reglas
pio run -e esp32_gateway      # gateway / puente multiprotocolo (TWAI)
pio run -e esp32_hmi          # pantalla táctil CAN
pio run -e atmega2560_node    # nodo de campo de alta densidad (MCP2515)
pio run -e atmega328p_node    # nodo de campo compacto (MCP2515)
```

El entorno `native` usa `VirtualBus`, un bus CAN en memoria que conecta varios `NativeCanBus`, lo que permite
validar el ecosistema completo sin hardware.

## Configuración

`src/system_config.h` concentra las banderas de compilación. Con `-D` en `platformio.ini` se activan
`FEATURE_OTA_MANAGER`, `FEATURE_WEB_SERVER`, `FEATURE_MQTT_BRIDGE`, `FEATURE_MODBUS_BRIDGE` y
`FEATURE_TUNNEL_BRIDGE`. `CAN_LOG_LEVEL=0` elimina de la Flash todas las cadenas de trazas, crítico en AVR.

## Hardware

- Transceptores: SN65HVD230 (3.3 V), MCP2551/TJA1050 (5 V), MAX485/ST485 (RS-485).
- Terminación de 120 Ω conmutable por DIP, habilitada **solo en los dos extremos** del bus.
- TVS bidireccional (PESD1CAN) entre `CAN_H`/`CAN_L` y GND; optoacoplamiento en las etapas de potencia.
