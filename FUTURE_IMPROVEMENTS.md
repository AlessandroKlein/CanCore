# Futuras mejoras de PCD_CAN (v7)

Estado de referencia: **7.0.0** (plataforma modular completa: v6.0..v7.0).

---

## Implementado

### Nucleo existente (v0.x, testeado en `native`)
- Protocolo CAN 2.0B de 29 bits con dos layouts (A: P3+T5+D7+S14; B: P3+T5+D8+S8+Seg5).
- `CanNode` (filtrado, despacho, suscripciones, heartbeat), `DeviceManager`, `RuleEngine`.
- Persistencia `ConfigStore` + CRC16 con adaptadores Memory/EEPROM/NVS.
- `NodeWatchdog` (watchdog de red) y `NodeRegistry` (inventario).
- OTA emisor (`OtaManager`) y receptor abstracto (`OtaReceiver`, `OtaWindow`).
- Routing (`RouteTable`, `RoutingEngine`, `Gateway`, `CanonicalFrame`) y bridges (MQTT, Modbus, Modbus TCP, DALI, CANopen/NMEA2000/KNX abstractos).
- Topologia (`bus_profile`, `CanTreeHub`) y validador de cableado.
- Seguridad SHA-256 / HMAC-SHA256, tunel CAN-over-IP con guard anti-bucle.
- HAL (`ICanBus`): TWAI, MCP2515 y bus virtual.

### Capa v6 (aditiva, testeada en `native`, sin hardware)
- `pcd_version.h`: version de libreria (6.0.0) y de protocolo (6.0) + feature flags.
- `pcd_errors.h`: sistema de errores `PCD_OK` / `PCD_ERR_*`.
- `pcd_crc.{h,cpp}`: CRC8, CRC16 (CCITT-FALSE) y CRC32 con vectores publicados.
- `pcd_datatypes.{h,cpp}`: tipos normalizados + codec Little Endian explicito (enteros y IEEE-754).
- `pcd_id.{h,cpp}`: `PCD_CAN_ID` (encode/decode/isBroadcast/isMulticast/isForNode/isForNetwork) y Node-ID de 16 bits.
- `pcd_resource.{h,cpp}`: `PCD_ResourceType` particionado + `PCD_ResourceDescriptor` con banderas.
- `pcd_node_info.h`: `PCD_NodeInfo` + comandos de discovery (request/response/announce/leave).
- `pcd_diagnostics.{h,cpp}`: `PCD_CAN_Diagnostics`, `PCD_CAN_State`, `PCD_HealthFlag`, config de bus-off.
- `pcd_firmware.h`: `PCD_FirmwareManifest` + compatibilidad + comandos de transferencia.
- `pcd_mock_can.{h,cpp}`: `PCD_MockCAN` con red simulada e inyeccion de fallas (drop, duplicate, delay, CRC error, bus-off).
- Tests `test/test_v6` (25 casos en verde).

---

## Implementable sin hardware

Estas funciones pueden desarrollarse y probarse con unit tests, mocks y simulacion:

- ACK / NACK / RETRY genericos con backoff (maxRetries, retryDelay, responseTimeout).
- Sequence numbers genericos y deteccion de duplicados/perdidos/fuera de orden.
- Rate limiting por nodo (maxFramesPerSecond, maxBytesPerSecond, burstLimit).
- Time synchronization (TIME_REQUEST / TIME_RESPONSE / TIME_ANNOUNCEMENT; wall vs monotonic).
- Escenas genericas (Node + Resource + Value + Transition + Delay) y ejecucion local/gateway.
- Motor de automatizacion `PCD_AutomationEngine` (PCD_Rule / PCD_Condition / PCD_Action).
- `PCD_Gateway`, `PCD_Extension`, `PCD_CanRouter` / `PCD_CanInterface` y reglas de enrutamiento multi-CAN.
- Extensiones abstractas `PCD_DALI_Extension`, `PCD_ModbusExtension`, `PCD_KNXExtension`, `PCD_MQTTBridge`.
- `PCD_Config` (loadConfig/saveConfig/resetConfig/loadFactoryDefaults) y factory reset.
- `PCD_SecurityProvider` (authenticate/verify/sign/encrypt/decrypt) con NO_SECURITY / CRC_ONLY / AUTHENTICATION_HOOK.
- `PCD_FirmwareManager` (metadata, chunk, progress, abort) y protocolo de bootloader AVR.
- Simulador de red (1..255 nodos: broadcast, unicast, discovery, heartbeat, offline, recovery, routing, ACK/retry, duplicados).
- Persistencia de estado (last known state) con debounce de escritura.

---

## Pendiente de hardware

Queda marcado como `HARDWARE_VALIDATION_REQUIRED` (no se afirma validez electrica):

- Validacion electrica CAN: terminaciones, impedancia, reflexiones, ramas, capacidad.
- Topologia estrella/arbol con star coupler fisico (el software ya soporta `CanTreeHub`).
- Transceptores concretos y aislamiento galvanico.
- Backends fisicos MCP2515 (SPI) y ESP32 TWAI (TEC/REC, alerts, modos listen-only/loopback).
- `PCD_CAN_Transceiver` con modos NORMAL/STANDBY/SILENT/LOOPBACK/DISABLED.
- DALI fisico, KNX TP1, RS-485 Modbus RTU.
- OTA ESP32 y bootloader AVR reales (tamano de boot section varia por modelo).

---

## Pendiente de validacion externa

Interoperabilidad que requiere un par real / especificacion del fabricante:

- CANopen interoperability (perfiles).
- NMEA2000 interoperability (requisitos especificos).
- KNX interoperability (TP1).
- Loxone interoperability (se usa "Loxone-inspired architecture" hasta probar un Miniserver real).
- Perfiles de fabricante especificos.
- Interoperabilidad criptografica (AEAD/HMAC con claves compartidas).

No se declara compatibilidad con ninguno de estos hasta disponer de prueba real.

