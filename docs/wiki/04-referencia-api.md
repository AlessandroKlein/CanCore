# 4. Referencia de API

Todo vive en el espacio de nombres `pcd`. Basta con incluir `<PCD_CAN.h>`.

## 4.1 `CanNode`

Capa de aplicacion del protocolo: filtra por destino, despacha comandos a los
recursos locales, difunde estado, mantiene suscripciones y emite el heartbeat.

```cpp
CanNode(ICanBus &bus, uint16_t node_id);

CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE);
void      poll(uint32_t now_ms);              // llamar en cada loop()
bool      handleFrame(const CanFrame &frame);
```

### Recursos y acciones

```cpp
bool registerResource(uint8_t resource, uint8_t channel,
                      ResourceHandler handler, void *ctx = 0);

bool applyLocal(uint8_t resource, uint8_t channel,
                uint8_t action, uint32_t param = 0);

DeviceManager &devices();
```

`applyLocal()` aplica el comando y difunde el estado resultante: es la forma
correcta de reaccionar a una tecla fisica.

### Emision

```cpp
CanStatus sendCommand(uint8_t target, uint8_t resource, uint8_t channel,
                      uint8_t action, uint32_t param = 0);
CanStatus publishState(uint8_t resource, uint8_t channel, float value,
                       uint16_t flags = DIAG_NONE);
CanStatus publishTelemetry(uint8_t resource, uint8_t channel, float value,
                           uint16_t flags = DIAG_NONE);
CanStatus publishInputEvent(uint8_t channel, uint8_t event);
CanStatus sendConfig(uint8_t target, uint8_t sub_command,
                     const uint8_t *payload, uint8_t payload_len);
CanStatus sendConfigAck(const uint8_t *payload, uint8_t payload_len);
CanStatus sendHeartbeat(uint32_t now_ms);
```

### Escucha

```cpp
bool subscribe(uint16_t source, uint8_t resource, uint8_t channel,
               StateListener listener, void *ctx = 0);   // kAnySource / kAnyChannel
void onConfig(ConfigListener listener, void *ctx = 0);
void onAnyFrame(FrameListener listener, void *ctx = 0);
```

`onAnyFrame()` recibe toda trama ajena: lo usan el motor de reglas, los
registradores y los puentes del gateway.

### Callbacks

```cpp
typedef bool (*ResourceHandler)(uint8_t channel, uint8_t action, uint32_t param,
                                float &out_value, void *ctx);
typedef void (*StateListener)(const CanFrame &frame, float value, void *ctx);
typedef void (*ConfigListener)(const CanFrame &frame, void *ctx);
typedef void (*FrameListener)(const CanFrame &frame, void *ctx);
```

El manejador de recurso devuelve `false` para rechazar un comando: en ese caso no
se difunde estado.

## 4.2 `DeviceManager`

Administra los canales heterogeneos del nodo con una maquina de estados por
canal (`PHASE_IDLE`, `PHASE_RAMPING`, `PHASE_TIMED`). `CanNode` ya contiene uno y
lo actualiza dentro de `poll()`.

```cpp
bool registerChannel(uint8_t resource, uint8_t channel,
                     ResourceHandler handler, void *ctx = 0);
bool apply(uint8_t resource, uint8_t channel, uint8_t action,
           uint32_t param, float &out_value);
void update(uint32_t now_ms);
void cancelPending(uint8_t resource, uint8_t channel);
void setFlags(uint8_t resource, uint8_t channel, uint16_t flags);
bool value(uint8_t resource, uint8_t channel, float &out_value) const;
```

Interpretacion del parametro:

| Accion | `param` |
|--------|---------|
| `ACT_ON`, `ACT_TOGGLE` | milisegundos hasta el apagado automatico (0 = sin limite) |
| `ACT_OFF` | ignorado |
| `ACT_SET_VALUE` | `packSetValue(valor, fade_ms)`: valor en los 16 bits altos, tiempo de rampa en los bajos |

Ejemplo: encender una luz de escalera por 2 minutos y llevar un dimmer al 80% en
3 segundos, sin bloquear el `loop()`:

```cpp
node.applyLocal(RES_RELAY,  0x01, ACT_ON,        120000);
node.applyLocal(RES_DIMMER, 0x01, ACT_SET_VALUE, packSetValue(80, 3000));
```

Cuando el temporizador vence o la rampa termina, el gestor difunde el estado por
si mismo (`StateEmitter`), cerrando el lazo de realimentacion.

## 4.3 `ConfigStore` y `StorageAdapter`

Persistencia del Node-ID y de las reglas, con cabecera, version y CRC-16.

```cpp
class StorageAdapter {
    virtual bool begin();
    virtual bool read(uint16_t offset, uint8_t *data, uint16_t length);
    virtual bool write(uint16_t offset, const uint8_t *data, uint16_t length);
    virtual bool commit();
    virtual uint16_t capacity() const;
};
```

Backends incluidos:

| Clase | Plataforma | Medio |
|-------|-----------|-------|
| `MemoryStorage` | escritorio / cualquiera | RAM (pruebas) |
| `EepromStorage` | AVR | EEPROM interna, con `EEPROM.update` |
| `NvsStorage` | ESP32 | NVS via `Preferences`, blob unico |

```cpp
pcd::EepromStorage storage;
pcd::ConfigStore config(storage);

config.begin(0x0016);        // false => bloque vacio o CRC invalido
config.addRule(rule);
config.save();
```

Disposicion del bloque:

```
offset 0  : magic 'PC' (2)  version (1)  cantidad de reglas (1)  node_id (2)
offset 6  : reglas de 20 bytes cada una, big-endian
final     : CRC-16/CCITT de la cabecera + las reglas (2)
```

Si el CRC no valida, `begin()` devuelve `false` y el nodo arranca con los valores
por defecto en lugar de con reglas a medio escribir.

## 4.4 `RuleEngine` y `BindingRule`

```cpp
struct BindingRule {
    uint16_t source_node;      // 0x0000 = cualquiera
    uint8_t  source_resource;
    uint8_t  source_channel;   // kAnyChannel = cualquiera
    uint8_t  trigger;          // TRIG_EVENT / TRIG_VALUE_* / TRIG_ANY
    uint8_t  event;            // evento esperado en TRIG_EVENT
    float    threshold;        // umbral en los disparos por valor
    uint8_t  target_resource;  // recurso local a accionar
    uint8_t  target_channel;
    uint8_t  action;
    uint32_t param;
};
```

"Si la entrada 1 del nodo 0x0005 informa una pulsacion corta, conmutar el rele
del canal 2 de este nodo":

```cpp
pcd::BindingRule rule;
rule.source_node     = 0x0005;
rule.source_resource = pcd::RES_DIGITAL_INPUT;
rule.source_channel  = 0x01;
rule.trigger         = pcd::TRIG_EVENT;
rule.event           = pcd::EVT_SHORT_CLICK;
rule.target_resource = pcd::RES_RELAY;
rule.target_channel  = 0x02;
rule.action          = pcd::ACT_TOGGLE;
rule.param           = 0;

config.addRule(rule);
config.save();

pcd::RuleEngine rules(node, config);
rules.begin();          // engancha onAnyFrame() y onConfig()
```

La evaluacion ocurre en el propio nodo que ejecuta la accion: no hay gateway en
el camino.

> Cuidado con los lazos: una regla disparada por `MSG_STATE` que produce otro
> `MSG_STATE` puede realimentarse entre dos nodos. Preferir `TRIG_EVENT` sobre
> entradas digitales para las vinculaciones de tecla.

## 4.5 Programacion remota por SDO

Una regla ocupa 20 bytes y no entra en los 6 bytes utiles de una trama, por lo
que se transfiere segmentada. Sub-comandos (byte 1 de un `MSG_CONFIG`):

| Sub-comando | Valor | Payload |
|-------------|------:|---------|
| `CFG_SET_NODE_ID` | 0x01 | nuevo Node-ID (2 bytes BE) |
| `CFG_RULE_BEGIN` | 0x02 | indice a reemplazar, 0xFF = anexar |
| `CFG_RULE_CHUNK` | 0x03 | segmento (0..3) + 5 bytes de la regla |
| `CFG_RULE_COMMIT` | 0x04 | CRC-16 de los 20 bytes |
| `CFG_RULE_DELETE` | 0x05 | indice |
| `CFG_RULE_CLEAR` | 0x06 | - |
| `CFG_SAVE` | 0x07 | - |
| `CFG_RULE_COUNT` | 0x08 | - |
| `CFG_ACK` | 0x7F | sub-comando + codigo de resultado |

La regla solo se aplica y se persiste si llegaron los cuatro segmentos y el CRC
coincide; si no, el nodo responde `CFG_STATUS_CRC_ERROR` y descarta el buffer.

## 4.6 HAL: `ICanBus`

```cpp
CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE);
void      end();
CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0);
CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0);
void      setFilter(const CanFilter &filter);
bool      isBusOff() const;
CanStatus recover();
```

| Implementacion | Constructor |
|----------------|-------------|
| `Esp32TwaiBus` | `Esp32TwaiBus(tx_pin, rx_pin)` |
| `Mcp2515Bus` | `Mcp2515Bus(cs_pin, MCP_CLOCK_16MHZ o MCP_CLOCK_8MHZ, int_pin)` |
| `NativeCanBus` | `NativeCanBus(VirtualBus *bus)` |

Codigos de estado: `CAN_OK`, `CAN_ERR_INIT`, `CAN_ERR_TX_FAIL`, `CAN_ERR_TX_BUSY`,
`CAN_ERR_NO_DATA`, `CAN_ERR_BUS_OFF`, `CAN_ERR_NOT_STARTED`.

## 4.7 Utilidades del protocolo

```cpp
uint32_t encodeId(uint8_t priority, uint8_t msg_type, uint8_t target, uint16_t source);
CanId    decodeId(uint32_t raw_id);

CanFrame makeCommand(...);   CanFrame makeStateBroadcast(...);
CanFrame makeTelemetry(...); CanFrame makeInputEvent(...);
CanFrame makeHeartbeat(...); CanFrame makeConfig(...);
CanFrame makeOtaData(...);

float    frameValue(const CanFrame &frame);
uint16_t frameFlags(const CanFrame &frame);
uint32_t frameParam(const CanFrame &frame);
uint16_t crc16(const uint8_t *data, uint16_t length, uint16_t seed = 0xFFFF);
```

## 4.8 Capa de servicios del sistema (`system_api`)

El nucleo **no** llama a `millis()`, `delay()` ni `Serial` de forma directa.
La aplicacion inyecta estos servicios una vez, tipicamente en `setup()`:

```cpp
struct pcd::SystemApi {
    uint32_t (*millis)();                          // reloj monotono en ms
    void     (*delay_ms)(uint32_t ms);             // bloqueo en ms
    void     (*log)(uint8_t level, const char *fmt, va_list args);
    void     (*lock)();                            // entrar en seccion critica
    void     (*unlock)();                          // salir de seccion critica
};

void pcd::setSystemApi(const pcd::SystemApi *api);  // instalar
const pcd::SystemApi *pcd::systemApi();             // consultar
uint32_t pcd::systemMillis();                       // acceso corto
void     pcd::systemDelay(uint32_t ms);
```

Mapeo tipico por plataforma:

| Plataforma | `millis` | `delay_ms` | `log` |
|-----------|----------|------------|-------|
| Arduino | `millis` | `delay` | `vsnprintf` + `Serial.printf` |
| ESP-IDF | `esp_timer_get_time()/1000` | `vTaskDelay(pdMS_TO_TICKS(ms))` | `ESP_LOGI` / `ESP_LOGV` |
| FreeRTOS | `xTaskGetTickCount()` | `vTaskDelay(pdMS_TO_TICKS(ms))` | `printf` personalizado |
| STM32 Cube | `HAL_GetTick()` | `HAL_Delay()` | `printf` redirigido |
| PC / native | `std::chrono` | `std::this_thread::sleep_for` | `std::printf` |

### Motor de registro inyectable

Los macros `LOG_ERROR` / `LOG_INFO` / `LOG_DEBUG` se enrutan al sumidero
`SystemApi::log`. Con `CAN_LOG_LEVEL=0` el compilador elimina todas las cadenas.

### LockGuard

```cpp
{
    pcd::LockGuard lock;      // entra en seccion critica
    // ... cola, EEPROM, tabla ...
}                             // sale automaticamente
```

Prioridad: si `SystemApi` provee `lock()/unlock()` se usan (semaforo/critical
section). En AVR sin inyeccion cae a `cli()/sei()`. En bare-metal single-thread
no hace nada.

## 4.9 Bucle de eventos asincrono

Desacopla la ISR del procesamiento en el hilo principal:

```cpp
pcd::CanEventLoop loop;
loop.attach(myFrameProcessor, nullptr);

// desde la ISR del controlador CAN:
loop.enqueue(frame);          // ~µs, solo copia

// desde el loop() principal:
loop.tick();                   // drena la cola y llama a process(frame, ctx)
```

Tambien esta `RingBuffer<T, Capacity>`, un buffer SPSC lock-free que usa
`CanEventLoop` internamente.

## 4.10 `CanNodeTmpl<RxDepth>`

Nodo con buffer de RX dimensionado en tiempo de compilacion:

```cpp
pcd::CanNodeTmpl<4>  node(bus, 0x0016);   // ATmega328P
pcd::CanNodeTmpl<64> gateway(bus, 0x0001); // ESP32 gateway
```

Expone la misma interfaz que `CanNode` mas:

```cpp
bool enqueueFrame(const CanFrame &frame);  // desde ISR
void poll(uint32_t now_ms);                // drena el ring buffer, luego service
size_t pendingFrames() const;
```

## 4.11 Tunel CAN sobre IP (`TunnelEngine`)

```cpp
class pcd::ITunnelTransport {
    virtual bool send(uint16_t peer, const uint8_t *data, size_t len) = 0;
    virtual bool receive(uint16_t &peer_out, uint8_t *data, size_t max_len, size_t &len) = 0;
    virtual bool available() const = 0;
};

pcd::TunnelEngine tunnel(transport);
bool tunnel.sendFrame(uint16_t peer, const pcd::CanFrame &frame);
bool tunnel.receiveFrame(uint16_t &peer_out, pcd::CanFrame &frame);
```

Formato del datagrama (16 bytes, little-endian):

```text
[0]    magia 0xA5
[1..4] ID 29-bit, LSB primero
[5]    DLC
[6..13] payload (8 bytes)
[14..15] CRC-16 de los bytes 0..13
```

El transporte de red (UDP/TCP/WiFi/Ethernet) **no** viene con la libreria; lo
implementa la aplicacion (ver `docs/wiki/08-librerias-recomendadas.md`).
