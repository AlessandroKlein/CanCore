# 7. Diagnostico, depuracion y FAQ

## 7.1 La primera medicion: 60 ohm

Con **todo el bus sin alimentacion**, medir con el ohmetro entre `CAN_H` y `CAN_L`
en cualquier punto del tronco:

| Lectura | Diagnostico |
|---------|-------------|
| ~60 ohm | Correcto: dos terminaciones de 120 ohm en paralelo |
| ~120 ohm | Falta una terminacion (o un extremo quedo sin cerrar) |
| ~40 ohm o menos | Sobran terminaciones: hay modulos intermedios con su resistencia soldada |
| Circuito abierto | Cable cortado o conector suelto |
| ~0 ohm | Cortocircuito entre CAN_H y CAN_L |

Es el 80% de las fallas de instalacion.

## 7.2 Tensiones en reposo y en actividad

Con el bus alimentado y sin trafico (estado recesivo), `CAN_H` y `CAN_L` deben
estar ambos cerca de 2.5 V respecto de GND. Durante un bit dominante, `CAN_H`
sube a ~3.5 V y `CAN_L` baja a ~1.5 V.

- Ambas lineas en 0 V: transceptor sin alimentacion o en modo standby (pin `Rs` /
  `S` mal conectado).
- Diferencia permanente: alguna salida trabada dominante, normalmente un nodo
  colgado o un transceptor danado.

## 7.3 Bus-off

Un nodo entra en **bus-off** cuando su contador de errores de transmision supera
127+: el controlador se desconecta solo para no arruinar el bus.

Causas tipicas:

1. Bitrate mal configurado en un nodo (o cristal del MCP2515 mal declarado).
2. Terminacion incorrecta.
3. Nodo solo en el bus: nadie envia el ACK y las retransmisiones acumulan errores.
4. Ruido electrico severo (motores, contactores sin snubber).

La libreria detecta y se recupera sola dentro de `poll()`:

```cpp
if (bus_.isBusOff()) {
    bus_.recover();
}
```

El heartbeat informa el estado de salud del nodo en su byte 2, lo que permite
detectar desde el gateway un nodo que entra y sale de bus-off.

## 7.4 Error frames

Un osciloscopio o un analizador CAN muestra rafagas de 6 bits dominantes
seguidos: son error frames. Interpretacion rapida:

| Sintoma | Causa habitual |
|---------|----------------|
| Error frames continuos desde el arranque | Bitrate distinto entre nodos |
| Errores solo al conmutar un rele | Falta de snubber o masa de potencia compartida |
| Errores al agregar un nodo | Terminacion extra o stub demasiado largo |
| Errores intermitentes por temperatura | Cristal marginal en un MCP2515 clonico |

## 7.5 Herramientas de monitorizacion

- **Analizador USB-CAN** (CANable, PCAN, Kvaser) con `candump` / `cansniffer` en
  Linux mediante SocketCAN.
- **`can-utils`**: `candump can0 -x` muestra IDs extendidos; util para verificar
  la descomposicion de los 29 bits.
- **ESP32 como sniffer**: un nodo con `CAN_LOG_LEVEL=2` y `onAnyFrame()`
  volcando por Serial.
- **Bus virtual del propio proyecto**: `pio test -e native` reproduce escenarios
  completos sin hardware.

Ejemplo de decodificacion manual de un ID `0x08805805`:

```
0x08805805 = 0000 1000 1000 0000 0101 1000 0000 0101
prioridad  = bits 28..26 = 2   (tiempo real)
tipo       = bits 25..21 = 0x02 (MSG_EVENT)
destino    = bits 20..14 = 0x16
origen     = bits 13..0  = 0x0005
```

## 7.6 Preguntas frecuentes

**¿Puedo mezclar nodos de 5 V (AVR) y 3.3 V (ESP32) en el mismo bus?**
Si. El bus CAN es diferencial y aislado logicamente de los niveles del
microcontrolador; lo que debe coincidir es el bitrate. Cada nodo usa el
transceptor adecuado a su tension.

**¿Cuantos nodos soporta el bus?**
Los transceptores tipicos admiten 110 o 112 nodos electricos. El protocolo
direcciona 126 destinos (0x01..0x7E) y 16 383 origenes.

**¿Que pasa si dos nodos tienen el mismo Node-ID?**
Ambos responderan a los mismos comandos y sus difusiones de estado se
confundiran. El Node-ID se persiste en `ConfigStore` y se puede cambiar en
caliente por SDO (`CFG_SET_NODE_ID`).

**¿Que pasa si se corta la energia mientras se guarda una regla?**
El bloque de configuracion se valida con CRC-16: si quedo a medias, `begin()`
devuelve `false` y el nodo arranca con la configuracion por defecto en lugar de
con reglas corruptas.

**¿Funciona la domotica sin el gateway?**
Si, ese es el punto del diseno. Las reglas de vinculacion viven en los nodos.
El gateway solo hace falta para la interfaz web, MQTT, Modbus y OTA.

**¿Por que mi ATmega328P se queda sin RAM?**
Las tablas estaticas se dimensionan por macros. En el entorno `atmega328p_node`
ya estan reducidas a `CAN_MAX_SUBSCRIPTIONS=8`, `CAN_MAX_CHANNELS=8` y
`CAN_MAX_RULES=8`. Cada regla ocupa 20 bytes en flash/RAM segun el uso.

**¿Puedo usar la libreria con otra libreria MCP2515 ya instalada?**
El driver MCP2515 de este proyecto es propio y no depende de ninguna otra
libreria; convivir con otra no da conflicto de compilacion, pero no deben
manejar el mismo chip select simultaneamente.

**¿Como agrego un tipo de recurso nuevo?**
Agregar el valor al enum `ResourceType` (rango libre 0x80..0xDF), registrar el
canal con `registerResource()` e implementar su `ResourceHandler`. El protocolo
no necesita cambios.
