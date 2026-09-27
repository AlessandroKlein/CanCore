#pragma once

/*
 * Perro guardian de red (Node Watchdog) con estado de falla seguro.
 *
 * Vigila los heartbeats de los nodos que le interesan: si un nodo de campo deja
 * de anunciarse, ejecuta una reaccion local (apagar cargas criticas, mantener la
 * ultima consigna segura, marcar el canal como no disponible...). El tiempo de
 * expiracion es configurable por nodo, de modo que un sensor de bateria pueda
 * tener una tolerancia distinta a la de un teclado.
 *
 * Es agnostico: no usa red ni memoria dinamica y sirve igual en un nodo de
 * campo (para vigilar al gateway) que en el gateway (para vigilar el campo).
 * El inventario completo con recursos es responsabilidad de NodeRegistry; este
 * watchdog solo responde a "quien dejo de hablar y que hago".
 */

#include <stdint.h>

#include "can_protocol.h"
#include "system_config.h"

namespace pcd {

#ifndef CAN_MAX_WATCHED_NODES
#define CAN_MAX_WATCHED_NODES 16
#endif

/* Reaccion de falla segura: se invoca una vez por transicion online -> offline. */
typedef void (*SafeStateHandler)(uint16_t node_id, uint32_t last_seen_ms, void *ctx);

struct WatchedNode {
    uint16_t node_id;
    uint32_t last_seen_ms;
    uint32_t timeout_ms;
    uint16_t missed;      /* ventanas de timeout vencidas desde el ultimo aviso */
    uint8_t health;       /* ultima bandera de salud (HealthFlag)               */
    bool online;
    bool ever_seen;
};

class NodeWatchdog {
  public:
    explicit NodeWatchdog(uint32_t default_timeout_ms = 3UL * CAN_HEARTBEAT_PERIOD_MS);

    /* Tolerancia por defecto para nodos nuevos. */
    void setDefaultTimeout(uint32_t timeout_ms);
    uint32_t defaultTimeout() const { return default_timeout_ms_; }

    /* Tolerancia especifica de un nodo (lo registra si no existe). */
    bool setTimeout(uint16_t node_id, uint32_t timeout_ms);

    /* Handler de falla segura invocado en cada expiracion. */
    void setSafeStateHandler(SafeStateHandler handler, void *ctx = 0);

    /* Alimenta el perro con una trama propia de un nodo remoto.
     * Acepta MSG_HEARTBEAT y el reporte de salud (MSG_STATE, RES_SYSTEM). */
    bool feed(const CanFrame &frame, uint32_t now_ms);

    /* Detecta expiraciones. Devuelve cuantas transiciones genero en esta llamada. */
    uint8_t poll(uint32_t now_ms);

    bool isOnline(uint16_t node_id) const;
    bool age(uint16_t node_id, uint32_t now_ms, uint32_t &age_ms) const;
    uint16_t missedWindows(uint16_t node_id) const;
    uint8_t healthFlags(uint16_t node_id) const;

    uint8_t count() const { return count_; }
    const WatchedNode *at(uint8_t index) const;
    const WatchedNode *find(uint16_t node_id) const;

    /* Olvida todos los nodos; el handler no se invoca. */
    void clear();

  private:
    WatchedNode *ensure(uint16_t node_id);

    WatchedNode nodes_[CAN_MAX_WATCHED_NODES];
    uint8_t count_;
    uint32_t default_timeout_ms_;
    SafeStateHandler handler_;
    void *handler_ctx_;
};

}  // namespace pcd
