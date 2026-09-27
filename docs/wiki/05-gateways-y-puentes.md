# 5. Enrutamiento, gateways y puentes

> **Estado: implementacion base disponible.** El repositorio ya incluye routing,
> gateway, puentes MQTT/Modbus, codec de tunel, transporte UDP de referencia,
> OTA emisor y recursos web. Los adaptadores concretos de red y HTTP siguen
> perteneciendo al proyecto de firmware.

## 5.1 Que es un gateway aqui

Un gateway ESP32 es **un nodo mas** del bus, con Node-ID propio (por convencion
`0x0001`). No arbitra ni orquesta: observa el trafico, lo traduce a otros
protocolos y, a la inversa, inyecta comandos que cualquier nodo podria haber
emitido. Si se apaga, el control en tiempo real sigue funcionando.

## 5.2 Motor de enrutamiento

El nucleo comun de los puentes es una tabla de rutas que asocia un patron de
tramas con un destino externo:

```
(origen, tipo de recurso, canal)  ->  destino externo (topico MQTT, registro Modbus, socket)
```

Regla de diseno: el gateway **no** inventa estado. Publica hacia afuera lo que
llega en `MSG_STATE` y traduce lo que llega de afuera a `MSG_EVENT` dirigido.

`Gateway::nodes()` expone `NodeRegistry`, que se actualiza con los heartbeats y
los anuncios `MSG_DISCOVERY`. Así, al conectar un nuevo elemento el conversor
puede mostrar su Node-ID, modo manual/automático y cada tipo de recurso con su
canal, en vez de presentar solo una dirección numérica.

## 5.3 Regla descentralizada de todos los puentes

Cada nodo conserva el control local, las reglas y la ejecucion de recursos. Un
puente no es un servidor obligatorio: solo observa estados confirmados y
traduce comandos externos a `MSG_EVENT`. Si el puente se apaga, los nodos y sus
automatizaciones siguen funcionando.

El ejemplo de cada tecnologia vive en `examples/` y usa la dependencia externa
solo en ese proyecto:

| Ejemplo | Direccion | Dependencia elegida por el proyecto |
|---|---|---|
| `BridgeMQTTBidireccional` | MQTT <-> CAN | PubSubClient/AsyncMqttClient |
| `BridgeZigbeeBidireccional` | Zigbee <-> CAN | core Zigbee o modulo externo |
| `BridgeMatterBidireccional` | Matter <-> CAN | Matter SDK/core ESP32 |
| `BridgeESPHomeBidireccional` | ESPHome <-> CAN | API nativa o MQTT |
| `BridgeESPNowBidireccional` | ESP-NOW <-> CAN | esp_now.h |
| `CanBusModbusBidireccional` | Modbus <-> CAN | ModbusMaster/ArduinoModbus |
| `CanBusEntreRedes` | CAN <-> UDP/TCP <-> CAN | WiFiUDP/Ethernet/TCP |
| `CanBusEthernetBidireccional` | Ethernet <-> CAN | Ethernet/W5500 |
| `PantallaTFTSPI` | TFT/tactil <-> CAN | TFT_eSPI/LovyanGFX |

## 5.4 Puente CAN <-> MQTT / Home Assistant

Estructura de topicos prevista:

```
pcd/<node_id>/<recurso>/<canal>/state     <- publicado por el gateway
pcd/<node_id>/<recurso>/<canal>/set       <- suscripto por el gateway
pcd/<node_id>/status                      <- online/offline segun heartbeat
```

- `MSG_STATE` recibido -> publicacion retenida en `.../state`.
- Mensaje en `.../set` -> `sendCommand()` dirigido al nodo.
- Ausencia de heartbeat durante N periodos -> `offline` en `.../status` (LWT).
- Descubrimiento automatico de Home Assistant publicando en
  `homeassistant/<componente>/<unique_id>/config`, derivando el componente del
  tipo de recurso (rele -> `switch`, dimmer -> `light`, sensor -> `sensor`,
  cortina -> `cover`).

## 5.5 Puente CAN <-> Ethernet / WiFi (tunel UDP-TCP)

Une dos segmentos CAN fisicamente separados, o expone el bus a un PC.

### Contrato agnostico

Ya existe en la libreria un **codec** CAN-sobre-IP:

- `tunnel/tunnel_engine.h`: `TunnelEngine` que serializa cada trama en un
  datagrama de 16 bytes (magia `0xA5` + ID de 29 bits LSB + DLC + payload +
  CRC-16) y lo deserializa validando magia/DLC/CRC.
- `tunnel/tunnel_transport.h`: `ITunnelTransport` — interfaz de 3 metodos
  (`send`, `receive`, `available`) que la aplicacion implementa con su stack de
  red preferido (WiFiUdp, W5500, LwIP, AsyncTCP...).

Diagrama:

```
[Bus CAN local] -> TunnelEngine -> ITunnelTransport  -> red (lan/WiFi)
[red]           -> ITunnelTransport -> TunnelEngine -> [Bus CAN local]
```

### Responsabilidades del gateway

- **Transporte**: `UdpTunnelTransport` sirve como cola de referencia; para red
  real el usuario implementa `ITunnelTransport` con `WiFiUdp.h`, Ethernet o TCP.
- **Filtro**: se recomienda filtrar por tipo de mensaje y prioridad antes de
  reenviar para no inundar la red con telemetria.
- **Anti-bucle**: cada trama reenviada lleva la marca del segmento de origen y
  no se devuelve a el.
- **UDP** para baja latencia dentro de la LAN; **TCP** cuando hace falta entrega
  fiable.

### Ejemplo minimo

```cpp
#include <PCD_CAN.h>

// ... implementar pcd::ITunnelTransport (ver wiki 8.5) ...
MyUdpTransport transport(8888);
pcd::TunnelEngine tunnel(transport);

void setup() {
    transport.begin();
}

void loop() {
    node.poll(millis());

    // recepcion de la red -> inyectar al bus local
    pcd::CanFrame frame;
    uint16_t from_peer;
    if (tunnel.receiveFrame(from_peer, frame)) {
        node.handleFrame(frame);
    }

    // trafico del bus -> reenviar a la red
    // (se hace dentro de onAnyFrame() con tunnel.sendFrame(peer, frame))
}
```

## 5.6 Puente CAN <-> Modbus RTU / TCP

Para integrar PLCs y SCADA existentes.

- El gateway actua como **esclavo** Modbus y mantiene una imagen de proceso
  actualizada con los `MSG_STATE` del bus.
- Mapa de registros: bloques contiguos por Node-ID, con un desplazamiento fijo
  por tipo de recurso (por ejemplo 16 registros por nodo).
- Coils -> reles; holding registers -> dimmers y consignas; input registers ->
  telemetria.
- Escritura de un coil o registro -> `sendCommand()` hacia el nodo correspondiente.
- Como **maestro** Modbus RTU, el mismo gateway puede leer medidores existentes y
  publicarlos al bus CAN como telemetria de un recurso virtual.

## 5.7 Interfaz web y OTA del gateway

- `web/web_pages.{h,cpp}` entrega una aplicacion HTML con vistas de inventario,
  rutas, OTA, tunel, diagnostico y ajustes.
- `docs/web-gateway.md` define los endpoints JSON y muestra integraciones con
  `WebServer.h` y `ESPAsyncWebServer`.
- El servidor HTTP en el ESP32 debe agregar autenticacion, inventario de nodos
  a partir de heartbeats, edicion de reglas por SDO y disparo de campanas OTA.
- WebSocket para el estado en tiempo real.
- OTA WiFi del propio gateway con `Update.h`, independiente del OTA por CAN de
  la seccion 6.

## 5.8 Punto de enganche disponible hoy

```cpp
void onFrame(const pcd::CanFrame &frame, void *ctx) {
    const pcd::CanId id = frame.fields();
    if (id.msg_type == pcd::MSG_STATE) {
        publishToMqtt(id.source, frame.resource(), frame.channel(),
                      pcd::frameValue(frame));
    }
}

node.onAnyFrame(onFrame, nullptr);
```

## 5.9 Puente CAN <-> Modbus TCP para Loxone

Loxone Config integra hardware de terceros como **maestro Modbus TCP**: el
Miniserver abre una conexion TCP al puerto **502** del gateway y lee/escribe
coils y holding registers. El gateway actua de **esclavo**.

El repositorio ya incluye las dos mitades necesarias:

- `bridge/bridge_modbus.h` (`ModbusBridge`) traduce coils/registers <-> CAN a
  traves del modelo de datos `IModbusRegisterMap`.
- `bridge/bridge_modbus_tcp.h` (`ModbusTcpServer`) es el **servidor** que habla
  el ADU Modbus TCP (cabecera MBAP + PDU) y responde a las consultas del
  maestro. No esta acoplado a ningun stack IP: la aplicacion implementa
  `IModbusTcpStream` con su servidor TCP (WiFiServer, EthernetServer, AsyncTCP,
  sockets...).

```
Loxone Config (maestro) --TCP:502--> ModbusTcpServer --IModbusRegisterMap-->
                                                          ModbusBridge <-> CAN
```

### Funciones Modbus soportadas

| Codigo | Funcion | Uso tipico en Loxone |
|---|---|---|
| 0x01 | Read Coils | leer estado de reles/entradas |
| 0x02 | Read Discrete Inputs | igual, lectura de discretos |
| 0x03 | Read Holding Registers | leer dimmers/consignas/telemetria |
| 0x04 | Read Input Registers | leer sensores |
| 0x05 | Write Single Coil | accionar rele |
| 0x06 | Write Single Register | fijar consigna/dimmer |
| 0x0F | Write Multiple Coils | escenas sobre varios reles |
| 0x10 | Write Multiple Registers | bloque de consignas |

### Esquema de direccionamiento recomendado

```
Coil = slot_nodo * 16 + (canal - 1)          -> reles y entradas digitales
Reg  = 0x0100 + slot_nodo * 16 + (canal - 1) -> holding (dimmer, consigna)
```

`slot_nodo` lo asigna la aplicacion al descubrir el Node-ID (via
`Gateway::nodes()` / `NodeRegistry`). El ejemplo completo vive en
`examples/CanBusModbusBidireccional`.

### Configuracion en Loxone Config

1. Periphery -> "Modbus" -> agregar un esclavo Modbus TCP con la IP del gateway
   y puerto 502 (Unit ID 0xFF o el configurado en `ModbusTcpServer::setUnitId`).
2. Mapear cada coil/registro a un "Virtual Input" (sensor) o "Virtual Output"
   (actuador) segun el esquema de direcciones anterior.
3. Las escrituras de Loxone se convierten en `queueIncoming()` -> comando CAN;
   las lecturas devuelven el ultimo `MSG_STATE` reflejado en el mapa de
   registros por la aplicacion.

