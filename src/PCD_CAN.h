#pragma once

/*
 * PCD_CAN - punto de entrada unico de la libreria.
 *
 *   #include <PCD_CAN.h>
 *
 * Incluye el protocolo, la HAL de la plataforma detectada, el nodo, el gestor
 * de recursos, la persistencia y el motor de reglas. Los drivers concretos se
 * seleccionan por macros del compilador para que el mismo sketch compile en
 * ESP32, AVR y en el simulador de escritorio.
 */

#include "can_node.h"
#include "can_node_tmpl.h"
#include "can_protocol.h"
#include "config_storage.h"
#include "device_manager.h"
#include "hal_can.h"
#include "node_watchdog.h"
#include "ota_receiver.h"
#include "rule_engine.h"
#include "system_config.h"
#include "units.h"

/* Capa de servicios del sistema (agnostica al framework). */
#include "system/system_api.h"
#include "system/system_logger.h"
#include "system/system_lock.h"

/* Auditoria y autenticacion sin dependencias externas. */
#include "security/hmac_authenticator.h"
#include "security/sha256.h"

/* Bucle de eventos asincrono (ISR -> ring buffer -> tick). */
#include "core/ring_buffer.h"
#include "core/event_loop.h"

/* Motor de tunel CAN sobre IP (Data Stream Transport). */
#include "tunnel/tunnel_transport.h"
#include "tunnel/tunnel_engine.h"
#include "tunnel/udp_tunnel_transport.h"
#include "tunnel/tunnel_relay_guard.h"
#include "ota_manager.h"
#include "ota_window.h"

/* Recursos HTML agnosticos para la interfaz del gateway. */
#include "web/web_pages.h"

/* Capa fisica por topologia: perfiles de bus y hub/star coupler en arbol. */
#include "topology/bus_profile.h"
#include "topology/can_tree_hub.h"

/* Capa v6 (aditiva): versionado, errores, codec, CRC, CAN ID v6, recursos,
 * diagnostico, manifiesto de firmware y transporte simulado. */
#include "v6/pcd_v6.h"

/* Motor de enrutamiento multi-protocolo y puentes. */
#include "routing/canonical.h"
#include "routing/knx_group_map.h"
#include "routing/route_table.h"
#include "routing/routing_engine.h"
#include "bridge/bridge.h"
#include "bridge/bridge_dali.h"
#include "bridge/bridge_mqtt.h"
#include "bridge/bridge_modbus.h"
#include "bridge/bridge_modbus_tcp.h"
#include "bridge/bridge_standard.h"
#include "gateway/gateway.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "hal/hal_can_esp32.h"
#elif defined(__AVR__)
#include "hal/hal_can_mcp2515.h"
#else
#include "hal/hal_can_native.h"
#endif

#define PCD_CAN_VERSION "6.2.0"
