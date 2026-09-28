#pragma once

/*
 * PCD v6 - Versionado de libreria, protocolo y feature flags.
 *
 * La version de la libreria (codigo) y la version del protocolo (wire format)
 * son independientes: un firmware con libreria 6.0.0 puede hablar el protocolo
 * 6.0 con otro firmware de libreria distinta. Tambien se separan la version de
 * firmware (build) y la revision de hardware (no gestionadas aqui).
 *
 * Los feature flags permiten que un ATmega328P (2 KB de RAM) no incorpore el
 * codigo completo de un Gateway ESP32: cada flag elimina en tiempo de
 * compilacion el modulo correspondiente.
 */

#include <stdint.h>

namespace pcd {

/* ------------------------------------------------------------------ */
/* Version de libreria                                                 */
/* ------------------------------------------------------------------ */

#define PCD_LIBRARY_VERSION_MAJOR 6
#define PCD_LIBRARY_VERSION_MINOR 0
#define PCD_LIBRARY_VERSION_PATCH 0
#define PCD_LIBRARY_VERSION_STRING "6.0.0"

/* ------------------------------------------------------------------ */
/* Version de protocolo                                                */
/* ------------------------------------------------------------------ */

#define PCD_PROTOCOL_MAJOR 6
#define PCD_PROTOCOL_MINOR 0
#define PCD_PROTOCOL_PATCH 0

/* Codificado como un entero comparable: (mayor << 16) | (minor << 8) | patch. */
#define PCD_PROTOCOL_VERSION \
    ((PCD_PROTOCOL_MAJOR << 16) | (PCD_PROTOCOL_MINOR << 8) | PCD_PROTOCOL_PATCH)

/* ------------------------------------------------------------------ */
/* Feature flags (0 = off, compilado fuera; 1 = on)                    */
/* ------------------------------------------------------------------ */

#ifndef PCD_ENABLE_ROUTER
#define PCD_ENABLE_ROUTER 0
#endif

#ifndef PCD_ENABLE_GATEWAY
#define PCD_ENABLE_GATEWAY 0
#endif

#ifndef PCD_ENABLE_DIAGNOSTICS
#define PCD_ENABLE_DIAGNOSTICS 0
#endif

#ifndef PCD_ENABLE_DISCOVERY
#define PCD_ENABLE_DISCOVERY 0
#endif

#ifndef PCD_ENABLE_SECURITY
#define PCD_ENABLE_SECURITY 0
#endif

#ifndef PCD_ENABLE_OTA
#define PCD_ENABLE_OTA 0
#endif

#ifndef PCD_ENABLE_AUTOMATION
#define PCD_ENABLE_AUTOMATION 0
#endif

#ifndef PCD_ENABLE_DALI
#define PCD_ENABLE_DALI 0
#endif

#ifndef PCD_ENABLE_MODBUS
#define PCD_ENABLE_MODBUS 0
#endif

#ifndef PCD_ENABLE_KNX
#define PCD_ENABLE_KNX 0
#endif

}  // namespace pcd
