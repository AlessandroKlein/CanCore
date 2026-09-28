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
