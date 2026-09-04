#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bridge/bridge.h"

namespace pcd {

/* Contratos minimos: los SDK de CANopen/NMEA2000/KNX quedan en la aplicacion. */
struct ICanopenTransport {
    bool (*sendPdo)(uint16_t cob_id, const uint8_t *data, size_t len, void *ctx);
    void *ctx;
    ICanopenTransport() : sendPdo(0), ctx(0) {}
};

struct INmea2000Transport {
    bool (*sendPgn)(uint32_t pgn, uint8_t destination, const uint8_t *data,
                    size_t len, void *ctx);
    void *ctx;
    INmea2000Transport() : sendPgn(0), ctx(0) {}
};

class CanopenBridge : public IBridge {
  public:
    explicit CanopenBridge(ICanopenTransport &transport);
    void queueIncoming(uint16_t node_id, uint8_t resource, uint8_t channel,
                       uint8_t action, uint32_t param = 0);
    bool process(const CanonicalFrame &frame) override;
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

  private:
    ICanopenTransport &transport_;
    CanonicalFrame pending_[8];
    uint8_t pending_count_;
};

class Nmea2000Bridge : public IBridge {
  public:
    explicit Nmea2000Bridge(INmea2000Transport &transport);
    void queueIncoming(uint16_t source_id, uint8_t resource, uint8_t channel,
                       float value, uint16_t flags = DIAG_NONE);
    bool process(const CanonicalFrame &frame) override;
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

  private:
    INmea2000Transport &transport_;
    CanonicalFrame pending_[8];
    uint8_t pending_count_;
};

class KnxBridge : public IBridge {
  public:
    explicit KnxBridge(IKnxTransport &transport);
    void queueIncoming(const uint8_t *address, const uint8_t *data, size_t len,
                       uint8_t dpt, uint16_t source_id, uint8_t resource, uint8_t channel);
    bool process(const CanonicalFrame &frame) override;
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

  private:
    IKnxTransport &transport_;
    CanonicalFrame pending_[8];
    uint8_t pending_count_;
};

/* PGNs de referencia para recursos PCD; la aplicacion puede reemplazarlos. */
static const uint32_t kPgnPcdState = 130000UL;
static const uint16_t kCanopenPcdBaseCobId = 0x180;

}  // namespace pcd
