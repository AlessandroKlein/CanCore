#pragma once

/*
 * Capa de aplicacion del protocolo PCD v1.
 *
 * CanNode implementa el comportamiento comun a todos los nodos del ecosistema:
 *
 *   - filtrado por destino propio y broadcast,
 *   - despacho de comandos a los recursos locales (Device Manager),
 *   - lazo comando -> ejecucion -> difusion de estado,
 *   - tabla de suscripciones a estados remotos (pantallas, gateways, reglas),
 *   - heartbeat periodico.
 *
 * No depende de ninguna plataforma: recibe un ICanBus ya construido.
 */

#include <stdint.h>

#include "can_protocol.h"
#include "device_manager.h"
#include "hal_can.h"
#include "system_config.h"

namespace pcd {

static const uint16_t kAnySource = 0x0000;

/* Notificacion de un estado remoto al que el nodo esta suscripto. */
typedef void (*StateListener)(const CanFrame &frame, float value, void *ctx);

/* Notificacion de tramas SDO de configuracion dirigidas a este nodo. */
typedef void (*ConfigListener)(const CanFrame &frame, void *ctx);

/* Observador de toda trama entrante (motor de reglas, registradores, puentes). */
typedef void (*FrameListener)(const CanFrame &frame, void *ctx);

struct Subscription {
    uint16_t source;
    uint8_t resource;
    uint8_t channel;
    StateListener listener;
    void *ctx;
};

class CanNode {
  public:
    CanNode(ICanBus &bus, uint16_t node_id);

    /* Inicializa el bus y emite un heartbeat inicial. */
    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE);

    uint16_t nodeId() const { return node_id_; }
    uint8_t address() const { return static_cast<uint8_t>(node_id_ & kTargetMask); }

    /* Registra un recurso local (rele, dimmer, cortina, ...). */
    bool registerResource(uint8_t resource, uint8_t channel, ResourceHandler handler,
                          void *ctx = 0);

    /* Gestor de recursos del nodo: rampas, temporizadores y valores actuales. */
    DeviceManager &devices() { return devices_; }
    const DeviceManager &devices() const { return devices_; }

    /* Suscribe el nodo a estados remotos; kAnySource / kAnyChannel actuan como comodin. */
    bool subscribe(uint16_t source, uint8_t resource, uint8_t channel, StateListener listener,
                   void *ctx = 0);

    void onConfig(ConfigListener listener, void *ctx = 0);

    /* Registra un observador de todas las tramas ajenas recibidas. */
    void onAnyFrame(FrameListener listener, void *ctx = 0);

    /* Procesa las tramas pendientes y emite el heartbeat cuando corresponde.
     * now_ms es el reloj monotono de la plataforma (millis() en Arduino). */
    void poll(uint32_t now_ms);

    /* Procesa una unica trama ya recibida. Devuelve true si le concierne. */
    bool handleFrame(const CanFrame &frame);

    /* Emite un comando dirigido a otro nodo (MSG_EVENT). */
    CanStatus sendCommand(uint8_t target, uint8_t resource, uint8_t channel, uint8_t action,
                          uint32_t param = 0);

    /* Difunde el estado de un recurso local (MSG_STATE broadcast). */
    CanStatus publishState(uint8_t resource, uint8_t channel, float value,
                           uint16_t flags = DIAG_NONE);

    /* Difunde telemetria de sensor con prioridad baja. */
    CanStatus publishTelemetry(uint8_t resource, uint8_t channel, float value,
                               uint16_t flags = DIAG_NONE);

    /* Difunde un evento de entrada digital (clic simple, doble, pulsacion larga). */
    CanStatus publishInputEvent(uint8_t channel, uint8_t event);

    /* Emite un SDO dirigido (MSG_CONFIG). */
    CanStatus sendConfig(uint8_t target, uint8_t sub_command, const uint8_t *payload,
                         uint8_t payload_len);

    /* Difunde la respuesta a un SDO (MSG_CONFIG con sub-comando CFG_ACK). */
    CanStatus sendConfigAck(const uint8_t *payload, uint8_t payload_len);

    CanStatus sendHeartbeat(uint32_t now_ms);

    /* Aplica localmente un comando (por ejemplo desde una tecla fisica) y
     * difunde el estado resultante, cerrando el lazo de realimentacion. */
    bool applyLocal(uint8_t resource, uint8_t channel, uint8_t action, uint32_t param = 0);

    uint8_t resourceCount() const { return devices_.count(); }
    uint8_t subscriptionCount() const { return subscription_count_; }

  private:
    bool isForThisNode(const CanId &id) const;
    void dispatchState(const CanFrame &frame);

    /* Puente entre los cambios autonomos del gestor de recursos y el bus. */
    static void emitState(uint8_t resource, uint8_t channel, float value, uint16_t flags,
                          void *ctx);

    ICanBus &bus_;
    uint16_t node_id_;
    uint32_t last_heartbeat_ms_;
    uint32_t boot_ms_;
    bool boot_ms_valid_;
    bool heartbeat_sent_;

    DeviceManager devices_;

    Subscription subscriptions_[CAN_MAX_SUBSCRIPTIONS];
    uint8_t subscription_count_;

    ConfigListener config_listener_;
    void *config_ctx_;

    FrameListener frame_listener_;
    void *frame_ctx_;
};

}  // namespace pcd
