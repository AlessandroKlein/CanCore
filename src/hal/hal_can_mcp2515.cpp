#if defined(__AVR__)

#include "hal/hal_can_mcp2515.h"

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

namespace pcd {

namespace {

/* Instrucciones SPI del MCP2515 */
const uint8_t CMD_RESET = 0xC0;
const uint8_t CMD_READ = 0x03;
const uint8_t CMD_WRITE = 0x02;
const uint8_t CMD_RTS_TXB0 = 0x81;
const uint8_t CMD_READ_STATUS = 0xA0;
const uint8_t CMD_BIT_MODIFY = 0x05;

/* Registros */
const uint8_t REG_CANCTRL = 0x0F;
const uint8_t REG_CANSTAT = 0x0E;
const uint8_t REG_CNF3 = 0x28;
const uint8_t REG_CNF2 = 0x29;
const uint8_t REG_CNF1 = 0x2A;
const uint8_t REG_CANINTE = 0x2B;
const uint8_t REG_CANINTF = 0x2C;
const uint8_t REG_EFLG = 0x2D;
const uint8_t REG_TXB0CTRL = 0x30;
const uint8_t REG_TXB0SIDH = 0x31;
const uint8_t REG_RXB0CTRL = 0x60;
const uint8_t REG_RXB0SIDH = 0x61;
const uint8_t REG_RXB1CTRL = 0x70;
const uint8_t REG_RXB1SIDH = 0x71;
const uint8_t REG_RXM0SIDH = 0x20;
const uint8_t REG_RXM1SIDH = 0x24;

/* Modos de operacion (bits 7..5 de CANCTRL) */
const uint8_t MODE_NORMAL = 0x00;
const uint8_t MODE_CONFIG = 0x80;
const uint8_t MODE_MASK = 0xE0;

const uint8_t EFLG_TXBO = 0x20;
const uint8_t CANINTF_RX0IF = 0x01;
const uint8_t CANINTF_RX1IF = 0x02;
const uint8_t TXB0CTRL_TXREQ = 0x08;

const SPISettings kSpiSettings(10000000, MSBFIRST, SPI_MODE0);

struct BitTiming {
    uint8_t cnf1;
    uint8_t cnf2;
    uint8_t cnf3;
};

bool timingFor(uint32_t bitrate, Mcp2515Clock clock, BitTiming &out) {
    if (clock == MCP_CLOCK_16MHZ) {
        switch (bitrate) {
            case 1000000UL: out.cnf1 = 0x00; out.cnf2 = 0xD0; out.cnf3 = 0x82; return true;
            case 500000UL:  out.cnf1 = 0x00; out.cnf2 = 0xF0; out.cnf3 = 0x86; return true;
            case 250000UL:  out.cnf1 = 0x41; out.cnf2 = 0xF1; out.cnf3 = 0x85; return true;
            case 125000UL:  out.cnf1 = 0x03; out.cnf2 = 0xF0; out.cnf3 = 0x86; return true;
            default: return false;
        }
    }
    switch (bitrate) {
        case 500000UL: out.cnf1 = 0x00; out.cnf2 = 0x90; out.cnf3 = 0x82; return true;
        case 250000UL: out.cnf1 = 0x00; out.cnf2 = 0xB1; out.cnf3 = 0x85; return true;
        case 125000UL: out.cnf1 = 0x01; out.cnf2 = 0x90; out.cnf3 = 0x82; return true;
        default: return false;
    }
}

void encodeExtendedId(uint32_t id, uint8_t *out) {
    out[0] = static_cast<uint8_t>(id >> 21);                                 /* SIDH */
    out[1] = static_cast<uint8_t>(((id >> 13) & 0xE0) | 0x08 | ((id >> 16) & 0x03)); /* SIDL */
    out[2] = static_cast<uint8_t>(id >> 8);                                  /* EID8 */
    out[3] = static_cast<uint8_t>(id);                                       /* EID0 */
}

uint32_t decodeExtendedId(const uint8_t *raw) {
    uint32_t id = static_cast<uint32_t>(raw[0]) << 21;
    id |= static_cast<uint32_t>(raw[1] & 0xE0) << 13;
    id |= static_cast<uint32_t>(raw[1] & 0x03) << 16;
    id |= static_cast<uint32_t>(raw[2]) << 8;
    id |= static_cast<uint32_t>(raw[3]);
    return id;
}

}  // namespace

Mcp2515Bus::Mcp2515Bus(uint8_t cs_pin, Mcp2515Clock clock, int8_t int_pin)
    : cs_pin_(cs_pin),
      clock_(clock),
      int_pin_(int_pin),
      started_(false),
      filter_(CanFilter::acceptAll()) {}

Mcp2515Bus::~Mcp2515Bus() {
    end();
}

void Mcp2515Bus::select() {
    digitalWrite(cs_pin_, LOW);
}

void Mcp2515Bus::deselect() {
    digitalWrite(cs_pin_, HIGH);
}

uint8_t Mcp2515Bus::transfer(uint8_t value) {
    return SPI.transfer(value);
}

void Mcp2515Bus::reset() {
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_RESET);
    deselect();
    SPI.endTransaction();
    delay(10);
}

uint8_t Mcp2515Bus::readRegister(uint8_t address) {
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_READ);
    transfer(address);
    const uint8_t value = transfer(0x00);
    deselect();
    SPI.endTransaction();
    return value;
}

void Mcp2515Bus::writeRegister(uint8_t address, uint8_t value) {
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_WRITE);
    transfer(address);
    transfer(value);
    deselect();
    SPI.endTransaction();
}

void Mcp2515Bus::modifyRegister(uint8_t address, uint8_t mask, uint8_t value) {
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_BIT_MODIFY);
    transfer(address);
    transfer(mask);
    transfer(value);
    deselect();
    SPI.endTransaction();
}

uint8_t Mcp2515Bus::readStatus() {
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_READ_STATUS);
    const uint8_t status = transfer(0x00);
    deselect();
    SPI.endTransaction();
    return status;
}

bool Mcp2515Bus::setMode(uint8_t mode) {
    modifyRegister(REG_CANCTRL, MODE_MASK, mode);
    for (uint8_t retries = 0; retries < 10; ++retries) {
        if ((readRegister(REG_CANSTAT) & MODE_MASK) == mode) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool Mcp2515Bus::configureBitrate(uint32_t bitrate) {
    BitTiming timing;
    if (!timingFor(bitrate, clock_, timing)) {
        LOG_ERROR("MCP2515: bitrate no soportado");
        return false;
    }
    writeRegister(REG_CNF1, timing.cnf1);
    writeRegister(REG_CNF2, timing.cnf2);
    writeRegister(REG_CNF3, timing.cnf3);
    return true;
}

CanStatus Mcp2515Bus::begin(uint32_t bitrate) {
    pinMode(cs_pin_, OUTPUT);
    deselect();
    if (int_pin_ >= 0) {
        pinMode(static_cast<uint8_t>(int_pin_), INPUT_PULLUP);
    }
    SPI.begin();

    reset();
    if (!setMode(MODE_CONFIG)) {
        LOG_ERROR("MCP2515 no entra en modo config");
        return CAN_ERR_INIT;
    }
    if (!configureBitrate(bitrate)) {
        return CAN_ERR_INIT;
    }

    /* Mascaras en cero: se acepta todo en hardware y se filtra por software. */
    for (uint8_t i = 0; i < 4; ++i) {
        writeRegister(static_cast<uint8_t>(REG_RXM0SIDH + i), 0x00);
        writeRegister(static_cast<uint8_t>(REG_RXM1SIDH + i), 0x00);
    }
    writeRegister(REG_RXB0CTRL, 0x64); /* recibe cualquier trama y habilita rollover a RXB1 */
    writeRegister(REG_RXB1CTRL, 0x60);
    writeRegister(REG_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF);
    writeRegister(REG_CANINTF, 0x00);

    if (!setMode(MODE_NORMAL)) {
        LOG_ERROR("MCP2515 no entra en modo normal");
        return CAN_ERR_INIT;
    }
    started_ = true;
    return CAN_OK;
}

void Mcp2515Bus::end() {
    if (!started_) {
        return;
    }
    setMode(MODE_CONFIG);
    started_ = false;
}

CanStatus Mcp2515Bus::send(const CanFrame &frame, uint32_t timeout_ms) {
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }

    const uint32_t deadline = millis() + timeout_ms;
    while (readRegister(REG_TXB0CTRL) & TXB0CTRL_TXREQ) {
        if (millis() >= deadline) {
            return CAN_ERR_TX_BUSY;
        }
    }

    uint8_t id_bytes[4];
    encodeExtendedId(frame.id & kExtendedIdMask, id_bytes);

    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_WRITE);
    transfer(REG_TXB0SIDH);
    for (uint8_t i = 0; i < 4; ++i) {
        transfer(id_bytes[i]);
    }
    transfer(frame.dlc & 0x0F);
    for (uint8_t i = 0; i < frame.dlc; ++i) {
        transfer(frame.data[i]);
    }
    deselect();

    select();
    transfer(CMD_RTS_TXB0);
    deselect();
    SPI.endTransaction();
    return CAN_OK;
}

bool Mcp2515Bus::readRxBuffer(uint8_t buffer_index, CanFrame &frame) {
    const uint8_t base = (buffer_index == 0) ? REG_RXB0SIDH : REG_RXB1SIDH;

    uint8_t header[5];
    SPI.beginTransaction(kSpiSettings);
    select();
    transfer(CMD_READ);
    transfer(base);
    for (uint8_t i = 0; i < 5; ++i) {
        header[i] = transfer(0x00);
    }
    const uint8_t dlc = header[4] & 0x0F;
    memset(frame.data, 0, sizeof(frame.data));
    for (uint8_t i = 0; i < dlc && i < kPayloadSize; ++i) {
        frame.data[i] = transfer(0x00);
    }
    deselect();
    SPI.endTransaction();

    const bool extended = (header[1] & 0x08) != 0;
    frame.id = decodeExtendedId(header);
    frame.dlc = dlc;
    return extended;
}

CanStatus Mcp2515Bus::receive(CanFrame &frame, uint32_t timeout_ms) {
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }

    const uint32_t deadline = millis() + timeout_ms;
    do {
        const uint8_t flags = readRegister(REG_CANINTF);
        for (uint8_t buffer = 0; buffer < 2; ++buffer) {
            const uint8_t bit = (buffer == 0) ? CANINTF_RX0IF : CANINTF_RX1IF;
            if ((flags & bit) == 0) {
                continue;
            }
            const bool extended = readRxBuffer(buffer, frame);
            modifyRegister(REG_CANINTF, bit, 0x00);
            if (extended && filter_.accepts(frame.id)) {
                return CAN_OK;
            }
        }
    } while (timeout_ms > 0 && millis() < deadline);

    return CAN_ERR_NO_DATA;
}

void Mcp2515Bus::setFilter(const CanFilter &filter) {
    filter_ = filter;
}

bool Mcp2515Bus::isBusOff() const {
    /* readRegister no es const por el uso de SPI; se accede por copia mutable. */
    return (const_cast<Mcp2515Bus *>(this)->readRegister(REG_EFLG) & EFLG_TXBO) != 0;
}

CanStatus Mcp2515Bus::recover() {
    reset();
    return begin(CAN_BUS_BITRATE);
}

}  // namespace pcd

#endif  /* __AVR__ */
