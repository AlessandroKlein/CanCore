#pragma once

/*
 * PCD v6 - Descubrimiento y anuncio de nodos.
 *
 * El discovery funciona sin servidor externo: un nodo difunde DISCOVERY_REQUEST
 * y los demas responden con NODE_ANNOUNCEMENT (o lo emiten al arrancar). Un
 * nodo que se apaga limpiamente difunde NODE_LEAVE; la ausencia de heartbeat
 * la detecta el watchdog de red como caida silenciosa.
 *
 * PCD_NodeInfo es la identidad de un nodo. El anuncio completo (capacidades,
 * lista de recursos, baudrates, transporte) puede exceder una trama y se
 * transfiere con el descriptor de recurso (ver pcd_resource.h).
 */

#include <stdint.h>

namespace pcd {

/* Sub-comandos del mensaje de discovery. */
enum PCD_DiscoveryCommand : uint8_t {
    PCD_DISCOVERY_REQUEST = 0x01,
    PCD_DISCOVERY_RESPONSE = 0x02,
    PCD_NODE_ANNOUNCEMENT = 0x03,
    PCD_NODE_LEAVE = 0x04
};

/* Identidad minima de un nodo. */
struct PCD_NodeInfo {
    uint8_t nodeId;           /* direccion dentro de la red              */
    uint16_t manufacturerId;  /* fabricante del dispositivo              */
    uint16_t deviceType;      /* tipo de dispositivo (catalogo)          */
    uint16_t hardwareVersion; /* revision de hardware                    */
    uint16_t firmwareMajor;   /* version de firmware (mayor)             */
    uint16_t firmwareMinor;   /* version de firmware (menor)             */
    uint32_t capabilities;    /* mapa de bits PCD_Capability             */
    uint8_t networkId;        /* red / segmento al que pertenece         */

    PCD_NodeInfo()
        : nodeId(0), manufacturerId(0), deviceType(0), hardwareVersion(0),
          firmwareMajor(0), firmwareMinor(0), capabilities(0), networkId(0) {}
};

/* Banderas de capacidad de nodo (PCD_NodeInfo.capabilities). */
enum PCD_Capability : uint32_t {
    PCD_CAP_ROUTER = 1u << 0,
    PCD_CAP_GATEWAY = 1u << 1,
    PCD_CAP_OTA = 1u << 2,
    PCD_CAP_CRYPTO = 1u << 3,
    PCD_CAP_RTC = 1u << 4,
    PCD_CAP_TEMPERATURE_SENSOR = 1u << 5,
    PCD_CAP_DUAL_CAN = 1u << 6,
    PCD_CAP_EEPROM = 1u << 7,
    PCD_CAP_NVS = 1u << 8,
    PCD_CAP_SEGMENTS = 1u << 9
};

}  // namespace pcd
