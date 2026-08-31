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

#if CAN_LOG_LEVEL > 0
#if defined(ARDUINO)
#include <Arduino.h>
#define CAN_LOG_PRINT(...) Serial.printf(__VA_ARGS__)
#else
#include <cstdio>
#define CAN_LOG_PRINT(...) std::printf(__VA_ARGS__)
#endif
#else
#define CAN_LOG_PRINT(...) \
    do {                   \
    } while (0)
#endif

#if CAN_LOG_LEVEL >= 1
#define LOG_ERROR(fmt, ...) CAN_LOG_PRINT("[E] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) \
    do {                    \
    } while (0)
#endif

#if CAN_LOG_LEVEL >= 2
#define LOG_INFO(fmt, ...) CAN_LOG_PRINT("[I] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) \
    do {                   \
    } while (0)
#endif

#if CAN_LOG_LEVEL >= 3
#define LOG_DEBUG(fmt, ...) CAN_LOG_PRINT("[D] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) \
    do {                    \
    } while (0)
#endif
