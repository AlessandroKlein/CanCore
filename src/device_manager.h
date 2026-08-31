#pragma once

/*
 * Gestor de recursos heterogeneos de un nodo (Device Manager).
 *
 * Un nodo no es "un interruptor": agrupa canales de distinto tipo bajo un mismo
 * Node-ID y el protocolo direcciona la terna (Node-ID, Tipo de Recurso, Canal).
 *
 * Cada canal se administra con una maquina de estados propia y no bloqueante:
 * los temporizadores de apagado y las rampas de dimmer avanzan en update() sin
 * usar delay(). Toda la memoria es estatica (sin malloc/new) para no fragmentar
 * el heap en AVR.
 */

#include <stdint.h>

#include "can_protocol.h"
#include "system_config.h"

namespace pcd {

static const uint8_t kAnyChannel = 0xFF;

#ifndef CAN_MAX_CHANNELS
#define CAN_MAX_CHANNELS CAN_MAX_SUBSCRIPTIONS
#endif

/*
 * Manejador fisico de un canal. Recibe la accion y el parametro ya decodificados
 * y debe aplicar el cambio sobre el hardware, devolviendo en out_value el valor
 * resultante (1.0 rele encendido, 0..100 dimmer, posicion de cortina, ...).
 * Devolver false rechaza el comando y evita la difusion de estado.
 */
typedef bool (*ResourceHandler)(uint8_t channel, uint8_t action, uint32_t param, float &out_value,
                                void *ctx);

/* Emisor de estado usado por el gestor cuando un canal cambia solo (temporizador,
 * rampa de dimmer) y hay que cerrar el lazo de realimentacion. */
typedef void (*StateEmitter)(uint8_t resource, uint8_t channel, float value, uint16_t flags,
                             void *ctx);

enum ChannelPhase : uint8_t {
    PHASE_IDLE = 0,   /* valor estable                                   */
    PHASE_RAMPING,    /* transicion progresiva hacia target_value        */
    PHASE_TIMED       /* valor estable con apagado automatico programado */
};

struct Channel {
    uint8_t resource;
    uint8_t channel;
    ResourceHandler handler;
    void *ctx;

    float value;          /* valor actual del recurso                       */
    float start_value;    /* valor al comenzar la rampa                     */
    float target_value;   /* destino de la rampa en curso                   */
    uint16_t flags;       /* banderas de diagnostico del canal              */
    uint8_t phase;
    bool phase_started;   /* la fase ya tomo su referencia temporal         */
    uint32_t start_ms;    /* instante en que arranco la fase                */
    uint32_t duration_ms; /* duracion de la rampa o del temporizador        */
};

class DeviceManager {
  public:
    DeviceManager();

    /* Registra un canal fisico. channel == kAnyChannel acepta cualquier indice. */
    bool registerChannel(uint8_t resource, uint8_t channel, ResourceHandler handler,
                         void *ctx = 0);

    Channel *find(uint8_t resource, uint8_t channel);
    const Channel *find(uint8_t resource, uint8_t channel) const;

    /*
     * Aplica un comando sobre un canal local.
     *
     *   ACT_ON / ACT_OFF / ACT_TOGGLE : param = temporizador de apagado en ms (0 = sin limite)
     *   ACT_SET_VALUE                 : param = valor << 16 | tiempo de fade en ms
     *
     * Devuelve true si el canal existe y el manejador acepto el comando; en ese
     * caso out_value contiene el valor resultante.
     */
    bool apply(uint8_t resource, uint8_t channel, uint8_t action, uint32_t param,
               float &out_value);

    /* Cancela la rampa o el temporizador pendiente de un canal. */
    void cancelPending(uint8_t resource, uint8_t channel);

    /* Avanza las maquinas de estado (rampas y temporizadores). Emite estado por
     * cada cambio autonomo a traves del StateEmitter registrado. */
    void update(uint32_t now_ms);

    void setStateEmitter(StateEmitter emitter, void *ctx);

    /* Marca banderas de diagnostico de un canal (error de sensor, sobretemperatura). */
    void setFlags(uint8_t resource, uint8_t channel, uint16_t flags);
    uint16_t flags(uint8_t resource, uint8_t channel) const;

    bool value(uint8_t resource, uint8_t channel, float &out_value) const;
    uint8_t count() const { return count_; }
    const Channel &at(uint8_t index) const { return channels_[index]; }

  private:
    bool drive(Channel &entry, uint8_t channel, uint8_t action, uint32_t param, float &out_value);

    Channel channels_[CAN_MAX_CHANNELS];
    uint8_t count_;
    StateEmitter emitter_;
    void *emitter_ctx_;
};

/* Empaqueta valor y tiempo de fade en el parametro de ACT_SET_VALUE. */
uint32_t packSetValue(uint16_t value, uint16_t fade_ms);
uint16_t unpackSetValueTarget(uint32_t param);
uint16_t unpackSetValueFade(uint32_t param);

}  // namespace pcd
