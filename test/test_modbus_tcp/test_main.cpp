/*
 * Pruebas del servidor Modbus TCP (slave) y del codec ADU.
 *
 * Cubre:
 *   - deteccion de ADU completo (stream parcial vs. completo),
 *   - lecturas de coils y holding/input registers,
 *   - escrituras simples y multiples (coils y registers),
 *   - respuestas de excepcion (funcion, direccion y valor invalidos),
 *   - servidor `poll()` con un stream simulado (acumulacion + unit id).
 */

#include <unity.h>

#include <string.h>

#include "PCD_CAN.h"

using namespace pcd;

namespace {

/* ---- Mapa de registros simulado ----------------------------------- */

struct MockMap {
    static const uint16_t kCoils = 32;
    static const uint16_t kRegs = 32;
    bool coils[kCoils];
    uint16_t regs[kRegs];
    bool fail_writes;

    MockMap() : fail_writes(false) {
        memset(coils, 0, sizeof(coils));
        memset(regs, 0, sizeof(regs));
    }

    static bool readCoil(uint16_t addr, bool &value, void *ctx) {
        MockMap *self = static_cast<MockMap *>(ctx);
        if (addr >= kCoils) return false;
        value = self->coils[addr];
        return true;
    }
    static bool readRegister(uint16_t addr, uint16_t &value, void *ctx) {
        MockMap *self = static_cast<MockMap *>(ctx);
        if (addr >= kRegs) return false;
        value = self->regs[addr];
        return true;
    }
    static bool writeCoil(uint16_t addr, bool value, void *ctx) {
        MockMap *self = static_cast<MockMap *>(ctx);
        if (addr >= kCoils) return false;
        if (self->fail_writes) return false;
        self->coils[addr] = value;
        return true;
    }
    static bool writeRegister(uint16_t addr, uint16_t value, void *ctx) {
        MockMap *self = static_cast<MockMap *>(ctx);
        if (addr >= kRegs) return false;
        if (self->fail_writes) return false;
        self->regs[addr] = value;
        return true;
    }

    IModbusRegisterMap bind() {
        IModbusRegisterMap m;
        m.readRegister = &MockMap::readRegister;
        m.readCoil = &MockMap::readCoil;
        m.writeRegister = &MockMap::writeRegister;
        m.writeCoil = &MockMap::writeCoil;
        m.ctx = this;
        return m;
    }
};

/* ---- Stream TCP simulado ------------------------------------------ */

struct MockStream : public IModbusTcpStream {
    uint8_t rx[256];
    size_t rx_len;
    uint8_t tx[256];
    size_t tx_len;
    bool connected;

    MockStream() : rx_len(0), tx_len(0), connected(false) {}

    bool begin() override {
        connected = true;
        return true;
    }
    bool clientConnected() override { return connected; }
    size_t available() override { return rx_len; }
    size_t read(uint8_t *data, size_t max_len) override {
        const size_t n = rx_len < max_len ? rx_len : max_len;
        memcpy(data, rx, n);
        rx_len = 0;
        return n;
    }
    size_t write(const uint8_t *data, size_t len) override {
        const size_t n = len < (sizeof(tx) - tx_len) ? len : (sizeof(tx) - tx_len);
        memcpy(tx + tx_len, data, n);
        tx_len += n;
        return n;
    }
    void stopClient() override { connected = false; }

    void enqueue(const uint8_t *data, size_t len) {
        memcpy(rx, data, len);
        rx_len = len;
    }
};

/* ---- Helpers ------------------------------------------------------- */

uint16_t be16(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void put16(uint8_t *p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xFFu);
}

/* Arma un ADU Modbus TCP: MBAP + fc + pdu. Devuelve el largo total. */
size_t buildRequest(uint8_t *buf, uint16_t txn, uint8_t unit, uint8_t fc,
                    const uint8_t *pdu, size_t pdu_len) {
    put16(buf, txn);
    buf[2] = 0;
    buf[3] = 0;
    put16(buf + 4, static_cast<uint16_t>(1 + 1 + pdu_len)); /* unit + fc + pdu */
    buf[6] = unit;
    buf[7] = fc;
    memcpy(buf + 8, pdu, pdu_len);
    return 8 + pdu_len;
}

}  // namespace
void test_has_complete_adu(void) {
    /* Request real de Read Coils: 12 bytes, campo longitud = 6. */
    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 1);
    uint8_t buf[16];
    const size_t len = buildRequest(buf, 0x0001, 0x01, MODBUS_FC_READ_COILS, pdu, 4);
    TEST_ASSERT_EQUAL_UINT(12, len);
    TEST_ASSERT_FALSE(modbusTcpHasCompleteAdu(buf, 6));  /* sin header completo */
    TEST_ASSERT_FALSE(modbusTcpHasCompleteAdu(buf, 11)); /* falta 1 byte del PDU */
    TEST_ASSERT_TRUE(modbusTcpHasCompleteAdu(buf, 12));  /* completo */
    TEST_ASSERT_TRUE(modbusTcpHasCompleteAdu(buf, 16));  /* con bytes de mas */
    TEST_ASSERT_FALSE(modbusTcpHasCompleteAdu(0, 0));

    /* Longitud invalida (0) => nunca se considera completo. */
    uint8_t bad[8];
    memset(bad, 0, sizeof(bad));
    TEST_ASSERT_FALSE(modbusTcpHasCompleteAdu(bad, 8));
}

void test_read_coils(void) {
    MockMap map;
    map.coils[0] = true;
    map.coils[1] = false;
    map.coils[2] = true;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 0);     /* inicio */
    put16(pdu + 2, 3); /* 3 coils */

    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0042, 0x01, MODBUS_FC_READ_COILS, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT16(0x0042, be16(response));
    TEST_ASSERT_EQUAL_UINT8(0x01, response[6]);
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_READ_COILS, response[7]);
    TEST_ASSERT_EQUAL_UINT8(1, response[8]); /* byte count */
    TEST_ASSERT_EQUAL_UINT8(0x05, response[9]); /* 00000101 -> coils 0 y 2 */
    TEST_ASSERT_EQUAL_UINT(10, resp_len);
}

void test_read_holding_registers(void) {
    MockMap map;
    map.regs[4] = 0x1234;
    map.regs[5] = 0xABCD;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 4);     /* inicio */
    put16(pdu + 2, 2); /* 2 registros */

    uint8_t request[16];
    const size_t req_len =
        buildRequest(request, 0x0007, 0xFF, MODBUS_FC_READ_HOLDING_REGISTERS, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(0xFF, response[6]);
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_READ_HOLDING_REGISTERS, response[7]);
    TEST_ASSERT_EQUAL_UINT8(4, response[8]); /* byte count */
    TEST_ASSERT_EQUAL_UINT16(0x1234, be16(response + 9));
    TEST_ASSERT_EQUAL_UINT16(0xABCD, be16(response + 11));
    TEST_ASSERT_EQUAL_UINT(13, resp_len);
}

void test_read_input_registers(void) {
    MockMap map;
    map.regs[0] = 42;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 1);

    uint8_t request[16];
    const size_t req_len =
        buildRequest(request, 0x0001, 0x01, MODBUS_FC_READ_INPUT_REGISTERS, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_READ_INPUT_REGISTERS, response[7]);
    TEST_ASSERT_EQUAL_UINT16(42, be16(response + 9));
    TEST_ASSERT_EQUAL_UINT(11, resp_len);
}

void test_write_single_coil(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 7);
    put16(pdu + 2, 0xFF00); /* ON */

    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0002, 0x01, MODBUS_FC_WRITE_SINGLE_COIL, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_TRUE(map.coils[7]);
    TEST_ASSERT_EQUAL_UINT(req_len, resp_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(request, response, req_len); /* eco */
}

void test_write_single_register(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 3);
    put16(pdu + 2, 0x0BB8);

    uint8_t request[16];
    const size_t req_len =
        buildRequest(request, 0x0003, 0x01, MODBUS_FC_WRITE_SINGLE_REGISTER, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT16(0x0BB8, map.regs[3]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(request, response, req_len);
}

void test_write_multiple_coils(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[7];
    put16(pdu, 10);
    put16(pdu + 2, 5);
    pdu[4] = 1; /* byte count */
    pdu[5] = 0b00010101; /* coils 10..14 */

    uint8_t request[16];
    const size_t req_len =
        buildRequest(request, 0x0004, 0x01, MODBUS_FC_WRITE_MULTIPLE_COILS, pdu, 6);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_TRUE(map.coils[10]);
    TEST_ASSERT_FALSE(map.coils[11]);
    TEST_ASSERT_TRUE(map.coils[12]);
    TEST_ASSERT_FALSE(map.coils[13]);
    TEST_ASSERT_TRUE(map.coils[14]);
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_WRITE_MULTIPLE_COILS, response[7]);
    TEST_ASSERT_EQUAL_UINT16(10, be16(response + 8));
    TEST_ASSERT_EQUAL_UINT16(5, be16(response + 10));
    TEST_ASSERT_EQUAL_UINT(12, resp_len);
}

void test_write_multiple_registers(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[9];
    put16(pdu, 0);
    put16(pdu + 2, 2);
    pdu[4] = 4; /* byte count */
    put16(pdu + 5, 0x1111);
    put16(pdu + 7, 0x2222);

    uint8_t request[16];
    const size_t req_len =
        buildRequest(request, 0x0005, 0x01, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, pdu, 9);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT16(0x1111, map.regs[0]);
    TEST_ASSERT_EQUAL_UINT16(0x2222, map.regs[1]);
    TEST_ASSERT_EQUAL_UINT16(2, be16(response + 10));
    TEST_ASSERT_EQUAL_UINT(12, resp_len);
}

void test_exception_illegal_function(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t request[12];
    const size_t req_len = buildRequest(request, 0x0001, 0x01, 0x2B, 0, 0);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(0x2B | 0x80, response[7]);
    TEST_ASSERT_EQUAL_UINT8(MODBUS_EX_ILLEGAL_FUNCTION, response[8]);
    TEST_ASSERT_EQUAL_UINT(9, resp_len);
}

void test_exception_illegal_data_value(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 0); /* cantidad 0: invalida */

    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0001, 0x01, MODBUS_FC_READ_COILS, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_READ_COILS | 0x80, response[7]);
    TEST_ASSERT_EQUAL_UINT8(MODBUS_EX_ILLEGAL_DATA_VALUE, response[8]);
}

void test_exception_illegal_data_address(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 1000); /* fuera del mapa */
    put16(pdu + 2, 1);

    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0001, 0x01, MODBUS_FC_READ_COILS, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(MODBUS_EX_ILLEGAL_DATA_ADDRESS, response[8]);
}

void test_exception_device_failure_on_write(void) {
    MockMap map;
    map.fail_writes = true;
    IModbusRegisterMap m = map.bind();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 0xFF00);

    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0001, 0x01, MODBUS_FC_WRITE_SINGLE_COIL, pdu, 4);
    uint8_t response[16];
    size_t resp_len = 0;

    TEST_ASSERT_TRUE(modbusTcpHandleRequest(request, req_len, m, response, resp_len));
    TEST_ASSERT_EQUAL_UINT8(MODBUS_EX_DEVICE_FAILURE, response[8]);
}

void test_server_poll_processes_request(void) {
    MockMap map;
    map.coils[1] = true;
    IModbusRegisterMap m = map.bind();
    MockStream stream;
    ModbusTcpServer server(stream, m);
    stream.begin();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 4);
    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x0009, 0x01, MODBUS_FC_READ_COILS, pdu, 4);

    stream.enqueue(request, req_len);
    server.poll();

    /* Respuesta Read Coils: 7 (MBAP) + 1 (fc) + 1 (byte count) + 1 (data) = 10. */
    TEST_ASSERT_EQUAL_UINT(10, stream.tx_len);
    TEST_ASSERT_EQUAL_UINT16(0x0009, be16(stream.tx));
    TEST_ASSERT_EQUAL_UINT8(MODBUS_FC_READ_COILS, stream.tx[7]);
    TEST_ASSERT_EQUAL_UINT8(0x02, stream.tx[9]); /* coil 1 encendido */
}

void test_server_poll_waits_for_complete_request(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();
    MockStream stream;
    ModbusTcpServer server(stream, m);
    stream.begin();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 2);
    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x000A, 0x01, MODBUS_FC_READ_COILS, pdu, 4);

    /* Enviamos solo la mitad del ADU: no debe responder todavia. */
    stream.enqueue(request, req_len / 2);
    server.poll();
    TEST_ASSERT_EQUAL_UINT(0, stream.tx_len);

    /* Completamos el ADU: ahora si responde. */
    stream.enqueue(request + req_len / 2, req_len - req_len / 2);
    server.poll();
    TEST_ASSERT_TRUE(stream.tx_len > 0);
    TEST_ASSERT_EQUAL_UINT16(0x000A, be16(stream.tx));
}

void test_server_poll_filters_unit_id(void) {
    MockMap map;
    IModbusRegisterMap m = map.bind();
    MockStream stream;
    ModbusTcpServer server(stream, m);
    server.setUnitId(0x02); /* solo acepta slave 2 */
    stream.begin();

    uint8_t pdu[4];
    put16(pdu, 0);
    put16(pdu + 2, 1);
    uint8_t request[16];
    const size_t req_len = buildRequest(request, 0x000B, 0x01, MODBUS_FC_READ_COILS, pdu, 4);

    stream.enqueue(request, req_len);
    server.poll();
    TEST_ASSERT_EQUAL_UINT(0, stream.tx_len); /* unit 1 ignorado */

    stream.tx_len = 0;
    const size_t req_len2 = buildRequest(request, 0x000C, 0x02, MODBUS_FC_READ_COILS, pdu, 4);
    stream.enqueue(request, req_len2);
    server.poll();
    TEST_ASSERT_TRUE(stream.tx_len > 0); /* unit 2 atendido */
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_has_complete_adu);
    RUN_TEST(test_read_coils);
    RUN_TEST(test_read_holding_registers);
    RUN_TEST(test_read_input_registers);
    RUN_TEST(test_write_single_coil);
    RUN_TEST(test_write_single_register);
    RUN_TEST(test_write_multiple_coils);
    RUN_TEST(test_write_multiple_registers);
    RUN_TEST(test_exception_illegal_function);
    RUN_TEST(test_exception_illegal_data_value);
    RUN_TEST(test_exception_illegal_data_address);
    RUN_TEST(test_exception_device_failure_on_write);
    RUN_TEST(test_server_poll_processes_request);
    RUN_TEST(test_server_poll_waits_for_complete_request);
    RUN_TEST(test_server_poll_filters_unit_id);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}

