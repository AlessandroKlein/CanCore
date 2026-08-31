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
#include "can_protocol.h"
#include "config_storage.h"
#include "device_manager.h"
#include "hal_can.h"
#include "rule_engine.h"
#include "system_config.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "hal/hal_can_esp32.h"
#elif defined(__AVR__)
#include "hal/hal_can_mcp2515.h"
#else
#include "hal/hal_can_native.h"
#endif

#define PCD_CAN_VERSION "0.2.0"
