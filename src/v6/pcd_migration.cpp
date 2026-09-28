#include "v6/pcd_migration.h"

namespace pcd {

PCD_ResourceType pcdMigrateLegacyResource(uint8_t legacyCode) {
    switch (legacyCode) {
        case 0x10: /* RES_RELAY          */ return PCD_ResourceType::RELAY;
        case 0x20: /* RES_DIMMER         */ return PCD_ResourceType::PWM;
        case 0x30: /* RES_DIGITAL_INPUT  */ return PCD_ResourceType::DIGITAL_INPUT;
        case 0x71: /* RES_BUTTON         */ return PCD_ResourceType::BUTTON;
        case 0x72: /* RES_BINARY_SENSOR  */ return PCD_ResourceType::CONTACT;
        case 0x80: /* RES_PRESSURE       */ return PCD_ResourceType::PRESSURE;
        case 0x81: /* RES_HUMIDITY       */ return PCD_ResourceType::HUMIDITY;
        case 0x86: /* RES_DALI_LIGHT     */ return PCD_ResourceType::DALI;
        case 0x87: /* RES_KNX_GROUP      */ return PCD_ResourceType::KNX;
        case 0x90: /* RES_VOLTAGE_SENSOR */ return PCD_ResourceType::ANALOG_INPUT;
        case 0x91: /* RES_CURRENT_SENSOR */ return PCD_ResourceType::ANALOG_INPUT;
        case 0x93: /* RES_ENERGY_SENSOR  */ return PCD_ResourceType::ENERGY;
        default: return PCD_ResourceType::NONE;
    }
}

bool pcdHasLegacyMigration(uint8_t legacyCode) {
    return pcdMigrateLegacyResource(legacyCode) != PCD_ResourceType::NONE;
}

}  // namespace pcd
