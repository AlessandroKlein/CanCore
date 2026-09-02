#pragma once

/*
 * Capa de servicios del sistema (System Layer API).
 *
 * La libreria base es 100% agnostica al framework: NO incluye <Arduino.h>,
 * no llama a millis(), delay() ni a Serial. En su lugar, la aplicacion
 * inyecta un SystemApi con punteros a las funciones de la plataforma.
 *
 * Mapeo tipico:
 *
 *   Arduino   -> millis(), delay(), una UART por vsnprintf
 *   ESP-IDF   -> esp_timer_get_time()/1000, vTaskDelay(), ESP_LOGV/ESP_LOGI
 *   FreeRTOS  -> xTaskGetTickCount(), vTaskDelay(), printf personalizado
 *   STM32 Cube -> HAL_GetTick(), HAL_Delay(), printf redirigido
 *   PC/native -> std::chrono, std::this_thread::sleep_for, std::printf
 *
 * El protocolo (can_protocol), la capa de nodo (can_node), el gestor de
 * recursos (device_manager), la persistencia (config_storage) y el motor de
 * reglas (rule_engine) dependen solo de esta interfaz para reloj, bloqueo y
 * diagnostico. Ninguna de ellas conoce la placa real.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

namespace pcd {

/* Instante monotono en milisegundos. En AVR la escala es suficiente (49 dias). */
typedef uint32_t time_ms_t;

/* Prioridades de log. Coinciden con los niveles de los macros LOG_*. */
enum LogLevel : uint8_t {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

/*
 * Contrato de servicios que la aplicacion debe proveer.
 *
 *   millis   - reloj monotono en ms (obligatorio)
 *   delay_ms - bloqueo en ms (obligatorio)
 *   log      - sumidero de diagnostico con formato printf + va_list
 *              (opcional; si es 0 se desactivan las trazas)
 *   lock     - entrar en seccion critica (opcional; ver LockGuard)
 *   unlock   - salir de seccion critica (opcional; ver LockGuard)
 *
 * Una funcion de log con va_list permite al cliente formatear en su propia
 * consola (UART secundaria, WebSocket, archivo en SD, Network Logger...)
 * sin que la libreria dependa de un flujo de salida concreto. Cuando
 * CAN_LOG_LEVEL == 0 el compilador elimina por completo las cadenas.
 */
struct SystemApi {
    time_ms_t (*millis)();                          /* monotonic ms */
    void (*delay_ms)(uint32_t ms);                  /* blocking delay */
    void (*log)(uint8_t level, const char *format, va_list args);
    void (*lock)();                                 /* enter critical section */
    void (*unlock)();                               /* exit critical section */

    SystemApi() : millis(0), delay_ms(0), log(0), lock(0), unlock(0) {}

    bool hasTime() const { return millis != 0 && delay_ms != 0; }
    bool hasLog() const { return log != 0; }
    bool hasLock() const { return lock != 0 && unlock != 0; }
};

/*
 * API global de sistema. La aplicacion la configura una vez en setup():
 *
 *   static pcd::SystemApi api;
 *   api.millis = millis;
 *   api.delay_ms = [](uint32_t ms) { delay(ms); };
 *   api.log = myLogSink;
 *   pcd::setSystemApi(&api);
 *
 * Los nodos y los drivers creados despues leen punteros desde esta estructura
 * cada vez que necesitan un reloj o un log, de modo que no hay estado global
 * de tiempo copiado en cada objeto.
 */

void setSystemApi(const SystemApi *api);
const SystemApi *systemApi();

/* Accesos convenientes con bloqueo "intento de no romper nada". */
time_ms_t systemMillis();
void systemDelay(uint32_t ms);

}  // namespace pcd
