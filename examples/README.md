# Ejemplos PCD_CAN 0.7.0

Todos los ejemplos son sketches de aplicacion. Las librerias de terceros aparecen solo en `examples/` y nunca en `src/` ni en los manifiestos de PCD_CAN.

## Nodos y UI

- `NodoReleBoton`: un rele y un pulsador.
- `NodoMultiRecursos`: dos reles, dos dimmers y cuatro entradas.
- `ReglasPersistentes`: automatizaciones locales persistentes.
- `IdentidadManualAutomatica`: Node-ID manual o derivado de hardware.
- `DescubrimientoYFiltros`: anuncios y filtros de escucha.
- `PantallaTFTSPI`: pantalla TFT/SPI tactil como nodo bidireccional.
- `GatewayWebPCD`: UI web y endpoints del gateway.

## Puentes bidireccionales

- `BridgeMQTTBidireccional`: estados CAN <-> MQTT.
- `BridgeZigbeeBidireccional`: atributos Zigbee <-> CAN.
- `BridgeMatterBidireccional`: clusters Matter <-> CAN.
- `BridgeESPHomeBidireccional`: entidades ESPHome <-> CAN.
- `BridgeESPNowBidireccional`: paquetes ESP-NOW <-> CAN.
- `BridgeCANopenNmeaKnx`: CANopen, NMEA2000 y KNX <-> CAN.
- `CanBusModbusBidireccional`: coils/registers Modbus <-> CAN.
- `CanBusEntreRedes`: dos buses CAN unidos por UDP/TCP.
- `CanBusEthernetBidireccional`: CAN sobre Ethernet/W5500.
- `CanLanBridgeGateway`: dos redes CAN por LAN, paginas por gateway y simulacion remota.

Cada puente debe:

1. descubrir recursos mediante `MSG_DISCOVERY`,
2. configurar filtros con `addListenFilter()` o `CFG_SUBSCRIBE`,
3. traducir eventos externos a comandos CAN,
4. traducir `MSG_STATE` a estados externos,
5. evitar inventar estado cuando el nodo CAN no lo confirmó.

Ver `docs/puentes-bidireccionales.md` y `docs/wiki/09-recursos-y-protocolos-estandar.md`.
