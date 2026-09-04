# Implementacion web del gateway

Este documento describe como convertir los recursos web de `PCD_CAN` en una interfaz real para un ESP32. La libreria entrega la vista HTML y los contratos CAN; el proyecto de firmware elige el servidor HTTP, el filesystem y la autenticacion.

## Recursos incluidos

`src/web/web_pages.h` expone:

- `pcd::webAppHtml()`: aplicacion completa HTML/CSS/JS.
- `pcd::webViewName()`: nombres de las siete vistas.
- `pcd::webViewPath()`: rutas/hash para Resumen, Nodos, Rutas, OTA, Tunel, Diagnostico y Ajustes.

La pagina se sirve como un unico recurso en `/`. El navegador cambia de vista usando `/#nodes`, `/#ota`, etc., evitando duplicar HTML en la flash.

## API HTTP esperada

El ejemplo de interfaz consulta estos endpoints JSON:

| Endpoint | Metodo | Respuesta minima |
|---|---|---|
| `/api/summary` | GET | `nodes`, `frames_per_minute`, `can_status` |
| `/api/nodes` | GET | inventario de node ID, heartbeat y estado |
| `/api/routes` | GET | tabla de rutas y destino |
| `/api/ota/status` | GET | estado, progreso y error |
| `/api/tunnel/status` | GET | transporte, peers y datagramas |
| `/api/diagnostics` | GET | bus-off, errores, uptime y memoria |
| `/api/identity` | GET/POST | modo manual/automático e ID actual |
| `/api/listen-filters` | GET/POST | filtros de origen, recurso y canal |
| `/api/ota/abort` | POST | cancela la campaña OTA |

Los endpoints son un contrato de aplicación, no forman parte del núcleo PCD. Las respuestas pueden crecer sin romper la UI.

## Integracion con `WebServer.h`

```cpp
#include <PCD_CAN.h>
#include <WebServer.h>

WebServer http(80);

void setupWeb() {
    http.on("/", HTTP_GET, []() {
        http.send(200, "text/html; charset=utf-8", pcd::webAppHtml());
    });
    http.on("/api/summary", HTTP_GET, []() {
        http.send(200, "application/json", "{\"nodes\":1,\"frames_per_minute\":0,\"can_status\":\"ok\"}");
    });
    http.begin();
}

void loopWeb() {
    http.handleClient();
}
```

Para `ESPAsyncWebServer`, la ruta equivalente usa `request->send(200, "text/html", pcd::webAppHtml())`. En ambos casos, la autenticacion debe ejecutarse antes de responder a `/api/*`.

`/api/nodes` debe presentar cada nodo descubierto con esta forma mínima:

```json
{
    "nodes": [{
        "id": 22,
        "status": "online",
        "last_heartbeat": 123456,
        "id_mode": "manual",
        "resources": [{"type": 16, "name": "Rele", "channel": 1}]
    }]
}
```

El nombre visible se deriva del tipo de recurso (`RES_RELAY`, `RES_DIMMER`,
`RES_ENV_SENSOR`, etc.) y no de texto enviado por un nodo, evitando que una
identificación remota inyecte HTML en la pantalla.

## Situaciones cubiertas

- **Operacion:** resumen y actividad del bus.
- **Inventario:** nodos vivos y ultimo heartbeat.
- **Identidad:** selección manual/automática y Node-ID actual.
- **Escucha:** filtros por origen, recurso y canal, configurables vía API y CAN.
- **Enrutamiento:** inspeccion de reglas hacia puentes.
- **Mantenimiento:** progreso y cancelacion OTA.
- **Interconexion:** estado de tunel UDP/TCP.
- **Diagnostico:** bus-off, errores, memoria y uptime.
- **Ajustes:** enlaces al backend de configuracion, sin guardar credenciales en el HTML.

## Seguridad y limites

- No exponer el servidor sin autenticacion y sin TLS fuera de una LAN confiable.
- No insertar valores CAN directamente en HTML sin escapar; preferir JSON.
- El HTML es una UI de referencia, no un reemplazo de la logica de seguridad del proyecto.
- `webAppHtml()` aumenta el uso de flash del firmware; en AVR no se recomienda incluirlo.
