#pragma once

/*
 * PCD v6 - punto de entrada unico de la capa v6.
 *
 *   #include <v6/pcd_v6.h>
 *
 * Incluye el versionado, el sistema de errores, el codec de tipos, el CRC, la
 * identificacion de nodos (CAN ID v6), los recursos y descriptores, el
 * descubrimiento, el diagnostico, el manifiesto de firmware y el transporte
 * simulado. Es una capa aditiva: no modifica la API publica existente (CanNode,
 * CanFrame, encodeId/decodeId, ...).
 */

#include "v6/pcd_version.h"
#include "v6/pcd_errors.h"
#include "v6/pcd_datatypes.h"
#include "v6/pcd_crc.h"
#include "v6/pcd_id.h"
#include "v6/pcd_resource.h"
#include "v6/pcd_node_info.h"
#include "v6/pcd_diagnostics.h"
#include "v6/pcd_firmware.h"
#include "v6/pcd_mock_can.h"

/* v6.1 - Robustez y confiabilidad. */
#include "v6/pcd_config.h"
#include "v6/pcd_shadow.h"
#include "v6/pcd_alarm.h"

/* v6.2 - Seguridad e identidad. */
#include "v6/pcd_security.h"
#include "v6/pcd_identity.h"
#include "v6/pcd_negotiation.h"

/* v6.3 - Gestion avanzada de dispositivos. */
#include "v6/pcd_event.h"
#include "v6/pcd_topology.h"
#include "v6/pcd_store_forward.h"
#include "v6/pcd_network_stats.h"

/* v6.4 - Automatizacion distribuida. */
#include "v6/pcd_group.h"
#include "v6/pcd_automation.h"
#include "v6/pcd_scheduler.h"

/* v6.5 - Gateway y router. */
#include "v6/pcd_router.h"
#include "v6/pcd_gateway.h"

/* v7.0 - Herramientas de plataforma. */
#include "v6/pcd_analyzer.h"
#include "v6/pcd_simulator.h"
#include "v6/pcd_migration.h"
