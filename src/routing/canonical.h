#pragma once

/*
 * Estructura canonica interna (Internal Canonical Frame).
 *
 * Es el modelo de datos neutro que usa el motor de enrutamiento: todo paquete
 * entrante (CAN, ESP-NOW, Modbus, KNX, MQTT, IP) se traduce a una CanonicalFrame
 * antes de evaluar las rutas. Asi el enrutador no conoce los detalles de cada
 * protocolo; solo sabe "algo cambio en el canal X del recurso Y del origen Z".
 */

#include <stdint.h>

#include "can_protocol.h"

namespace pcd {

/* Protocolos de origen que puede traer una CanonicalFrame. */
enum ProtocolSource : uint8_t {
    PROTO_CAN = 0,
    PROTO_ESP_NOW = 1,
    PROTO_MODBUS = 2,
    PROTO_KNX = 3,
    PROTO_IP = 4,
    PROTO_MQTT = 5
    ,PROTO_CANOPEN = 6
    ,PROTO_NMEA2000 = 7
    ,PROTO_MATTER = 8
    ,PROTO_ZIGBEE = 9
    ,PROTO_ESPHOME = 10
};

/*
 * Representacion normalizada de un evento, estado o comando.
 *
 *   protocol   - de donde vino (CAN, Modbus, KNX, ...)
 *   source_id  - direccion del emisor segun su protocolo (Node-ID CAN,
 *                identificador Modbus, direccion fisica KNX, MAC ESP-NOW...)
 *   resource   - ResourceType (rele, dimmer, entrada, sensor...)
 *   channel    - indice de canal dentro del nodo/dispositivo
 *   value      - valor normalizado (1.0 ON, 0..100 dimmer, temperatura...)
 *   action     - accion si es un comando (ACT_ON, ACT_OFF, ACT_TOGGLE...)
 *   param      - parametro de comando (temporizador, fade, consigna...)
 *   flags      - DiagFlag (error de sensor, sobretemperatura...)
 *   is_command - 1 si es un comando/accion, 0 si es un estado/telemetria
 */
struct CanonicalFrame {
    uint8_t protocol;
    uint16_t source_id;
    uint8_t resource;
    uint8_t channel;
    float value;
    uint8_t action;
    uint32_t param;
    uint16_t flags;
    uint8_t is_command;

    CanonicalFrame()
        : protocol(PROTO_CAN),
          source_id(0),
          resource(0),
          channel(0),
          value(0.0f),
          action(ACT_TOGGLE),
          param(0),
          flags(DIAG_NONE),
          is_command(0) {}

    /* Convierte una trama CAN a la estructura canonica. */
    static CanonicalFrame fromCanFrame(const CanFrame &frame) {
        CanonicalFrame c;
        const CanId id = frame.fields();
        c.protocol = PROTO_CAN;
        c.source_id = id.source;
        c.resource = frame.resource();
        c.channel = frame.channel();
        if (id.msg_type == MSG_STATE) {
            c.is_command = 0;
            c.value = frameValue(frame);
            c.flags = frameFlags(frame);
        } else if (id.msg_type == MSG_EVENT) {
            c.is_command = 1;
            c.action = frame.data[2];
            c.param = frameParam(frame);
        }
        return c;
    }

    /* Construye un comando CAN dirigido a `target` a partir del canonico. */
    CanFrame toCanCommand(uint16_t target) const {
        return makeCommand(source_id, static_cast<uint8_t>(target & kTargetMask),
                           resource, channel, action, param);
    }

    /* Construye un broadcast de estado CAN (MSG_STATE) a partir del canonico. */
    CanFrame toCanStateBroadcast() const {
        return makeStateBroadcast(source_id, resource, channel, value, flags);
    }
};

}  // namespace pcd
