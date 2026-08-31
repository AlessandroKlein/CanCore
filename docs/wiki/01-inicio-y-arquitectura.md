# 1. Inicio y arquitectura general

## 1.1 Filosofia: 100% peer-to-peer

El ecosistema es una red de automatizacion **descentralizada, sin punto unico de
falla**. No existe un orquestador obligatorio: cada nodo conoce sus recursos, sus
reglas y su estado, y los publica al bus.

Consecuencias practicas del modelo:

- Si el gateway se apaga, las teclas siguen encendiendo las luces.
- No hay "polling" central: quien cambia de estado lo difunde.
- Un gateway es un participante mas del bus, util para puentes y para la interfaz
  web, nunca un intermediario obligatorio del control en tiempo real.
- Las reglas viven en el nodo que ejecuta la accion, guardadas en su memoria no
  volatil.

## 1.2 Nodos multifuncion

Un nodo no es "un interruptor": es un conjunto de recursos agrupados bajo un
mismo **Node-ID**. Un nodo de campo puede exponer al mismo tiempo 4 reles, 8
entradas digitales y un sensor de temperatura.

El direccionamiento logico completo es la terna:

```
(Node-ID, Tipo de recurso, Canal)
```

## 1.3 Identificador CAN extendido de 29 bits

La prioridad del mensaje es la prioridad de arbitraje del bus: un ID menor gana
el bus. Por eso los bits mas significativos son la prioridad.

| Bits | Ancho | Campo | Rango |
|-----:|------:|-------|-------|
| 28-26 | 3 | Prioridad | 0 (critico) .. 7 (fondo) |
| 25-21 | 5 | Tipo de mensaje | 0x01 .. 0x1F |
| 20-14 | 7 | Destino | 0x00 broadcast, 0x01..0x7E nodos |
| 13-0 | 14 | Origen | 0x0001 .. 0x3FFF |

```cpp
id  = (priority & 0x07)  << 26;
id |= (msg_type & 0x1F)  << 21;
id |= (target   & 0x7F)  << 14;
id |= (source   & 0x3FFF);
```

### Prioridades

| Valor | Uso |
|------:|-----|
| 0 | Critico: paro de emergencia, alarma de fuego o temperatura |
| 2 | Tiempo real: comandos de control |
| 4 | Configuracion / SDO |
| 6 | Estado y telemetria |
| 7 | OTA y trafico de fondo |

### Tipos de mensaje

| Valor | Nombre | Uso |
|------:|--------|-----|
| 0x01 | `MSG_HEARTBEAT` | Presencia y uptime del nodo |
| 0x02 | `MSG_EVENT` | PDO: comando o evento de entrada |
| 0x03 | `MSG_CONFIG` | SDO: configuracion y vinculaciones |
| 0x04 | `MSG_OTA` | Transferencia de firmware |
| 0x05 | `MSG_GROUP` | Escenas y grupos |
| 0x06 | `MSG_STATE` | Estado / ACK tras ejecutar |

## 1.4 Payload de 8 bytes

| Byte | Contenido |
|-----:|-----------|
| 0 | Tipo de recurso |
| 1 | Indice de canal |
| 2..7 | Accion + parametro, o valor `float` big-endian + banderas |

### Tipos de recurso

| Valor | Recurso |
|------:|---------|
| 0x10 | Rele |
| 0x20 | Dimmer |
| 0x30 | Entrada digital |
| 0x40 | Sensor ambiental |
| 0x50 | Sensor de gas / calidad de aire |
| 0x60 | Sensor electrico |
| 0x70 | Cortina / persiana |
| 0xE0 | Configuracion |
| 0xF0 | Sistema / OTA / red |

### Acciones y eventos

| Accion | Valor | | Evento de entrada | Valor |
|--------|------:|-|-------------------|------:|
| `ACT_OFF` | 0x00 | | `EVT_RELEASED` | 0x10 |
| `ACT_ON` | 0x01 | | `EVT_SHORT_CLICK` | 0x11 |
| `ACT_TOGGLE` | 0x02 | | `EVT_DOUBLE_CLICK` | 0x12 |
| `ACT_SET_VALUE` | 0x03 | | `EVT_LONG_PRESS` | 0x13 |

## 1.5 Lazo de realimentacion

```
Comando dirigido  ->  Ejecucion local  ->  Difusion de estado (broadcast)
```

El nodo que recibe un comando actualiza el hardware y difunde inmediatamente un
`MSG_STATE`. Pantallas, gateways y registradores se sincronizan escuchando esa
difusion; nadie tiene que preguntar el estado de nada.

Los cambios autonomos (fin de un temporizador, fin de una rampa de dimmer)
tambien difunden estado: el lazo se cierra igual que si el cambio hubiera venido
del bus.

## 1.6 Capas del software

```
        sketch / firmware del nodo
    ------------------------------------
     RuleEngine        ConfigStore          <- reglas y persistencia
    ------------------------------------
     CanNode           DeviceManager        <- protocolo y recursos
    ------------------------------------
     ICanBus: Esp32TwaiBus | Mcp2515Bus | NativeCanBus
    ------------------------------------
        TWAI / MCP2515 / bus virtual
```

Ninguna capa superior conoce el hardware: `CanNode` recibe un `ICanBus` ya
construido, y `ConfigStore` recibe un `StorageAdapter`. Eso permite probar el
ecosistema completo en el escritorio con el bus virtual y un backend de memoria.

## 1.7 Restricciones de implementacion

- Sin `malloc` ni `new`: todas las tablas son estaticas y dimensionadas por macros
  (`CAN_MAX_CHANNELS`, `CAN_MAX_RULES`, `CAN_MAX_SUBSCRIPTIONS`).
- Sin `delay()` en la logica de produccion: temporizadores por comparacion de
  marcas de tiempo dentro de `poll()`.
- Tipos de ancho fijo de `<stdint.h>` y serializacion big-endian explicita, para
  que AVR de 8 bits y ESP32 de 32 bits interpreten los mismos bytes.
- C++11, compatible con el compilador de Arduino IDE y con PlatformIO.
