#pragma once

/*
 * PCD v6.2 - Negociacion de capacidades y version de protocolo.
 *
 * Dos dispositivos con versiones distintas negocian el subconjunto comun de
 * funcionalidades en lugar de un simple "compatible / incompatible".
 */

#include <stdint.h>

namespace pcd {

/* Mapa de bits de funcionalidades del protocolo. */
enum PCD_FeatureFlag : uint32_t {
    PCD_FEATURE_DISCOVERY = 1u << 0,
    PCD_FEATURE_HEARTBEAT = 1u << 1,
    PCD_FEATURE_RESOURCE = 1u << 2,
    PCD_FEATURE_OTA = 1u << 3,
    PCD_FEATURE_DIAGNOSTICS = 1u << 4,
    PCD_FEATURE_AUTOMATION = 1u << 5,
    PCD_FEATURE_SCENES = 1u << 6,
    PCD_FEATURE_SECURITY = 1u << 7,
    PCD_FEATURE_GROUPS = 1u << 8,
    PCD_FEATURE_CONFIG = 1u << 9,
    PCD_FEATURE_SHADOW = 1u << 10,
    PCD_FEATURE_ALARM = 1u << 11
};

/* Interseccion de capacidades entre dos nodos. */
inline uint32_t pcdCommonFeatures(uint32_t a, uint32_t b) {
    return a & b;
}

/*
 * Negocia una version de protocolo comun.
 *
 *   aMajor/aMinor  version del dispositivo A
 *   bMajor/bMinor  version del dispositivo B
 *   outMajor/outMinor  version comun acordada (salida)
 *
 * Devuelve true si hay compatibilidad (misma major). La minor comun es el
 * minimo de ambas.
 */
bool pcdNegotiateProtocol(uint8_t aMajor, uint8_t aMinor, uint8_t bMajor, uint8_t bMinor,
                          uint8_t &outMajor, uint8_t &outMinor);

}  // namespace pcd
