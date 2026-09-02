#pragma once

/*
 * Abstraccion avanzada de concurrencia (Thread-Safety).
 *
 * Protege las secciones criticas de la libreria (colas de transmision y
 * recepcion, acceso a EEPROM/NVS, tablas de suscripciones) segun el entorno:
 *
 *   - AVR 8-bit monocore -> cli()/sei() breves (no hay RTOS, y cualquier
 *     condicion de carrera se elimina enmascarando interrupciones).
 *   - ESP32 / RTOS        -> el usuario inyecta lock()/unlock() en SystemApi
 *     (semafaros o portENTER_CRITICAL) para proteger colas entre ISRs.
 *   - Bare-metal single-thread / PC -> no-op.
 *
 * Prioridad del guard:
 *   1. Si SystemApi provee lock/unlock, usa esos callbacks (framework
 *      agnostico, adecuado para ESP-IDF, FreeRTOS, Zephyr, etc.).
 *   2. Si no, en AVR cae a cli()/sei(); en el resto no hace nada.
 *
 * Uso:
 *   {
 *       LockGuard lock;   // entra en seccion critica
 *       rxQueue_.push(frame);
 *   }                     // sale automaticamente
 */

#include "system_api.h"

#if defined(__AVR__)
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

namespace pcd {

class LockGuard {
  public:
    LockGuard()
        : lock_fn_(0),
          unlock_fn_(0),
          custom_(false)
#if defined(__AVR__)
          ,
          saved_sreg_(0)
#endif
    {
        const SystemApi *api = systemApi();
        if (api && api->hasLock()) {
            lock_fn_ = api->lock;
            unlock_fn_ = api->unlock;
            if (lock_fn_ != 0) {
                lock_fn_();
            }
            custom_ = true;
        } else {
#if defined(__AVR__)
            saved_sreg_ = SREG;
            cli();
#endif
        }
    }

    ~LockGuard() {
        if (custom_ && unlock_fn_ != 0) {
            unlock_fn_();
        } else {
#if defined(__AVR__)
            SREG = saved_sreg_;
#endif
        }
    }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

  private:
    void (*lock_fn_)();
    void (*unlock_fn_)();
    bool custom_;
#if defined(__AVR__)
    uint8_t saved_sreg_;
#endif
};

}  // namespace pcd
