#pragma once

/*
 * Hub / star coupler CAN multi-puerto (Tree Extension).
 *
 * Es el nucleo software del nodo "Hub Repetidor" del PRD: un microcontrolador
 * con varios transceptores CAN, uno por ramal, que reenvia tramas entre ramales
 * aislados galvanicamente. Cada ramal es un bus lineal propio con sus dos
 * terminaciones de 120 ohm, de modo que la topologia global puede ser estrella
 * o arbol sin que las reflexiones de una rama afecten a las otras.
 *
 * Caracteristicas:
 *
 *   - un ICanBus por puerto (TWAI, MCP2515, bus virtual en pruebas),
 *   - reenvio con anti-bucle por huella temporal de trama (dedupe),
 *   - modo FLOOD (copia a todos los ramales) y modo LEARNED (aprende que
 *     direccion vive en cada ramal a partir del origen de las tramas y enruta
 *     los comandos dirigidos solo hacia el ramal correcto),
 *   - recuperacion automatica de bus-off por puerto y estadisticas por ramal,
 *   - sin memoria dinamica: tablas de tamano fijo.
 *
 * La topologia con lazos (dos hubs uniendo los mismos ramales) esta prohibida;
 * si ocurre, el dedupe la contiene pero el trafico se degrada.
 */

#include <stdint.h>

#include "can_protocol.h"
#include "hal_can.h"
#include "system_config.h"
#include "topology/bus_profile.h"

namespace pcd {

#ifndef CAN_HUB_MAX_PORTS
#define CAN_HUB_MAX_PORTS 4
#endif

#ifndef CAN_HUB_DEDUPE_SLOTS
#define CAN_HUB_DEDUPE_SLOTS 16
#endif

#ifndef CAN_HUB_LEARN_SLOTS
#define CAN_HUB_LEARN_SLOTS 16
#endif

/* Ventana anti-bucle por defecto: un lazo se cierra en menos de un milisegundo. */
static const uint32_t kHubDefaultDedupeMs = 20;

/* Tiempo de validez del aprendizaje de direccion -> ramal. */
static const uint32_t kHubLearnHoldMs = 30000;

enum HubRouteMode : uint8_t {
    HUB_ROUTE_FLOOD = 0,   /* reenvia toda trama a los demas ramales        */
    HUB_ROUTE_LEARNED = 1  /* enruta los comandos dirigidos al ramal sabido */
};

struct CanHubPortStats {
    uint32_t received;
    uint32_t forwarded;
    uint32_t dropped;
    uint32_t errors;
};

struct CanHubPort {
    ICanBus *bus;
    uint8_t segment;
    CanHubPortStats stats;
};

class CanTreeHub {
  public:
    explicit CanTreeHub(uint32_t bitrate = CAN_BUS_BITRATE);
    ~CanTreeHub();

    /* Registra un ramal. `segment` es el identificador logico del ramal. */
    bool addPort(ICanBus &bus, uint8_t segment);

    /* Inicializa todos los puertos. bitrate 0 usa el del constructor. */
    bool begin(uint32_t bitrate = 0);
    void end();

    /* Procesa todos los puertos: recibe, aprende y reenvia. */
    void poll(uint32_t now_ms);

    /* Procesa una trama ya recibida de `origin_port` (uso avanzado/pruebas). */
    bool handleFrame(uint8_t origin_port, const CanFrame &frame, uint32_t now_ms);

    void setRouteMode(HubRouteMode mode) { route_mode_ = mode; }
    HubRouteMode routeMode() const { return route_mode_; }
    void setDedupeWindow(uint32_t window_ms) { dedupe_ms_ = window_ms; }
    uint32_t dedupeWindow() const { return dedupe_ms_; }

    uint8_t portCount() const { return port_count_; }
    const CanHubPort *port(uint8_t index) const;
    ICanBus *portBus(uint8_t segment) const;

    uint32_t forwardedCount() const { return forwarded_; }
    uint32_t droppedCount() const { return dropped_; }
    uint32_t loopSuppressed() const { return loops_; }

    /* Ramal aprendido para una direccion; -1 (0xFF) si se desconoce. */
    int8_t learnedPort(uint8_t address) const;

    /* Devuelve el puerto al estado inicial sin tocar el hardware. */
    void clear();

  private:
    static const int8_t kUnknownPort = -1;

    uint32_t fingerprint(const CanFrame &frame) const;
    bool recentlySeen(uint32_t fingerprint, uint32_t now_ms);
    void learn(uint16_t node_id, uint8_t port);
    void expireLearning(uint32_t now_ms);
    void forward(uint8_t origin_port, const CanFrame &frame, const CanId &id);

    ICanBus *buses_[CAN_HUB_MAX_PORTS];
    CanHubPort ports_[CAN_HUB_MAX_PORTS];
    uint8_t port_count_;

    struct LearnEntry {
        uint8_t address;
        uint8_t port;
        uint32_t seen_ms;
        bool valid;
    };
    LearnEntry learned_[CAN_HUB_LEARN_SLOTS];
    uint8_t learn_next_;

    struct DedupeEntry {
        uint32_t fingerprint;
        uint32_t seen_ms;
        bool valid;
    };
    DedupeEntry dedupe_[CAN_HUB_DEDUPE_SLOTS];
    uint8_t dedupe_next_;

    uint32_t bitrate_;
    uint32_t dedupe_ms_;
    HubRouteMode route_mode_;
    uint32_t forwarded_;
    uint32_t dropped_;
    uint32_t loops_;
    bool started_;
};

}  // namespace pcd
