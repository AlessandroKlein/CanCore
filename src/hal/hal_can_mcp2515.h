#pragma once

/*
 * Driver CAN para microcontroladores AVR (ATmega328P / ATmega2560) basado en el
 * controlador MCP2515 conectado por SPI. Implementacion propia y minima: solo
 * usa el buffer de transmision TXB0 y los dos buffers de recepcion, sin
 * dependencias externas, para no consumir Flash innecesaria.
 */

#if defined(__AVR__)

#include <stdint.h>

#include "hal_can.h"

namespace pcd {

/* Frecuencia del cristal del modulo MCP2515. */
enum Mcp2515Clock : uint8_t { MCP_CLOCK_8MHZ = 8, MCP_CLOCK_16MHZ = 16 };

class Mcp2515Bus : public ICanBus {
  public:
    Mcp2515Bus(uint8_t cs_pin, Mcp2515Clock clock = MCP_CLOCK_16MHZ, int8_t int_pin = -1);
    ~Mcp2515Bus() override;

    CanStatus begin(uint32_t bitrate = CAN_BUS_BITRATE) override;
    void end() override;
    CanStatus send(const CanFrame &frame, uint32_t timeout_ms = 0) override;
    CanStatus receive(CanFrame &frame, uint32_t timeout_ms = 0) override;
    void setFilter(const CanFilter &filter) override;
    bool isBusOff() const override;
    CanStatus recover() override;

  private:
    void select();
    void deselect();
    uint8_t transfer(uint8_t value);
    void reset();
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);
    void modifyRegister(uint8_t address, uint8_t mask, uint8_t value);
    uint8_t readStatus();
    bool setMode(uint8_t mode);
    bool configureBitrate(uint32_t bitrate);
    bool readRxBuffer(uint8_t buffer_index, CanFrame &frame);

    uint8_t cs_pin_;
    Mcp2515Clock clock_;
    int8_t int_pin_;
    bool started_;
    CanFilter filter_;
};

}  // namespace pcd

#endif  /* __AVR__ */
