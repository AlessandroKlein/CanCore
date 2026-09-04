# Recursos y protocolos estandar

Referencia de `PCD_CAN` **0.7.0**.

## Catalogo de recursos

Los valores existentes se mantienen compatibles. Los nuevos valores ocupan
rangos libres del byte de recurso y permiten que un nodo anuncie muchos tipos
sin crear un protocolo paralelo.

| Valor | Constante | Semantica sugerida | UI/MQTT |
|---:|---|---|---|
| 0x10 | `RES_RELAY` | salida binaria | switch |
| 0x20 | `RES_DIMMER` | nivel 0..100 | light |
| 0x30 | `RES_DIGITAL_INPUT` | entrada digital | binary_sensor |
| 0x40 | `RES_ENV_SENSOR` | temperatura/ambiente | sensor |
| 0x50 | `RES_GAS_SENSOR` | gas/calidad | sensor |
| 0x60 | `RES_POWER_SENSOR` | potencia | sensor |
| 0x70 | `RES_COVER` | persiana/cortina | cover |
| 0x71 | `RES_BUTTON` | pulsador | binary_sensor |
| 0x72 | `RES_BINARY_SENSOR` | sensor binario | binary_sensor |
| 0x73 | `RES_LIGHT` | luz regulable o direccionable | light |
| 0x74 | `RES_FAN` | ventilador | fan |
| 0x75 | `RES_LOCK` | cerradura | lock |
| 0x76 | `RES_VALVE` | válvula | valve |
| 0x77 | `RES_WATER_LEAK` | fuga de agua | binary_sensor |
| 0x78 | `RES_SMOKE` | humo | binary_sensor |
| 0x80 | `RES_PRESSURE_SENSOR` | presión | sensor |
| 0x81 | `RES_HUMIDITY_SENSOR` | humedad | sensor |
| 0x82 | `RES_CO2_SENSOR` | CO2 | sensor |
| 0x83 | `RES_AIR_QUALITY` | calidad de aire | sensor |
| 0x84 | `RES_GPS` | posición | sensor/device_tracker |
| 0x85 | `RES_VIBRATION_SENSOR` | vibración | sensor |
| 0x90 | `RES_VOLTAGE_SENSOR` | tensión | sensor |
| 0x91 | `RES_CURRENT_SENSOR` | corriente | sensor |
| 0x92 | `RES_FREQUENCY_SENSOR` | frecuencia | sensor |
| 0x93 | `RES_ENERGY_SENSOR` | energía acumulada | sensor |
| 0x94 | `RES_BATTERY` | batería | sensor |
| 0xE0 | `RES_CONFIG` | configuración | interno |
| 0xF0 | `RES_SYSTEM` | sistema/red/OTA | interno |

Para tipos propietarios usar valores no ocupados y documentar el perfil. El
nombre legible se obtiene con `resourceName()`; no se debe enviar texto sin
escapar desde un nodo a una página web.

## CANopen

`CanopenBridge` no implementa el stack CANopen. Traduce una acción canónica a
un PDO mediante `ICanopenTransport::sendPdo()` y acepta comandos externos con
`queueIncoming()`. El proyecto debe mapear COB-ID, PDO, objetos y NMT a sus
reglas CANopen reales.

## NMEA2000

`Nmea2000Bridge` ofrece un PGN de referencia para publicar estados. El proyecto
que use NMEA2000 debe elegir PGNs oficiales, campos, unidades y dirección de
origen según el equipo. No se debe presentar `kPgnPcdState` como PGN marítimo
estándar: es un valor interno de ejemplo.

## KNX

`KnxBridge` usa `IKnxTransport` y un DPT numérico de referencia para demostrar
el flujo de ida y vuelta. El integrador debe reemplazarlo por la dirección de
grupo y DPT real, validando longitud, endianess y escalas.

## Estado de interoperabilidad

Las conversiones y colas se prueban en `native`; la compatibilidad eléctrica,
los COB-ID, PGN, direcciones de grupo y perfiles de fabricante requieren una
prueba física o un simulador oficial. Esta separación mantiene el núcleo sin
librerías de terceros.
