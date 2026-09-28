#pragma once

/*
 * PCD v7.0 - Analizador de trafico CAN.
 *
 * Herramienta de diagnostico: decodifica la actividad del bus y agrega
 * estadisticas (tramas/segundo, bytes/segundo, utilizacion, errores, nodos
 * activos). Sirve tanto en el firmware (panel de diagnostico) como en una
 * herramienta de PC que decodifique tramas capturadas.
 */

#include <stdint.h>

#include "v6/pcd_id.h"

namespace pcd {

class PCD_TrafficAnalyzer {
  public:
    static const uint8_t kMaxTrackedNodes = 16;

    PCD_TrafficAnalyzer();

    /* Registra una trama valida. */
    void record(const PCD_CAN_ID &id, uint8_t len, uint32_t nowMs);

    /* Registra un error (corrupcion, bus-off, etc.). */
    void recordError(uint32_t nowMs);

    uint32_t totalFrames() const { return total_frames_; }
    uint32_t totalBytes() const { return total_bytes_; }
    uint32_t totalErrors() const { return total_errors_; }

    /* Tramas del ultimo segundo. */
    uint32_t framesPerSec() const { return frames_this_second_; }

    uint8_t utilizationPercent(uint32_t bitrate) const;

    /* Nodos origen distintos vistos. */
    uint8_t activeNodeCount() const { return node_count_; }
    bool hasSeenNode(uint8_t address) const;

    void reset();

  private:
    void rollWindow(uint32_t nowMs);

    uint32_t total_frames_;
    uint32_t total_bytes_;
    uint32_t total_errors_;
    uint32_t frames_this_second_;
    uint32_t second_start_ms_;
    uint8_t seen_nodes_[kMaxTrackedNodes];
    uint8_t node_count_;
};

}  // namespace pcd
