#pragma once

/*
 * PCD v6.3 - Estadisticas de bus, rate limiting y salud de red.
 *
 * PCD_BusStatistics agrega contadores de trafico y errores. PCD_RateLimiter es
 * un token-bucket por nodo para no saturar el bus (sensores, discovery, logs).
 * La salud de red es una clasificacion OPERACIONAL (no subjetiva) basada en
 * utilizacion, nodos offline y tasa de errores.
 */

#include <stdint.h>

namespace pcd {

struct PCD_BusStatistics {
    uint32_t framesTx;
    uint32_t framesRx;
    uint32_t errors;
    uint32_t retries;
    uint32_t dropped;
    uint32_t framesPerSec;     /* estimacion actual */
    uint32_t peakFramesPerSec;

    PCD_BusStatistics()
        : framesTx(0), framesRx(0), errors(0), retries(0), dropped(0), framesPerSec(0),
          peakFramesPerSec(0) {}

    /* Utilizacion porcentual estimada para un bitrate (bps) y 8 bytes/frame. */
    uint8_t utilizationPercent(uint32_t bitrate) const;
};

/* Token-bucket simple. */
class PCD_RateLimiter {
  public:
    PCD_RateLimiter();

    void configure(uint16_t maxFramesPerSecond, uint16_t burstLimit);

    /* true si se permite transmitir ahora. */
    bool allow(uint32_t nowMs);

    void reset(uint32_t nowMs);

  private:
    uint16_t maxPerSecond_;
    uint16_t burst_;
    uint16_t tokens_;
    uint32_t lastRefillMs_;
};

enum PCD_NetworkHealth : uint8_t {
    PCD_HEALTH_HEALTHY = 0,
    PCD_HEALTH_DEGRADED,
    PCD_HEALTH_CRITICAL
};

const char *pcdNetworkHealthName(PCD_NetworkHealth health);

/*
 * Clasifica la salud de red:
 *   utilizationPercent (0..100), offlineNodes, errorRate (errores/1000 frames).
 */
PCD_NetworkHealth pcdComputeNetworkHealth(uint8_t utilizationPercent, uint8_t offlineNodes,
                                          uint32_t errorRatePer1000);

}  // namespace pcd
