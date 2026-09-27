#pragma once

/*
 * Perfiles de capa fisica CAN por topologia.
 *
 * El CAN clasico (ISO 11898-2) exige bus lineal con dos terminaciones de
 * 120 ohm en los extremos. Una topologia en estrella o en arbol no se resuelve
 * cambiando el controlador: se resuelve aislando cada rama con su propio
 * transceptor (star coupler / repetidor de capa fisica) y bajando la velocidad.
 *
 * Este modulo concentra los numeros de instalacion (derivacion maxima, longitud,
 * terminaciones, necesidad de hub) para que el firmware pueda:
 *
 *   - elegir la velocidad del bus segun la topologia del proyecto,
 *   - validar el cableado declarado por el instalador antes de energizar,
 *   - documentar por que una rama no terminada solo es segura a baja velocidad.
 *
 * Los valores de `max_stub_cm` son deliberadamente conservadores (peor caso de
 * reflexion con transceptores economicos). El fabricante del transceptor o del
 * cable puede permitir derivaciones mas largas; la wiki 10 reproduce ambas
 * tablas y explica el criterio.
 */

#include <stdint.h>

namespace pcd {

enum BusTopology : uint8_t {
    BUS_TOPOLOGY_LINEAR = 0,  /* tronco unico, terminado en los dos extremos     */
    BUS_TOPOLOGY_STAR = 1,    /* varias ramas cortas sobre un star coupler       */
    BUS_TOPOLOGY_TREE = 2     /* ramas y sub-ramas sobre un hub repetidor        */
};

struct BusProfile {
    const char *name;
    BusTopology topology;
    uint32_t bitrate;             /* bits por segundo                          */
    uint16_t max_stub_cm;         /* derivacion maxima por nodo, en cm         */
    uint32_t max_total_stub_cm;   /* suma de todas las derivaciones, en cm     */
    uint32_t max_length_m;        /* longitud maxima del tramo o de la rama    */
    uint8_t terminations;         /* 120 ohm requeridas por rama               */
    bool requires_hub;            /* true si exige star coupler/repetidor      */
    bool recommended;             /* false: posible pero fuera de lo prudente  */
};

/* Tabla de perfiles incluida en la libreria. */
uint8_t busProfileCount();
const BusProfile *busProfileAt(uint8_t index);

/* Perfil exacto para la combinacion pedida (0 si no existe). */
const BusProfile *findBusProfile(BusTopology topology, uint32_t bitrate);

/* Perfil recomendado mas rapido de una topologia (0 si no hay). */
const BusProfile *recommendedProfile(BusTopology topology);

/* true si la combinacion topologia + velocidad esta soportada. */
bool topologySupports(BusTopology topology, uint32_t bitrate);

/* Derivacion maxima (cm) para la combinacion; 0 si no esta soportada. */
uint16_t maxStubCm(BusTopology topology, uint32_t bitrate);

/* true si la topologia exige un hub/star coupler con un transceptor por rama. */
bool topologyRequiresHub(BusTopology topology);

const char *topologyName(BusTopology topology);

/* ------------------------------------------------------------------ */
/* Validador de instalacion                                             */
/* ------------------------------------------------------------------ */

enum BusWiringIssue : uint8_t {
    BUS_WIRING_OK = 0,
    BUS_WIRING_UNKNOWN_PROFILE,
    BUS_WIRING_STUB_TOO_LONG,
    BUS_WIRING_TOTAL_STUB_TOO_LONG,
    BUS_WIRING_TOO_LONG,
    BUS_WIRING_MISSING_HUB,
    BUS_WIRING_BAD_TERMINATION
};

/*
 * Valida el cableado declarado:
 *
 *   stub_cm        derivacion mas larga de la rama
 *   total_stub_cm  suma de todas las derivaciones de la rama
 *   length_m       longitud del tramo (lineal) o de la rama (estrella/arbol)
 *   terminations   resistencias de 120 ohm presentes en la rama
 *   has_hub        true si la rama cuelga de un star coupler
 */
BusWiringIssue validateWiring(BusTopology topology, uint32_t bitrate, uint32_t stub_cm,
                              uint32_t total_stub_cm, uint32_t length_m, uint8_t terminations,
                              bool has_hub);

const char *wiringIssueName(BusWiringIssue issue);

}  // namespace pcd
