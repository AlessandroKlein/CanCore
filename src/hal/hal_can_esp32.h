#pragma once

/* Driver CAN para ESP32 basado en el controlador TWAI integrado. */

#if defined(ARDUINO_ARCH_ESP32)

#include <stdint.h>

#include "hal_can.h"

namespace pcd {

class Esp32TwaiBus : public ICanBus {
  public:
    /* tx_pin / rx_pin: GPIO conectados al transceptor (ej. SN65HVD230). */
    Esp32TwaiBus(int tx_pin, int rx_pin);
    ~Esp32TwaiBus() override;

    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) override;
    void end() override;
    CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0) override;
    CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0) override;
    void setFilter(const CanFilter &filter) override;
    bool isBusOff() const override;
    CanStatus recover() override;

  private:
    int tx_pin_;
    int rx_pin_;
    bool started_;
    CanFilter filter_;
};

}  // namespace pcd

#endif  /* ARDUINO_ARCH_ESP32 */
