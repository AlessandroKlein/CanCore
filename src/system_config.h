#pragma once

/*
 * Configuracion global del ecosistema CAN Bus (PCD v1).
 * Todas las caracteristicas se activan con banderas 0/1 para que el compilador
 * elimine por completo el codigo no utilizado en plataformas con poca Flash/RAM.
 */

#ifndef FEATURE_OTA_MANAGER
#define FEATURE_OTA_MANAGER 0
#endif

#ifndef FEATURE_WEB_SERVER
#define FEATURE_WEB_SERVER 0
#endif

#ifndef FEATURE_MQTT_BRIDGE
#define FEATURE_MQTT_BRIDGE 0
#endif

#ifndef FEATURE_MODBUS_BRIDGE
#define FEATURE_MODBUS_BRIDGE 0
#endif

#ifndef FEATURE_MODBUS_TCP
#define FEATURE_MODBUS_TCP 0
#endif

#ifndef FEATURE_TUNNEL_BRIDGE
#define FEATURE_TUNNEL_BRIDGE 0
#endif

/* Nivel de trazas: 0 = produccion (sin strings en Flash), 1 = error, 2 = info, 3 = debug */
#ifndef CAN_LOG_LEVEL
#define CAN_LOG_LEVEL 0
#endif

/* Velocidad nominal del bus CAN en bits por segundo. */
#ifndef CAN_BUS_BITRATE
#define CAN_BUS_BITRATE 500000UL
#endif

/* Periodo de emision del heartbeat de nodo, en milisegundos. */
#ifndef CAN_HEARTBEAT_PERIOD_MS
#define CAN_HEARTBEAT_PERIOD_MS 5000UL
#endif

/* Cantidad maxima de suscripciones y recursos registrables por nodo. */
#ifndef CAN_MAX_SUBSCRIPTIONS
#define CAN_MAX_SUBSCRIPTIONS 16
#endif

/* La libreria base es 100% agnostica al framework: no incluye <Arduino.h>.
 * El reloj, el bloqueo y el diagnostico se inyectan via SystemApi.
 * Los macros LOG_* provienen de system/system_logger.h. */
#include "system/system_api.h"
#include "system/system_logger.h"
