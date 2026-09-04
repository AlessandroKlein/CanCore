# 3. Configuracion del entorno de desarrollo

El repositorio es a la vez un **proyecto PlatformIO** y una **libreria instalable**
para Arduino IDE y para el registro de PlatformIO.

## 3.1 Estructura

```
canbus_ecosistema_v5/
├── library.properties        metadatos para Arduino IDE
├── library.json              metadatos para PlatformIO
├── platformio.ini            entornos de compilacion y pruebas
├── src/
│   ├── PCD_CAN.h             cabecera unica de la libreria
│   ├── can_protocol.{h,cpp}  ID de 29 bits y payload
│   ├── can_node.{h,cpp}      capa de aplicacion del nodo
│   ├── device_manager.{h,cpp} recursos, temporizadores y rampas
│   ├── config_storage.{h,cpp} persistencia con CRC
│   ├── rule_engine.{h,cpp}   reglas de vinculacion y SDO
│   ├── hal_can.h             interfaz ICanBus
│   ├── hal/hal_can_esp32.*   driver TWAI
│   ├── hal/hal_can_mcp2515.* driver MCP2515 por SPI
│   ├── hal/hal_can_native.*  bus virtual para escritorio
│   ├── ota_manager.{h,cpp}  emisor OTA por CAN
│   ├── web/web_pages.{h,cpp} UI HTML agnostica del gateway
│   ├── routing/              tabla y motor de rutas
│   ├── bridge/               bridges MQTT y Modbus
│   ├── gateway/              composicion, registro de nodos y bridges
│   └── main.cpp              firmware de referencia (solo PlatformIO)
├── examples/                 sketches .ino para Arduino IDE
├── test/                     pruebas unitarias Unity
└── docs/wiki/                esta documentacion
```

Las cabeceras viven junto a las fuentes en `src/` porque la especificacion de
librerias de Arduino compila recursivamente esa carpeta y solo agrega `src/` al
include path.

`src/main.cpp` esta encerrado en `#if defined(ARDUINO) && defined(PCD_BUILD_FIRMWARE)`:
PlatformIO define ese simbolo al construir firmware, y al instalar el repositorio
como libreria de Arduino el archivo se compila vacio, sin colisionar con el
`setup()` / `loop()` del sketch del usuario.

## 3.2 Arduino IDE

1. Descargar el repositorio como ZIP.
2. *Sketch -> Incluir libreria -> Anadir biblioteca .ZIP*.
3. Placas necesarias en el gestor de tarjetas: **esp32** (Espressif) o
   **Arduino AVR Boards**.
4. Abrir *Archivo -> Ejemplos -> PCD_CAN -> NodoReleBoton*.

En el sketch alcanza con:

```cpp
#include <PCD_CAN.h>
```

Ejemplos incluidos:

- `NodoReleBoton`: recurso de relé y pulsador local.
- `ReglasPersistentes`: reglas guardadas y programables por CAN.
- `IdentidadManualAutomatica`: modo manual/automático y persistencia.
- `DescubrimientoYFiltros`: anuncios de recursos y filtros de escucha.
- `GatewayWebPCD`: UI web de referencia para ESP32.

`PCD_CAN.h` selecciona el driver segun la plataforma detectada: TWAI en ESP32,
MCP2515 en AVR, bus virtual en escritorio.

## 3.3 PlatformIO

Instalacion:

```bash
pip install --upgrade platformio
```

Entornos definidos en `platformio.ini`:

| Entorno | Plataforma | Uso |
|---------|-----------|-----|
| `native` | escritorio | pruebas unitarias con bus virtual |
| `esp32_gateway` | esp32dev | gateway / puentes multiprotocolo |
| `esp32_hmi` | esp32dev | pantalla tactil |
| `atmega2560_node` | megaatmega2560 | nodo de campo de alta densidad |
| `atmega328p_node` | uno | nodo de campo compacto |

Comandos habituales:

```bash
pio test -e native                 # pruebas unitarias (56 casos)
pio run -e esp32_gateway           # compilar el gateway
pio run -e atmega328p_node -t upload
pio device monitor -b 115200
```

Para usar la libreria desde otro proyecto PlatformIO:

```ini
lib_deps = https://github.com/AlessandroKlein/canbus_ecosistema_v5.git
```

## 3.4 Banderas de compilacion

Definidas en `system_config.h` y sobreescribibles desde `build_flags`:

| Macro | Por defecto | Significado |
|-------|------------:|-------------|
| `CAN_BUS_BITRATE` | 500000 | Velocidad del bus |
| `CAN_HEARTBEAT_PERIOD_MS` | 5000 | Periodo del heartbeat |
| `CAN_LOG_LEVEL` | 1 | 0 silencio, 1 errores/info, 2 depuracion |
| `CAN_MAX_SUBSCRIPTIONS` | 16 | Suscripciones a estados remotos |
| `CAN_MAX_CHANNELS` | = suscripciones | Canales locales del nodo |
| `CAN_MAX_RULES` | 12 | Reglas de vinculacion persistentes |
| `PCD_BUILD_FIRMWARE` | - | Habilita `src/main.cpp` |
| `FEATURE_*` | 0 | Perfiles de gateway; activan integraciones en el firmware |

En ATmega328P conviene reducir las tablas; el entorno `atmega328p_node` ya usa
8 suscripciones, 8 canales y 8 reglas.

## 3.5 Pruebas sin hardware

`NativeCanBus` implementa `ICanBus` contra un `VirtualBus` en memoria: todos los
nodos conectados al mismo `VirtualBus` se escuchan entre si. Permite validar el
lazo de realimentacion, las reglas y los SDO en el escritorio.

```cpp
pcd::VirtualBus wire;
pcd::NativeCanBus bus_a(&wire);
pcd::NativeCanBus bus_b(&wire);

pcd::CanNode sensor(bus_a, 0x0005);
pcd::CanNode actuador(bus_b, 0x0016);
```

## 3.6 Reglas de empaquetado y compatibilidad

- `PCD_CAN.h` es la unica cabecera publica recomendada.
- `library.properties` permite instalar por ZIP en Arduino IDE y no arrastra
   librerias de red ni JSON.
- `library.json` incluye `system`, `tunnel`, `routing`, `bridge`, `gateway`,
   `web` y `hal`, y excluye `main.cpp` como fuente de libreria.
- El codigo comun usa C++11 y no incluye `Arduino.h`; los HAL concretos solo se
   compilan cuando la plataforma correspondiente esta definida.
- La UI web debe reservarse para el gateway ESP32; los nodos AVR no necesitan
   incluirla en su firmware.

La validacion disponible en este entorno es `pio test -e native` y cubre 53
casos. `arduino-cli` no esta instalado aqui, por lo que la compatibilidad con
Arduino IDE se corrobora mediante los metadatos, includes y estructura de
libreria; antes de publicar una release conviene ejecutar un sketch minimo en
el IDE con la placa objetivo.

## 3.7 Consumo actual de recursos

Medido con `pio run` sobre el firmware de referencia:

| Entorno | RAM | Flash |
|---------|----:|------:|
| esp32_gateway | 22 976 B (7.0%) | 290 301 B (22.1%) |
| atmega2560_node | 954 B (11.6%) | 11 836 B (4.7%) |
| atmega328p_node | 582 B (28.4%) | 11 074 B (34.3%) |
