#pragma once

/*
 * PCD v6 - Sistema de errores normalizado.
 *
 * Las APIs de v6 no devuelven unicamente `bool`: devuelven un PCD_Error que
 * explica la causa. PCD_OK == 0 es el exito. Los codigos son estables y se
 * documentan para que un gateway pueda traducirlos a un reporte de diagnostico
 * o a una respuesta de error de protocolo.
 */

#include <stdint.h>

namespace pcd {

enum PCD_Error : uint8_t {
    PCD_OK = 0,
    PCD_ERR_INVALID_ARGUMENT,  /* parametro fuera de rango o puntero nulo   */
    PCD_ERR_INVALID_ID,        /* Node-ID / direccion reservada o invalida  */
    PCD_ERR_INVALID_PAYLOAD,   /* formato de payload incorrecto             */
    PCD_ERR_TIMEOUT,           /* expiro el tiempo de espera de respuesta   */
    PCD_ERR_BUSY,              /* recurso ocupado (transferencia en curso)  */
    PCD_ERR_NOT_SUPPORTED,     /* operacion no soportada por la plataforma  */
    PCD_ERR_NO_MEMORY,         /* tabla/cola/descriptor lleno               */
    PCD_ERR_CRC,               /* verificacion de integridad fallida        */
    PCD_ERR_BUS_OFF,           /* bus CAN en bus-off                        */
    PCD_ERR_OFFLINE,           /* nodo destino sin heartbeat (offline)      */
    PCD_ERR_AUTH,              /* autenticacion/integridad rechazada        */
    PCD_ERR_VERSION,           /* incompatibilidad de version de protocolo  */
    PCD_ERR_HARDWARE,          /* fallo de hardware concreto                */
    PCD_ERR_COUNT
};

/* Nombre legible del codigo de error (para logs y reportes). */
const char *pcdErrorName(PCD_Error error);

/* true si el codigo representa un exito (solo PCD_OK). */
inline bool pcdIsOk(PCD_Error error) { return error == PCD_OK; }

}  // namespace pcd
