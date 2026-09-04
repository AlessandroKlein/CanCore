# Guia de implementacion en proyectos

Esta guia separa el nucleo `PCD_CAN` del firmware que lo usa. La libreria contiene protocolo, nodos, persistencia, reglas, routing, bridges agnosticos, OTA emisor y recursos HTML. El proyecto final agrega pines, red, credenciales, almacenamiento y servidor. Version documentada: **0.5.0**.

## 1. Elegir el tipo de proyecto

### Nodo AVR o ESP32 sin gateway

Usar solo `CanNode`, `DeviceManager`, `ConfigStore` y `RuleEngine`. El nodo debe continuar funcionando sin WiFi ni gateway.

```text
mi-nodo/
├── platformio.ini
├── src/main.cpp
└── lib/PCD_CAN/              # solo si se usa copia local
```

### Gateway ESP32

Usar `Gateway`, `RouteTable`, bridges, `TunnelEngine`, `OtaManager` y `webAppHtml()`.

```text
gateway/
├── platformio.ini
├── src/main.cpp
├── include/secrets.h         # fuera del repositorio
└── data/                     # opcional: LittleFS del proyecto
```

No incluir la UI web en nodos AVR salvo que exista una razon concreta: el gateway es el lugar adecuado para HTTP, WiFi y OTA.

## 2. PlatformIO

Dependencia desde Git:

```ini
[env:esp32_gateway]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    https://github.com/AlessandroKlein/canbus_ecosistema_v5.git
build_flags =
    -DPCD_BUILD_FIRMWARE
    -DFEATURE_OTA_MANAGER=1
    -DFEATURE_WEB_SERVER=1
```

Dependencia local:

```ini
lib_deps =
    file://../canbus_ecosistema_v5
```

Validaciones recomendadas:

```text
pio test -e native
pio run -e esp32_gateway
pio run -e esp32_hmi
pio run -e atmega328p_node
pio run -e atmega2560_node
```

El entorno `native` valida comportamiento sin hardware. Los cuatro targets Arduino verifican compilacion cruzada, pero requieren tener instaladas las plataformas PlatformIO correspondientes.

## 3. Arduino IDE

1. Descargar el repositorio como ZIP.
2. Instalarlo con **Sketch > Incluir libreria > Anadir biblioteca .ZIP**.
3. Instalar el core de la placa: ESP32 de Espressif o Arduino AVR Boards.
4. Incluir solo `#include <PCD_CAN.h>`.
5. Configurar `SystemApi` y crear el HAL de la placa.

La raiz publica es `PCD_CAN.h`. `library.properties` declara `includes=PCD_CAN.h`, mientras `src/main.cpp` no aporta `setup()` ni `loop()` al instalar la libreria porque su contenido de firmware esta protegido por `PCD_BUILD_FIRMWARE`.

## 4. Inicializacion comun

```cpp
#include <PCD_CAN.h>

pcd::SystemApi services;

uint32_t clockMs() {
    return static_cast<uint32_t>(millis());
}

void setup() {
    services.millis = clockMs;
    services.delay_ms = [](uint32_t value) { delay(value); };
    pcd::setSystemApi(&services);
}
```

En ESP32 no asignar directamente `millis` si el compilador informa que su retorno es `unsigned long`; usar un wrapper que devuelva `uint32_t`, como en `src/main.cpp`.

## 5. Identidad y descubrimiento

Elegir una identidad manual para instalaciones controladas o automática cuando
la placa expone un valor único:

```cpp
pcd::CanNode node(bus, 0x0016);
node.setAutomaticNodeId(0x12345678UL); // cambia el origen CAN y lo anuncia
node.setNodeIdMode(pcd::NODE_ID_AUTOMATIC);
```

El primer `poll()` emite `MSG_DISCOVERY_ANNOUNCE` y una trama
`MSG_DISCOVERY_RESOURCE` por canal registrado. El gateway debe guardar esos
datos y mostrar tipo de recurso, canal, modo de ID y último heartbeat. Se puede
solicitar un nuevo anuncio con `makeDiscoveryRequest()`.

Los filtros de escucha se pueden definir localmente o recibir por SDO:

```cpp
node.addListenFilter(pcd::kAnySource, pcd::RES_ENV_SENSOR, pcd::kAnyChannel);
```

Los comandos `CFG_SET_NODE_ID`, `CFG_SET_NODE_MODE`, `CFG_SUBSCRIBE` y
`CFG_CLEAR_SUBSCRIPTIONS` permiten administrarlos desde otro nodo o desde el
conversor ESP32/web.

## 6. Nodo CAN

```cpp
pcd::Esp32TwaiBus bus(5, 4);
pcd::CanNode node(bus, 0x0016);

bool relay(uint8_t, uint8_t action, uint32_t, float &value, void *) {
    static bool state = false;
    if (action == pcd::ACT_ON) state = true;
    else if (action == pcd::ACT_OFF) state = false;
    else if (action == pcd::ACT_TOGGLE) state = !state;
    else return false;
    digitalWrite(7, state ? HIGH : LOW);
    value = state ? 1.0f : 0.0f;
    return true;
}

void setup() {
    // configurar SystemApi antes de crear actividad del nodo
    node.begin(500000);
    node.registerResource(pcd::RES_RELAY, 1, relay);
}

void loop() {
    node.poll(millis());
}
```

En AVR cambiar el HAL por `Mcp2515Bus(cs, pcd::MCP_CLOCK_16MHZ, intPin)` y reservar los dos extremos del bus con terminacion de 120 ohm.

## 7. Gateway y web

```cpp
#include <PCD_CAN.h>
#include <WebServer.h>

WebServer http(80);

void setupWeb() {
    http.on("/", HTTP_GET, []() {
        http.send(200, "text/html; charset=utf-8", pcd::webAppHtml());
    });
    http.on("/api/summary", HTTP_GET, []() {
        http.send(200, "application/json", "{\"nodes\":1,\"frames_per_minute\":0,\"can_status\":\"ok\"}");
    });
    http.begin();
}

void loopWeb() {
    http.handleClient();
}
```

La UI de `webAppHtml()` presenta estas situaciones: resumen operativo, inventario de nodos, tabla de rutas, OTA por CAN, tunel IP, diagnostico y ajustes. El backend debe implementar los endpoints definidos en `docs/web-gateway.md` y autenticar todas las rutas de escritura.

## 8. Puentes bidireccionales

Los sketches de `examples/` muestran el patron para MQTT, Matter, Zigbee,
ESPHome, ESP-NOW, Modbus, Ethernet, tunel entre redes y pantallas TFT/SPI.
Cada uno tiene dos flujos:

```text
CAN MSG_STATE  -> adaptador -> estado del protocolo externo
comando externo -> adaptador -> CAN MSG_EVENT -> estado confirmado
```

El adaptador debe solicitar/recibir `MSG_DISCOVERY`, configurar filtros con
`addListenFilter()` o `CFG_SUBSCRIBE` y nunca reportar un estado externo como
confirmado antes de recibir el `MSG_STATE` del nodo. Las dependencias externas
se agregan únicamente al proyecto del ejemplo, nunca a `library.json` ni a
`library.properties`.

| Familia | Sketch | Dependencia típica |
|---|---|---|
| MQTT | `BridgeMQTTBidireccional` | PubSubClient / AsyncMqttClient |
| Matter | `BridgeMatterBidireccional` | Matter SDK/core |
| Zigbee | `BridgeZigbeeBidireccional` | core Zigbee o módulo |
| ESPHome | `BridgeESPHomeBidireccional` | API nativa o MQTT |
| ESP-NOW | `BridgeESPNowBidireccional` | esp_now.h |
| Modbus | `CanBusModbusBidireccional` | ModbusMaster / ArduinoModbus |
| Ethernet/IP | `CanBusEntreRedes`, `CanBusEthernetBidireccional` | WiFiUDP / Ethernet / TCP |
| HMI | `PantallaTFTSPI` | TFT_eSPI / LovyanGFX |

## 9. Reglas de seguridad

- No guardar contrasenas, tokens ni claves dentro de `web_pages.cpp`.
- No exponer el gateway fuera de la LAN sin autenticacion y TLS.
- Validar Node-ID, recurso, canal, tamaño de imagen y CRC antes de actuar.
- Mantener una campaña OTA por nodo o grupo pequeño; no saturar el bus.
- No ejecutar trabajo pesado dentro de callbacks de ISR o recepcion CAN.
- No enviar tramas reenviadas de vuelta al mismo segmento sin anti-bucle.

## 10. Checklist de entrega

- [ ] `pio test -e native` pasa.
- [ ] Compila el target de la placa final.
- [ ] El sketch incluye solo `PCD_CAN.h`.
- [ ] El `SystemApi` se configura antes de `CanNode::begin()`.
- [ ] La terminacion CAN existe solo en los extremos.
- [ ] Credenciales y secretos estan fuera de Git.
- [ ] El servidor web exige autenticacion para cambios.
- [ ] OTA valida tamaño, destino y CRC.
- [ ] Se prueba perdida de red y reinicio del gateway.
- [ ] Se registra el consumo de RAM y flash de la release.
