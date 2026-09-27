#include "bridge/bridge_modbus_tcp.h"

#include <string.h>

namespace pcd {

/* ------------------------------------------------------------------ */
/* Helpers big-endian                                                  */
/* ------------------------------------------------------------------ */

static uint16_t be16(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

static void put16(uint8_t *p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFFu);
}

/* ------------------------------------------------------------------ */
/* Codec                                                               */
/* ------------------------------------------------------------------ */

bool modbusTcpHasCompleteAdu(const uint8_t *data, size_t len) {
    if (data == 0 || len < 7) {
        return false;
    }
    /* Los bytes 4..5 (longitud) cuentan UnitId + PDU. */
    const uint16_t length = be16(data + 4);
    if (length < 2) {
        return false; /* longitud invalida: se ignora */
    }
    const size_t total = static_cast<size_t>(6) + length;
    return len >= total;
}

/* Escribe una respuesta de excepcion. Devuelve el largo escrito. */
static size_t buildException(uint8_t *out, const uint8_t *request, uint8_t function,
                             uint8_t code) {
    out[0] = request[0]; /* transaction id hi */
    out[1] = request[1]; /* transaction id lo */
    out[2] = 0;          /* protocol id hi */
    out[3] = 0;          /* protocol id lo */
    out[4] = 0;
    out[5] = 3;          /* longitud: unit id + fc + code */
    out[6] = request[6]; /* unit id */
    out[7] = static_cast<uint8_t>(function | 0x80);
    out[8] = code;
    return 9;
}

/* Preambulo comun de una respuesta normal. Devuelve el offset del PDU. */
static size_t beginResponse(uint8_t *out, const uint8_t *request, uint8_t function,
                            uint8_t pdu_bytes) {
    out[0] = request[0];
    out[1] = request[1];
    out[2] = 0;
    out[3] = 0;
    const uint16_t length = static_cast<uint16_t>(1 + pdu_bytes); /* unit + pdu */
    put16(out + 4, length);
    out[6] = request[6];
    out[7] = function;
    return 8;
}
bool modbusTcpHandleRequest(const uint8_t *request, size_t request_len,
                            IModbusRegisterMap &map, uint8_t *response,
                            size_t &response_len) {
    response_len = 0;
    if (request == 0 || response == 0 || request_len < 8) {
        return false;
    }
    /* Protocolo siempre 0 en Modbus TCP. */
    if (request[2] != 0 || request[3] != 0) {
        return false;
    }
    const uint16_t length = be16(request + 4);
    if (length < 2 || static_cast<size_t>(6) + length > request_len) {
        return false;
    }

    const uint8_t function = request[7];
    const uint8_t *pdu = request + 8;          /* primer byte tras el codigo */
    const size_t pdu_len = static_cast<size_t>(length) - 1; /* sin unit id */

    switch (function) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_READ_DISCRETE_INPUTS: {
            if (pdu_len < 4) return false;
            const uint16_t start = be16(pdu);
            const uint16_t qty = be16(pdu + 2);
            if (qty < 1 || qty > 2000) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_ILLEGAL_DATA_VALUE);
                return true;
            }
            const uint16_t byte_count = static_cast<uint16_t>((qty + 7) / 8);
            const size_t off = beginResponse(response, request, function,
                                             static_cast<uint8_t>(byte_count + 2));
            response[off] = static_cast<uint8_t>(byte_count);
            uint8_t bits = 0;
            uint8_t bit_index = 0;
            size_t out_index = off + 1;
            for (uint16_t i = 0; i < qty; ++i) {
                bool value = false;
                if (!map.readCoil(static_cast<uint16_t>(start + i), value, map.ctx)) {
                    response_len = buildException(response, request, function,
                                                  MODBUS_EX_ILLEGAL_DATA_ADDRESS);
                    return true;
                }
                if (value) {
                    bits |= static_cast<uint8_t>(1u << bit_index);
                }
                ++bit_index;
                if (bit_index == 8) {
                    response[out_index++] = bits;
                    bits = 0;
                    bit_index = 0;
                }
            }
            if (bit_index != 0) {
                response[out_index++] = bits;
            }
            response_len = out_index;
            return true;
        }

        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS: {
            if (pdu_len < 4) return false;
            const uint16_t start = be16(pdu);
            const uint16_t qty = be16(pdu + 2);
            if (qty < 1 || qty > 125) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_ILLEGAL_DATA_VALUE);
                return true;
            }
            const uint16_t byte_count = static_cast<uint16_t>(qty * 2);
            const size_t off = beginResponse(response, request, function,
                                             static_cast<uint8_t>(byte_count + 1));
            response[off] = static_cast<uint8_t>(byte_count);
            size_t out_index = off + 1;
            for (uint16_t i = 0; i < qty; ++i) {
                uint16_t value = 0;
                if (!map.readRegister(static_cast<uint16_t>(start + i), value, map.ctx)) {
                    response_len = buildException(response, request, function,
                                                  MODBUS_EX_ILLEGAL_DATA_ADDRESS);
                    return true;
                }
                put16(response + out_index, value);
                out_index += 2;
            }
            response_len = out_index;
            return true;
        }

        case MODBUS_FC_WRITE_SINGLE_COIL: {
            if (pdu_len < 4) return false;
            const uint16_t address = be16(pdu);
            const uint16_t value = be16(pdu + 2);
            if (value != 0x0000 && value != 0xFF00) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_ILLEGAL_DATA_VALUE);
                return true;
            }
            if (!map.writeCoil(address, value == 0xFF00, map.ctx)) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_DEVICE_FAILURE);
                return true;
            }
            /* Eco exacto del request. */
            response_len = static_cast<size_t>(6) + length;
            memcpy(response, request, response_len);
            return true;
        }

        case MODBUS_FC_WRITE_SINGLE_REGISTER: {
            if (pdu_len < 4) return false;
            const uint16_t address = be16(pdu);
            const uint16_t value = be16(pdu + 2);
            if (!map.writeRegister(address, value, map.ctx)) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_DEVICE_FAILURE);
                return true;
            }
            response_len = static_cast<size_t>(6) + length;
            memcpy(response, request, response_len);
            return true;
        }

        case MODBUS_FC_WRITE_MULTIPLE_COILS: {
            if (pdu_len < 5) return false;
            const uint16_t start = be16(pdu);
            const uint16_t qty = be16(pdu + 2);
            const uint8_t byte_count = pdu[4];
            if (qty < 1 || qty > 1968 ||
                byte_count != static_cast<uint8_t>((qty + 7) / 8) ||
                pdu_len < static_cast<size_t>(5) + byte_count) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_ILLEGAL_DATA_VALUE);
                return true;
            }
            const uint8_t *bits = pdu + 5;
            for (uint16_t i = 0; i < qty; ++i) {
                const bool value = (bits[i / 8] & (1u << (i % 8))) != 0;
                if (!map.writeCoil(static_cast<uint16_t>(start + i), value, map.ctx)) {
                    response_len = buildException(response, request, function,
                                                  MODBUS_EX_DEVICE_FAILURE);
                    return true;
                }
            }
            const size_t off = beginResponse(response, request, function, 4);
            put16(response + off, start);
            put16(response + off + 2, qty);
            response_len = off + 4;
            return true;
        }

        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
            if (pdu_len < 5) return false;
            const uint16_t start = be16(pdu);
            const uint16_t qty = be16(pdu + 2);
            const uint8_t byte_count = pdu[4];
            if (qty < 1 || qty > 123 || byte_count != static_cast<uint8_t>(qty * 2) ||
                pdu_len < static_cast<size_t>(5) + byte_count) {
                response_len = buildException(response, request, function,
                                              MODBUS_EX_ILLEGAL_DATA_VALUE);
                return true;
            }
            const uint8_t *regs = pdu + 5;
            for (uint16_t i = 0; i < qty; ++i) {
                const uint16_t value = be16(regs + i * 2);
                if (!map.writeRegister(static_cast<uint16_t>(start + i), value, map.ctx)) {
                    response_len = buildException(response, request, function,
                                                  MODBUS_EX_DEVICE_FAILURE);
                    return true;
                }
            }
            const size_t off = beginResponse(response, request, function, 4);
            put16(response + off, start);
            put16(response + off + 2, qty);
            response_len = off + 4;
            return true;
        }

        default:
            response_len = buildException(response, request, function,
                                          MODBUS_EX_ILLEGAL_FUNCTION);
            return true;
    }
}

/* ------------------------------------------------------------------ */
/* Servidor                                                            */
/* ------------------------------------------------------------------ */

ModbusTcpServer::ModbusTcpServer(IModbusTcpStream &stream, IModbusRegisterMap &map)
    : stream_(stream), map_(map), unit_id_(kModbusTcpAnyUnit), buffered_(0),
      client_was_connected_(false) {
    resetBuffer();
}

bool ModbusTcpServer::begin() {
    return stream_.begin();
}

void ModbusTcpServer::resetBuffer() {
    memset(buffer_, 0, sizeof(buffer_));
    buffered_ = 0;
}

void ModbusTcpServer::poll() {
    const bool connected = stream_.clientConnected();

    if (!connected) {
        if (client_was_connected_) {
            resetBuffer(); /* nueva conexion limpia en el proximo ciclo */
        }
        client_was_connected_ = false;
        return;
    }

    if (!client_was_connected_) {
        resetBuffer();
    }
    client_was_connected_ = true;

    /* 1. Acumular bytes entrantes. */
    const size_t available = stream_.available();
    if (available > 0 && buffered_ < kModbusTcpBuffer) {
        const size_t space = kModbusTcpBuffer - buffered_;
        const size_t to_read = available < space ? available : space;
        buffered_ += stream_.read(buffer_ + buffered_, to_read);
    }

    /* 2. Procesar ADUs completos. */
    while (buffered_ >= 7) {
        const uint16_t length = be16(buffer_ + 4);
        if (length < 2) {
            resetBuffer(); /* cabecera corrupta */
            break;
        }
        const size_t adu_len = static_cast<size_t>(6) + length;
        if (buffered_ < adu_len) {
            break; /* esperar mas bytes */
        }

        uint8_t response[kModbusTcpBuffer];
        size_t response_len = 0;
        const uint8_t unit = buffer_[6];
        if (unit_id_ == kModbusTcpAnyUnit || unit == unit_id_) {
            modbusTcpHandleRequest(buffer_, adu_len, map_, response, response_len);
            if (response_len > 0) {
                stream_.write(response, response_len);
            }
        }

        /* Consumir el ADU procesado. */
        const size_t remaining = buffered_ - adu_len;
        if (remaining > 0) {
            memmove(buffer_, buffer_ + adu_len, remaining);
        }
        buffered_ = remaining;
    }
}

}  // namespace pcd

