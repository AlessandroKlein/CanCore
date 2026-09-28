#pragma once

/*
 * PCD v6 - Identificacion de nodos y CAN ID de 29 bits.
 *
 * Layout del identificador extendido (v6):
 *
 *   bits 28..26 (3)  Prioridad      0 = critico ... 7 = trafico de fondo
 *   bits 25..21 (5)  Message Type   0..31
 *   bits 20..13 (8)  Destination    0x01..0xFE direccion; 0xFF = broadcast
 *   bits 12..5  (8)  Source         direccion del emisor dentro de la red
 *   bits 4..0   (5)  Network        0..31 (segmento / red logica)
 *
 *   Total: 3 + 5 + 8 + 8 + 5 = 29 bits.
 *
 * El Node-ID publico es de 16 bits: node_id = (network << 8) | source, de modo
 * que la direccion es local a la red y dos redes distintas pueden reutilizar la
 * misma direccion sin colision (ver segmentacion).
 *
 * Convencion de direcciones (minima reserva para no desperdiciar espacio):
 *
 *   0x00            PCD_NODE_INVALID   (no asignable)
 *   0x01 .. 0xEF    unicast de nodo
 *   0xF0 .. 0xFE    grupos multicast
 *   0xFF            PCD_NODE_BROADCAST
 */

#include <stdint.h>

namespace pcd {

static const uint8_t PCD_NODE_BROADCAST = 0xFF;
static const uint8_t PCD_NODE_INVALID = 0x00;
static const uint8_t PCD_MULTICAST_BASE = 0xF0;  /* 0xF0..0xFE son grupos */
static const uint8_t PCD_NETWORK_DEFAULT = 0x00;
static const uint8_t PCD_NETWORK_MAX = 0x1F;     /* 0..31 */

/* Desplazamientos de bit del identificador extendido. */
static const uint8_t kPcdPriorityShift = 26;
static const uint8_t kPcdMsgTypeShift = 21;
static const uint8_t kPcdDestinationShift = 13;
static const uint8_t kPcdSourceShift = 5;
static const uint8_t kPcdNetworkShift = 0;

static const uint32_t kPcdPriorityMask = 0x07UL;
static const uint32_t kPcdMsgTypeMask = 0x1FUL;
static const uint32_t kPcdDestinationMask = 0xFFUL;
static const uint32_t kPcdSourceMask = 0xFFUL;
static const uint32_t kPcdNetworkMask = 0x1FUL;

struct PCD_CAN_ID {
    uint8_t priority;      /* 3 bits: 0..7                     */
    uint8_t messageType;   /* 5 bits: 0..31                    */
    uint8_t destination;   /* 8 bits: direccion o grupo        */
    uint8_t source;        /* 8 bits: direccion del emisor     */
    uint8_t network;       /* 5 bits: red / segmento           */

    PCD_CAN_ID()
        : priority(0), messageType(0), destination(PCD_NODE_BROADCAST), source(0),
          network(PCD_NETWORK_DEFAULT) {}

    PCD_CAN_ID(uint8_t prio, uint8_t type, uint8_t dst, uint8_t src, uint8_t net)
        : priority(prio), messageType(type), destination(dst), source(src), network(net) {}

    /* Empaqueta en un identificador extendido de 29 bits. */
    uint32_t encode() const;

    /* Descompone un identificador extendido en sus campos. */
    static PCD_CAN_ID decode(uint32_t id);

    bool isBroadcast() const { return destination == PCD_NODE_BROADCAST; }
    bool isMulticast() const {
        return destination >= PCD_MULTICAST_BASE && destination < PCD_NODE_BROADCAST;
    }

    /* true si el mensaje va dirigido a este nodo (unicast o broadcast). */
    bool isForNode(uint8_t nodeAddress) const {
        return destination == nodeAddress || isBroadcast();
    }

    /* true si el mensaje pertenece a la red indicada. */
    bool isForNetwork(uint8_t networkId) const {
        return (network & PCD_NETWORK_MAX) == (networkId & PCD_NETWORK_MAX);
    }

    /* Node-ID publico de 16 bits: (network << 8) | source. */
    uint16_t nodeId() const;
};

/* Compone un Node-ID de 16 bits a partir de red + direccion. */
uint16_t pcdMakeNodeId(uint8_t network, uint8_t address);

/* Descompone un Node-ID en sus componentes. */
uint8_t pcdNodeNetwork(uint16_t node_id);
uint8_t pcdNodeAddress(uint16_t node_id);

/* true si la direccion es asignable (no reservada). */
bool pcdIsValidNodeAddress(uint8_t address);

}  // namespace pcd
