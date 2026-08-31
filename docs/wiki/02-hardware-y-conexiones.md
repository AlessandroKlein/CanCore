# 2. Conexiones fisicas y hardware

## 2.1 El bus CAN es un par diferencial

CAN transmite por diferencia de tension entre `CAN_H` y `CAN_L`. Esa es la razon
de su inmunidad al ruido y de casi todas las reglas de cableado que siguen.

Requisitos no negociables:

- **Par trenzado** de impedancia caracteristica ~120 ohm.
- **Topologia lineal** (bus/daisy chain), nunca estrella ni arbol.
- **Dos** resistencias de terminacion de 120 ohm, una en cada extremo fisico.
- Derivaciones (stubs) lo mas cortas posible: menos de 30 cm a 500 kbps.
- Masa de referencia comun entre nodos (tercer conductor), aunque las masas de
  potencia esten separadas.

### Longitud contra velocidad

| Velocidad | Longitud maxima aproximada |
|----------:|---------------------------|
| 1 Mbps | 25 m |
| 500 kbps | 100 m |
| 250 kbps | 250 m |
| 125 kbps | 500 m |
| 100 kbps | 600 m |

El proyecto usa 500 kbps por defecto (`CAN_BUS_BITRATE`), buen compromiso para
una instalacion domotica o de tablero industrial.

## 2.2 ESP32 + transceptor CAN

El ESP32 integra el controlador **TWAI** (compatible con CAN 2.0B), pero **no**
el transceptor: hace falta un SN65HVD230, TJA1050, MCP2551 o similar.

| ESP32 | Transceptor | Nota |
|-------|-------------|------|
| GPIO 5 (`kCanTxPin`) | TXD | Configurable en el sketch |
| GPIO 4 (`kCanRxPin`) | RXD | Configurable en el sketch |
| 3V3 | VCC (SN65HVD230) | El SN65HVD230 es de 3.3 V |
| GND | GND | Referencia comun |

Con TJA1050 o MCP2551 (5 V) hay que adaptar niveles en la linea RX hacia el
ESP32: divisor resistivo o un transceptor de 3.3 V. El SN65HVD230 evita el
problema y es la opcion recomendada.

## 2.3 ATmega328P (Uno / Nano) + MCP2515

El AVR no tiene controlador CAN: se usa un modulo MCP2515 + TJA1050 por SPI.

| Arduino Uno / Nano | MCP2515 |
|--------------------|---------|
| D13 | SCK |
| D12 | SO (MISO) |
| D11 | SI (MOSI) |
| D10 | CS (`kMcpCsPin`) |
| D2 | INT (opcional, `kMcpIntPin`) |
| 5V | VCC |
| GND | GND |

## 2.4 ATmega2560 (Mega) + MCP2515

El Mega tiene el SPI en otros pines:

| Arduino Mega | MCP2515 |
|--------------|---------|
| D52 | SCK |
| D50 | SO (MISO) |
| D51 | SI (MOSI) |
| D53 (o el que se elija) | CS |
| D2 | INT (opcional) |
| 5V | VCC |
| GND | GND |

## 2.5 Cristal del MCP2515: 8 MHz contra 16 MHz

Es el error mas frecuente en modulos chinos. El bit timing depende del cristal
soldado en la placa; si se declara mal, el nodo no recibe nada o genera error
frames continuos.

```cpp
pcd::Mcp2515Bus bus(10, pcd::MCP_CLOCK_16MHZ, 2);  // cristal de 16 MHz
pcd::Mcp2515Bus bus(10, pcd::MCP_CLOCK_8MHZ, 2);   // cristal de 8 MHz
```

Verificar visualmente el encapsulado metalico del modulo: suele decir `16.000`
o `8.000`.

## 2.6 Terminacion

- Exactamente **dos** resistencias de 120 ohm, en los dos extremos del tronco.
- Los modulos MCP2515 comerciales traen una de 120 ohm soldada (a veces con
  jumper). Hay que **quitarla o abrir el jumper en todos los nodos intermedios**;
  de lo contrario el bus queda sobrecargado y aparecen errores intermitentes.

## 2.7 Alimentacion, proteccion y aislamiento

Para instalaciones reales (tablero, cableado por pared, motores cerca):

- **TVS bidireccional** de ~24 V entre `CAN_H` y `CAN_L`, y de cada linea a GND,
  contra transitorios.
- **Optoacoplamiento** del transceptor (por ejemplo ISO1050 o un transceptor +
  optoacopladores + DC/DC aislado) cuando conviven nodos alimentados desde
  fuentes distintas.
- **Separacion de tierras**: la masa de la parte de potencia (reles, contactores)
  no debe ser el camino de retorno de la senal CAN. Estrella de masas en la
  fuente y una referencia unica para el bus.
- Fusible o PTC en la alimentacion de cada nodo.
- Reles con snubber RC o diodo volante; los picos inductivos son la causa
  habitual de tramas corruptas.

## 2.8 Conector recomendado

Un conector de 4 vias por nodo simplifica el mantenimiento:

| Pin | Senal |
|----:|-------|
| 1 | V+ (12/24 V) |
| 2 | CAN_H |
| 3 | CAN_L |
| 4 | GND |

Entrada y salida en paralelo dentro del nodo mantienen la topologia lineal sin
stubs largos.
