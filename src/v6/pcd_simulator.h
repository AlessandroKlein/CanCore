#pragma once

/*
 * PCD v7.0 - Simulador de red.
 *
 * Orquesta una red virtual (PCD_MockNetwork) con hasta kMaxNodes nodos
 * PCD_MockCAN, permitiendo simular offline, perdida de paquetes, latencia,
 * errores CRC, reinicios y bus-off sin hardware.
 */

#include <stdint.h>

#include "v6/pcd_mock_can.h"

namespace pcd {

class PCD_NetworkSimulator {
  public:
    static const uint8_t kMaxNodes = 8;

    PCD_NetworkSimulator();

    /* Crea un nodo nuevo en la red y devuelve su transporte (0 si lleno). */
    PCD_MockCAN *addNode();

    /* Avanza la simulacion un tick. */
    void step(uint32_t nowMs);

    uint8_t nodeCount() const { return node_count_; }
    PCD_MockCAN *node(uint8_t index);
    PCD_MockNetwork &network() { return network_; }

  private:
    PCD_MockNetwork network_;
    PCD_MockCAN nodes_[kMaxNodes];
    uint8_t node_count_;
};

}  // namespace pcd
