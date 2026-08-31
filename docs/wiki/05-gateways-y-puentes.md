# 5. Enrutamiento, gateways y puentes

> **Estado: diseno.** Los puentes descritos en esta seccion todavia **no estan
> implementados** en el repositorio. Se documenta el contrato acordado para que
> la implementacion no cambie el protocolo del bus. Lo que si existe hoy es
> `CanNode::onAnyFrame()`, el punto de enganche sobre el que se construiran.

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

## 5.3 Puente CAN <-> MQTT / Home Assistant

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

## 5.4 Puente CAN <-> Ethernet / WiFi (tunel UDP-TCP)

Une dos segmentos CAN fisicamente separados, o expone el bus a un PC.

- Encapsulado propuesto: `[id 4 bytes BE][dlc 1][data 0..8]`, un datagrama por
  trama.
- UDP para baja latencia dentro de la LAN; TCP cuando hace falta entrega fiable.
- Filtro por tipo de mensaje y por prioridad para no inundar la red con
  telemetria.
- Proteccion contra bucles: cada trama reenviada lleva la marca del segmento de
  origen y no se devuelve a el.

## 5.5 Puente CAN <-> Modbus RTU / TCP

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

## 5.6 Interfaz web y OTA del gateway

- Servidor HTTP en el ESP32 para inventario de nodos (a partir de heartbeats),
  estado en vivo, edicion de reglas por SDO y disparo de campanas OTA.
- WebSocket para el estado en tiempo real.
- OTA WiFi del propio gateway con `Update.h`, independiente del OTA por CAN de
  la seccion 6.

## 5.7 Punto de enganche disponible hoy

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
