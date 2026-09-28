#pragma once

/*
 * PCD v6 - Transporte CAN simulado (PCD_MockCAN).
 *
 * Permite probar la pila completa sin hardware: un PCD_MockNetwork conecta
 * varias instancias de PCD_MockCAN que se escuchan entre si. Cada nodo puede
 * inyectar fallas deterministas:
 *
 *   - perdida de paquetes (dropProbability),
 *   - duplicacion (duplicateProbability),
 *   - latencia fija (delayMs),
 *   - corrupcion de CRC de aplicacion (crcErrorProbability),
 *   - bus-off simulado (simulateBusOff),
 *   - desaparicion de nodo (clearRx),
 *   - timeouts (receive devuelve CAN_ERR_NO_DATA hasta que vence la latencia).
 *
 * El generador pseudoaleatorio (xorshift32) es determinista con la semilla de
 * FaultConfig, de modo que los tests son reproducibles.
 */

#include <stdint.h>

#include "hal_can.h"
#include "v6/pcd_diagnostics.h"

namespace pcd {

class PCD_MockCAN;

/* Red simulada que une multiples nodos mock. */
class PCD_MockNetwork {
  public:
    static const size_t kMaxNodes = 32;

    PCD_MockNetwork();

    void attach(PCD_MockCAN *node);
    void detach(PCD_MockCAN *node);

    /* Entrega la trama a todos los nodos excepto al emisor, aplicando a cada
     * receptor su propia configuracion de fallas. base_ms es el instante del
     * emisor; cada receptor le suma su latencia configurada. */
    void deliver(const CanFrame &frame, PCD_MockCAN *sender, uint32_t base_ms);

    size_t nodeCount() const { return node_count_; }

  private:
    PCD_MockCAN *nodes_[kMaxNodes];
    size_t node_count_;
};

class PCD_MockCAN : public ICanBus {
  public:
    struct FaultConfig {
        float dropProbability;        /* 0..1: probabilidad de perdida        */
        float duplicateProbability;   /* 0..1: probabilidad de duplicacion    */
        float crcErrorProbability;    /* 0..1: corromper un byte del payload  */
        uint32_t delayMs;             /* latencia fija antes de la entrega    */
        uint32_t seed;                /* semilla del PRNG deterministico      */

        FaultConfig()
            : dropProbability(0.0f), duplicateProbability(0.0f), crcErrorProbability(0.0f),
              delayMs(0), seed(1u) {}
    };

    explicit PCD_MockCAN(PCD_MockNetwork *network = 0);

    /* ICanBus */
    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) override;
    void end() override;
    CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0) override;
    CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0) override;
    void setFilter(const CanFilter &filter) override;
    bool isBusOff() const override;
    CanStatus recover() override;

    /* ------------------------------------------------------------------ */
    /* Control de simulacion                                              */
    /* ------------------------------------------------------------------ */

    /* Avanza el reloj de simulacion y promueve tramas retrasadas a listas. */
    void tick(uint32_t now_ms);

    /* Inyecta una trama como si llegara del bus (equivalente a un peer). */
    void injectRx(const CanFrame &frame);

    /* Recepcion desde la red simulada (aplica fallas de este receptor). */
    void deliverFromNetwork(const CanFrame &frame, uint32_t base_ms);

    /* Fuerza bus-off en el proximo envio. */
    void simulateBusOff() { bus_off_ = true; }

    /* Simula la desaparicion del nodo descartando las tramas pendientes. */
    void clearRx();

    /* Vincula el nodo a una red simulada (alternativa al constructor). */
    void setNetwork(PCD_MockNetwork *network);

    void setFaults(const FaultConfig &faults) { faults_ = faults; }
    const FaultConfig &faults() const { return faults_; }

    size_t rxPending() const { return rx_count_; }

    /* Contadores de diagnostico acumulados. */
    const PCD_CAN_Diagnostics &diagnostics() const { return diag_; }

  private:
    static const size_t kRxQueue = 16;

    struct RxEntry {
        CanFrame frame;
        uint32_t deliver_ms;
        bool ready;
    };

    uint32_t nextRandom();
    float randomFloat();
    bool shouldDrop();
    bool shouldDuplicate();
    bool shouldCorrupt();
    void enqueue(const CanFrame &frame, uint32_t deliver_ms);

    PCD_MockNetwork *net_;
    CanFilter filter_;
    bool started_;
    bool bus_off_;
    FaultConfig faults_;
    uint32_t now_ms_;
    uint32_t rng_;

    RxEntry rx_[kRxQueue];
    size_t rx_count_;

    PCD_CAN_Diagnostics diag_;
};

}  // namespace pcd
