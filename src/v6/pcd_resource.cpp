#include "v6/pcd_resource.h"

namespace pcd {

const char *pcdResourceTypeName(PCD_ResourceType type) {
    switch (type) {
        case PCD_ResourceType::NONE: return "none";
        case PCD_ResourceType::DIGITAL_INPUT: return "digital_input";
        case PCD_ResourceType::DIGITAL_OUTPUT: return "digital_output";
        case PCD_ResourceType::ANALOG_INPUT: return "analog_input";
        case PCD_ResourceType::ANALOG_OUTPUT: return "analog_output";
        case PCD_ResourceType::RELAY: return "relay";
        case PCD_ResourceType::PWM: return "pwm";
        case PCD_ResourceType::TEMPERATURE: return "temperature";
        case PCD_ResourceType::HUMIDITY: return "humidity";
        case PCD_ResourceType::PRESSURE: return "pressure";
        case PCD_ResourceType::LIGHT: return "light";
        case PCD_ResourceType::MOTION: return "motion";
        case PCD_ResourceType::SWITCH: return "switch";
        case PCD_ResourceType::BUTTON: return "button";
        case PCD_ResourceType::ENCODER: return "encoder";
        case PCD_ResourceType::COUNTER: return "counter";
        case PCD_ResourceType::ENERGY: return "energy";
        case PCD_ResourceType::CONTACT: return "contact";
        case PCD_ResourceType::ALARM: return "alarm";
        case PCD_ResourceType::SCENE: return "scene";
        case PCD_ResourceType::HVAC: return "hvac";
        case PCD_ResourceType::CUSTOM: return "custom";
        case PCD_ResourceType::DALI: return "dali";
        case PCD_ResourceType::MODBUS: return "modbus";
        case PCD_ResourceType::KNX: return "knx";
        default: return "unknown";
    }
}

bool pcdIsStandardResource(uint8_t code) {
    return code > 0x00 && code < PCD_RESOURCE_EXTENSION_BASE;
}

bool pcdIsExtensionResource(uint8_t code) {
    return code >= PCD_RESOURCE_EXTENSION_BASE && code < PCD_RESOURCE_MANUFACTURER_BASE;
}

bool pcdIsManufacturerResource(uint8_t code) {
    return code >= PCD_RESOURCE_MANUFACTURER_BASE && code != 0xFF;
}

float PCD_ResourceDescriptor::minFloat() const {
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = minValue;
    return conv.f;
}

float PCD_ResourceDescriptor::maxFloat() const {
    union {
        uint32_t u;
        float f;
    } conv;
    conv.u = maxValue;
    return conv.f;
}

}  // namespace pcd
