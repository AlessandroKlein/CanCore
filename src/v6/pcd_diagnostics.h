#pragma once

/*
 * PCD v6 - Diagnostico abstracto del bus CAN y salud del nodo.
 *
 * PCD_CAN_Diagnostics agrega contadores que el backend de hardware informa
 * cuando puede. Los campos TEC/REC (0xFF = no disponible) nunca se inventan:
 * si el controlador no los expone, quedan en 0xFF.
 *
 * PCD_CAN_State modela la maquina de estados del controlador (ISO 11898) para
 * la recuperacion de bus-off.
 */

#include <stdint.h>

namespace pcd {

/* Estado del controlador CAN. */
enum PCD_CAN_State : uint8_t {
    PCD_CAN_STATE_BUS_OK = 0,
    PCD_CAN_STATE_ERROR_WARNING,
    PCD_CAN_STATE_ERROR_PASSIVE,
    PCD_CAN_STATE_BUS_OFF,
    PCD_CAN_STATE_RECOVERING
};

const char *pcdCanStateName(PCD_CAN_State state);

/* Contadores de diagnostico. `unavailable` marca campos no medibles. */
static const uint8_t PCD_DIAG_UNAVAILABLE = 0xFF;

struct PCD_CAN_Diagnostics {
    uint32_t txCount;            /* tramas transmitidas con exito          */
    uint32_t rxCount;            /* tramas recibidas validas               */
    uint32_t txErrorCount;       /* fallos de transmision                  */
    uint32_t rxErrorCount;       /* fallos de recepcion                    */
    uint32_t arbitrationLost;    /* perdidas de arbitraje                  */
    uint32_t busOffCount;        /* veces que entro en bus-off             */
    uint32_t errorPassiveCount;  /* transiciones a error-passive           */
    uint32_t errorWarningCount;  /* transiciones a error-warning           */
    uint32_t droppedFrames;      /* tramas descartadas por cola llena      */
    uint8_t tec;                 /* transmit error counter (0xFF = n/d)    */
    uint8_t rec;                 /* receive error counter  (0xFF = n/d)    */

    PCD_CAN_Diagnostics()
        : txCount(0), rxCount(0), txErrorCount(0), rxErrorCount(0), arbitrationLost(0),
          busOffCount(0), errorPassiveCount(0), errorWarningCount(0), droppedFrames(0),
          tec(PCD_DIAG_UNAVAILABLE), rec(PCD_DIAG_UNAVAILABLE) {}

    bool hasTecRec() const { return tec != PCD_DIAG_UNAVAILABLE && rec != PCD_DIAG_UNAVAILABLE; }
};

/* Banderas de salud del nodo (v6). */
enum PCD_HealthFlag : uint8_t {
    PCD_HEALTH_CAN_OK = 0x00,        /* sin fallas                         */
    PCD_HEALTH_CAN_ERROR = 0x01,     /* bus con errores / bus-off          */
    PCD_HEALTH_LOW_MEMORY = 0x02,    /* memoria RAM/flash critica          */
    PCD_HEALTH_WATCHDOG = 0x04,      /* reinicio por watchdog              */
    PCD_HEALTH_BROWNOUT = 0x08,      /* reinicio por caida de tension      */
    PCD_HEALTH_SENSOR_ERROR = 0x10,  /* fallo de un sensor local           */
    PCD_HEALTH_CONFIG_ERROR = 0x20,  /* configuracion corrupta/perdida     */
    PCD_HEALTH_BOOTLOADER = 0x40     /* ejecutando desde bootloader        */
};

/* Configuracion de recuperacion de bus-off. */
struct PCD_BusOffConfig {
    bool autoBusOffRecovery;    /* reingreso automatico al bus          */
    uint32_t recoveryDelayMs;   /* espera entre intentos               */
    uint8_t maxRecoveryAttempts;/* 0 = infinito                        */

    PCD_BusOffConfig()
        : autoBusOffRecovery(true), recoveryDelayMs(200), maxRecoveryAttempts(5) {}
};

}  // namespace pcd
