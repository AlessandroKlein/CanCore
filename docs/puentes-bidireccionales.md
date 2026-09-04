# Puentes bidireccionales y comunicacion descentralizada

PCD_CAN sigue siendo una libreria de protocolo CAN personalizada. El bus CAN es el plano de control descentralizado: cada nodo ejecuta recursos y reglas localmente, y un gateway solo traduce protocolos o conecta segmentos.

## Regla de integracion

`src/` contiene contratos, codec y traducciones sin dependencias externas. Los ejemplos usan dependencias concretas solo para enseñar el adaptador:

```text
protocolo externo <-> adaptador de ejemplo <-> IBridge/ITunnelTransport <-> Gateway <-> CAN
```

No agregar `PubSubClient`, Matter, Zigbee, ESPHome, `Ethernet.h`, `WiFi.h`, Modbus ni TFT al manifiesto de la libreria.

## Direccion doble

| Puente | Entrada externa a CAN | Salida CAN a externo |
|---|---|---|
| MQTT | `.../set` -> `MqttBridge::buildCanonical()` | `MSG_STATE` -> `.../state` |
| Modbus | coil/register escrito -> `queueIncoming()` | estado -> coil/register |
| ESP-NOW | callback de paquete -> comando CAN | estado CAN -> `esp_now_send()` |
| Zigbee | atributo/command cluster -> CanonicalFrame | `MSG_STATE` -> atributo |
| Matter | command cluster -> CanonicalFrame | estado -> atributo Matter |
| ESPHome | API command -> CAN | `MSG_STATE` -> entidad |
| UDP/TCP/Ethernet | datagrama -> `TunnelEngine` -> CAN | CAN -> datagrama |
| TFT/SPI | toque -> `sendCommand()` | suscripcion -> pantalla |

## Configuracion de escucha

- `addListenFilter(source, resource, channel)` restringe estados entregados a las suscripciones.
- `CFG_SUBSCRIBE` permite configurar ese filtro desde otro nodo CAN.
- `CFG_CLEAR_SUBSCRIPTIONS` lo elimina.
- `MSG_DISCOVERY_ANNOUNCE` informa modo de ID y cantidad.
- `MSG_DISCOVERY_RESOURCE` informa el tipo de recurso y canal.
- `Gateway::nodes()` mantiene el inventario para interfaces web y puentes.

## Matter y Zigbee

No hay una API universal suficientemente estable entre todos los cores Arduino. Por eso los ejemplos dejan tres funciones claras: publicar estado, recibir comando y convertir recursos. El proyecto debe mapear sus clusters/endpoints a `RES_RELAY`, `RES_DIMMER`, `RES_COVER` y sensores, sin cambiar el protocolo CAN.

## ESPHome

ESPHome puede conectarse por MQTT, API nativa o un componente externo. La opción se elige en el proyecto. El adaptador debe mantener la regla `comando externo -> CAN -> confirmacion MSG_STATE`; no marcar una entidad como activa antes de recibir el estado confirmado.

## Redes CAN

`TunnelEngine` serializa una trama CAN en 16 bytes con CRC. UDP sirve para baja latencia; TCP o Ethernet con W5500 sirven cuando el proyecto exige entrega fiable. Para mantener descentralización, cada gateway debe aplicar filtros, anti-bucle y una lista de peers, sin convertir el gateway en controlador único.

## Pantallas

Una TFT/SPI puede ser un nodo más. Debe suscribirse solo a los recursos que muestra y enviar comandos dirigidos al Node-ID correspondiente. No hace falta que conozca MQTT, Modbus ni la topología completa.

## Futuras mejoras recomendadas

1. Agregar autenticación y firma de comandos de configuración/OTA.
2. Implementar ventana ACK y bootloader receptor OTA para AVR/ESP32.
3. Añadir TTL/segment-id al túnel para anti-bucle formal.
4. Crear un `NodeRegistry` con expiración de heartbeat y persistencia opcional.
5. Definir perfiles de recursos para Matter/Zigbee/ESPHome sin acoplar SDKs.
6. Agregar pruebas de interoperabilidad con hardware real y fuzzing de frames.
7. Medir consumo de RAM/flash por bridge y permitir `build_flags` para excluir UI.
8. Incorporar un generador de API web a partir de `Gateway::nodes()`.
