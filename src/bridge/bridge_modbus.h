#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bridge/bridge.h"

namespace pcd {

class ModbusBridge : public IBridge {
  public:
    ModbusBridge(IModbusRegisterMap &map, uint16_t base_address = 0x0100);

    void queueIncoming(uint16_t node_id, uint8_t resource, uint8_t channel,
                       uint8_t action, uint32_t param = 0);

    bool process(const CanonicalFrame &frame) override;
    bool buildCanonical(CanonicalFrame &out) override;
    bool available() const override;

  private:
    static uint16_t registerForResource(uint8_t resource, uint8_t channel);
    static uint16_t coilForResource(uint8_t resource, uint8_t channel);

    IModbusRegisterMap &map_;
    uint16_t base_address_;
    CanonicalFrame pending_[8];
    uint8_t pending_count_;
};

}  // namespace pcd
