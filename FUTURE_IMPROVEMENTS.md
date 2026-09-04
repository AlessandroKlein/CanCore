# Futuras mejoras de PCD_CAN

Estado de referencia: **0.6.0**.

## Implementado en 0.6.0

- Expiracion de nodos en `NodeRegistry` despues de tres heartbeats perdidos.
- Politica `TunnelRelayGuard` para segmentar y deduplicar frames durante una ventana configurable.
- Ejemplo LAN completo `examples/CanLanBridgeGateway` con dos gateways simetricos.
- Paginas web de estado, configuracion de peer y simulacion de dispositivos remotos.
- Documentacion de despliegue, anti-loop y limites de seguridad.

## Siguiente prioridad

1. Autenticacion criptografica de datagramas: HMAC-SHA256 o AEAD elegida por la aplicacion, sin meter una implementacion criptografica en el nucleo.
2. ACK de ventanas OTA y bootloader receptor para ESP32, ATmega2560 y ATmega328P.
3. Persistencia opcional del inventario `NodeRegistry` y nombre amigable por nodo.
4. Expiracion diferenciada por tipo de nodo y diagnostico de perdida de heartbeat.
5. API web generada desde `NodeRegistry`, con autenticacion, CSRF y control de permisos.
6. Pruebas de interoperabilidad con dos ESP32 fisicos, MCP2515, W5500 y trafico LAN real.
7. Fuzzing de `decodeTunnelFrame()`, `decodeId()` y comandos SDO antes de publicar una release mayor.
8. Perfiles declarativos de recursos para Matter, Zigbee y ESPHome mantenidos fuera de `src/`.
9. Mecanismo formal de versionado/compatibilidad de frames de tunel.
10. Herramienta de configuracion de escritorio que use el mismo protocolo CAN, sin USB propietario.

## Criterio para 1.0.0

La libreria no se considera de produccion mayor hasta tener:

- autenticacion o aislamiento de red documentado para tuneles,
- bootloader OTA receptor probado con rollback o estrategia de recuperacion,
- pruebas de hardware y de perdida/reconexion de red,
- contrato de compatibilidad de protocolo versionado,
- revision de consumo de RAM/flash en cada placa soportada,
- ejemplos LAN y web revisados contra el stack concreto de la aplicacion.

Mientras esos puntos sigan abiertos, las releases `0.x` reflejan correctamente
que el protocolo y el nucleo son utilizables, pero la seguridad y el firmware de
produccion dependen del proyecto que los integra.
