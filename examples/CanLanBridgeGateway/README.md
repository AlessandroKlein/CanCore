# CanLanBridgeGateway

Ejemplo completo para unir dos redes CAN independientes por una LAN usando dos ESP32.

## Topologia

```text
Red CAN A -- ESP32 Gateway A -- UDP LAN -- ESP32 Gateway B -- Red CAN B
```

Cada gateway conserva su Node-ID y sus dispositivos locales. El enlace solo
reenvia frames CAN seleccionados; no existe un controlador central obligatorio.
Si el peer LAN se apaga, cada red CAN local sigue operativa.

## Configuracion de las dos placas

En la primera instancia:

```cpp
const uint8_t kSegment = 1;
const char *kLocalIp = "192.168.10.21";
const char *kPeerIp = "192.168.10.22";
```

En la segunda:

```cpp
const uint8_t kSegment = 2;
const char *kLocalIp = "192.168.10.22";
const char *kPeerIp = "192.168.10.21";
```

El sketch usa UDP en el puerto 17890. Se puede reemplazar `WiFiUDP` por
`EthernetUDP`/W5500 manteniendo `ITunnelTransport`.

## Paginas de cada gateway

- `/`: estado del peer, frames enviados/recibidos y anti-loop.
- `/config`: cambia IP y puerto del peer.
- `/simulate`: genera un comando CAN dirigido a un Node-ID remoto para probar
  la red sin conectar todavía un dispositivo físico en el otro segmento.

La simulacion no inventa estados: solo envia el comando. El estado confirmado
debe volver como `MSG_STATE` desde el dispositivo remoto.

## Anti-loop

`TunnelRelayGuard` rechaza frames cuyo segmento de origen coincide con el local
y deduplica el mismo frame durante una ventana de 5 segundos. El datagrama base
no contiene autenticacion; el ejemplo debe usarse en una LAN confiable o detrás
de una VPN/firewall. Para una instalacion de produccion agregar HMAC/AEAD en el
proyecto de aplicacion.
