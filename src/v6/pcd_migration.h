#pragma once

/*
 * PCD v7.0 - Capa de migracion de protocolo.
 *
 * Permite que un dispositivo v6 entienda tramas/recursos de versiones
 * anteriores (o viceversa) sin romperse. La migracion de recursos traduce los
 * codigos legacy del nucleo original (RES_RELAY=0x10, RES_DIMMER=0x20, ...) a
 * los tipos v6 (PCD_ResourceType). La idea se extiende a codecs de payload por
 * version manteniendo varios codecs en paralelo.
 */

#include <stdint.h>

#include "v6/pcd_resource.h"

namespace pcd {

/*
 * Traduce un codigo de recurso legacy (ResourceType original) a un
 * PCD_ResourceType v6. Devuelve PCD_ResourceType::NONE si no hay equivalente.
 */
PCD_ResourceType pcdMigrateLegacyResource(uint8_t legacyCode);

/* true si el codigo legacy tiene un equivalente v6 conocido. */
bool pcdHasLegacyMigration(uint8_t legacyCode);

}  // namespace pcd
