#pragma once

/*
 * Motor de registro inyectable (Custom Logger).
 *
 * Reemplaza las llamadas directas a Serial.print() por un sumidero de
 * diagnostico configurable por la aplicacion:
 *
 *   - un UART secundario,
 *   - la consola WebSocket del Gateway,
 *   - un archivo de texto en una tarjeta SD,
 *   - un logger de red,
 *   - o la desactivacion total (CAN_LOG_LEVEL == 0) para maximizar el
 *     rendimiento en microcontroladores con poca memoria Flash.
 *
 * El nivel de compilacion (CAN_LOG_LEVEL) decide si las cadenas entran o no
 * al binario. En produccion (0) el compilador elimina TODO, sin dejar ni un
 * string de trazas en Flash.
 *
 * Uso tipico (Arduino):
 *
 *   void myLog(uint8_t level, const char *format, va_list args) {
 *       char buf[128];
 *       vsnprintf(buf, sizeof(buf), format, args);
 *       Serial2.println(buf);
 *   }
 *
 *   static pcd::SystemApi api;
 *   api.millis = millis;
 *   api.delay_ms = [](uint32_t ms) { delay(ms); };
 *   api.log = myLog;
 *   pcd::setSystemApi(&api);
 */

#include <stdarg.h>

#include "system/system_api.h"

#ifndef CAN_LOG_LEVEL
#define CAN_LOG_LEVEL 0
#endif

namespace pcd {

/* Sumidero de mensajes. Se ejecuta solo si CAN_LOG_LEVEL >= nivel. */
inline void logMessage(uint8_t level, const char *format, ...) {
    const SystemApi *api = systemApi();
    if (api && api->log) {
        va_list args;
        va_start(args, format);
        api->log(level, format, args);
        va_end(args);
    }
}

}  // namespace pcd

/* Macro de nivel ERROR. `__VA_ARGS__` incluye la cadena de formato y los
 * argumentos, de modo que no se necesita el truco `##__VA_ARGS__` de GCC. */
#define LOG_ERROR(...)                                              \
    do {                                                            \
        if (CAN_LOG_LEVEL >= 1) {                                   \
            ::pcd::logMessage(::pcd::LOG_LEVEL_ERROR, __VA_ARGS__); \
        }                                                           \
    } while (0)

#define LOG_INFO(...)                                               \
    do {                                                            \
        if (CAN_LOG_LEVEL >= 2) {                                   \
            ::pcd::logMessage(::pcd::LOG_LEVEL_INFO, __VA_ARGS__);  \
        }                                                           \
    } while (0)

#define LOG_DEBUG(...)                                              \
    do {                                                            \
        if (CAN_LOG_LEVEL >= 3) {                                   \
            ::pcd::logMessage(::pcd::LOG_LEVEL_DEBUG, __VA_ARGS__); \
        }                                                           \
    } while (0)
