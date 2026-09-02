/*
 * Pruebas unitarias de la capa de servicios (SystemApi), el bucle de eventos
 * (RingBuffer/CanEventLoop) y el motor de tunel CAN-sobre-IP.
 *
 * Estas pruebas corren en el entorno "native" sin ningun hardware: validan el
 * desacoplamiento framework-agnostico y el serializado de tramas a datagramas
 * UDP con checksum.
 */

#include <unity.h>

#include "PCD_CAN.h"

using namespace pcd;

/* ------------------------------------------------------------------ */
/* SystemApi                                                          */
/* ------------------------------------------------------------------ */

namespace {

uint32_t g_mock_millis = 0;
uint32_t g_delay_calls = 0;
int g_log_calls = 0;
int g_lock_calls = 0;
int g_unlock_calls = 0;

uint32_t mockMillis() { return g_mock_millis; }
void mockDelay(uint32_t ms) {
    (void)ms;
    ++g_delay_calls;
}
void mockLog(uint8_t level, const char *format, va_list args) {
    (void)level;
    (void)format;
    (void)args;
    ++g_log_calls;
}
void mockLock() { ++g_lock_calls; }
void mockUnlock() { ++g_unlock_calls; }

struct MockTransport : public ITunnelTransport {
    MockTransport() : sent_count(0), rx_head(0), rx_count(0) {}

    bool send(uint16_t peer, const uint8_t *data, size_t len) override {
        if (sent_count < 16 && len <= kMaxUdpDatagram) {
            sent_peer[sent_count] = peer;
            sent_len[sent_count] = len;
            memcpy(sent_data[sent_count], data, len);
            ++sent_count;
        }
        return true;
    }

    bool receive(uint16_t &peer_out, uint8_t *data, size_t max_len,
                 size_t &len) override {
        if (rx_count == 0) {
            return false;
        }
        const size_t to_copy = (rx_len[rx_head] <= max_len) ? rx_len[rx_head] : max_len;
        memcpy(data, rx_data[rx_head], to_copy);
        len = to_copy;
        peer_out = rx_peer[rx_head];
        rx_head = (rx_head + 1) % 16;
        --rx_count;
        return true;
    }

    bool available() const override { return rx_count > 0; }

    void deliver(uint16_t peer, const uint8_t *data, size_t len) {
        if (rx_count < 16 && len <= kMaxUdpDatagram) {
            rx_peer[(rx_head + rx_count) % 16] = peer;
            rx_len[(rx_head + rx_count) % 16] = len;
            memcpy(rx_data[(rx_head + rx_count) % 16], data, len);
            ++rx_count;
        }
    }

    static const size_t kMaxUdpDatagram = 64;

    uint16_t sent_peer[16];
    size_t sent_len[16];
    uint8_t sent_data[16][kMaxUdpDatagram];
    int sent_count;

    uint16_t rx_peer[16];
    size_t rx_len[16];
    uint8_t rx_data[16][kMaxUdpDatagram];
    size_t rx_head;
    size_t rx_count;
};

}  // namespace

void test_set_system_api(void) {
    setSystemApi(0);
    TEST_ASSERT_NULL(systemApi());

    static SystemApi api;
    api.millis = mockMillis;
    api.delay_ms = mockDelay;
    api.log = mockLog;
    api.lock = mockLock;
    api.unlock = mockUnlock;
    setSystemApi(&api);

    TEST_ASSERT_EQUAL_PTR(&api, systemApi());
    TEST_ASSERT_TRUE(api.hasTime());
    TEST_ASSERT_TRUE(api.hasLog());
    TEST_ASSERT_TRUE(api.hasLock());

    g_mock_millis = 1234;
    TEST_ASSERT_EQUAL_UINT32(1234, systemMillis());

    systemDelay(50);
    TEST_ASSERT_EQUAL_INT(1, g_delay_calls);

    logMessage(LOG_LEVEL_ERROR, "test %d", 42);
    TEST_ASSERT_EQUAL_INT(1, g_log_calls);

    {
        LockGuard lock;
        TEST_ASSERT_EQUAL_INT(1, g_lock_calls);
    }
    TEST_ASSERT_EQUAL_INT(1, g_unlock_calls);

    setSystemApi(0);
}

/* ------------------------------------------------------------------ */
/* RingBuffer                                                         */
/* ------------------------------------------------------------------ */

void test_ring_buffer_push_pop(void) {
    RingBuffer<int, 4> buf;
    TEST_ASSERT_TRUE(buf.empty());
    TEST_ASSERT_FALSE(buf.full());

    TEST_ASSERT_TRUE(buf.push(1));
    TEST_ASSERT_TRUE(buf.push(2));
    TEST_ASSERT_TRUE(buf.push(3));
    TEST_ASSERT_FALSE(buf.full());  /* reserva una casilla */
    TEST_ASSERT_EQUAL_UINT(3, buf.count());

    int value = 0;
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(1, value);
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(2, value);
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(3, value);
    TEST_ASSERT_FALSE(buf.pop(value));
    TEST_ASSERT_TRUE(buf.empty());
}

void test_ring_buffer_full_and_wrap(void) {
    RingBuffer<int, 4> buf;
    TEST_ASSERT_TRUE(buf.push(10));
    TEST_ASSERT_TRUE(buf.push(20));
    TEST_ASSERT_TRUE(buf.push(30));
    TEST_ASSERT_TRUE(buf.full());
    TEST_ASSERT_FALSE(buf.push(40));  /* lleno */

    int value = 0;
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(10, value);
    TEST_ASSERT_TRUE(buf.push(40));
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(20, value);
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(30, value);
    TEST_ASSERT_TRUE(buf.pop(value));
    TEST_ASSERT_EQUAL_INT(40, value);
}

/* ------------------------------------------------------------------ */
/* CanEventLoop                                                       */
/* ------------------------------------------------------------------ */

namespace {

int g_processed = 0;
uint32_t g_last_frame_id = 0;

void processFrame(const CanFrame &frame, void *ctx) {
    ++g_processed;
    g_last_frame_id = frame.id;
    (void)ctx;
}

}  // namespace

void test_event_loop_enqueue_and_tick(void) {
    CanEventLoop loop;
    loop.attach(&processFrame, 0);

    CanFrame frame;
    frame.id = encodeId(PRIO_REALTIME, MSG_STATE, kBroadcastTarget, 0x0016);
    frame.dlc = 8;

    g_processed = 0;
    TEST_ASSERT_TRUE(loop.enqueue(frame));
    TEST_ASSERT_TRUE(loop.enqueue(frame));
    TEST_ASSERT_EQUAL_UINT(2, loop.pending());

    loop.tick();
    TEST_ASSERT_EQUAL_INT(2, g_processed);
    TEST_ASSERT_EQUAL_UINT32(frame.id, g_last_frame_id);
    TEST_ASSERT_TRUE(loop.empty());
}

/* ------------------------------------------------------------------ */
/* TunnelEngine                                                       */
/* ------------------------------------------------------------------ */

void test_tunnel_roundtrip(void) {
    const CanFrame original = makeCommand(0x0005, 0x16, RES_RELAY, 0x01, ACT_ON, 1500);

    uint8_t datagram[kTunnelDatagramBytes];
    TEST_ASSERT_TRUE(encodeTunnelFrame(original, datagram, sizeof(datagram)));

    CanFrame decoded;
    TEST_ASSERT_TRUE(decodeTunnelFrame(datagram, sizeof(datagram), decoded));
    TEST_ASSERT_EQUAL_UINT32(original.id, decoded.id);
    TEST_ASSERT_EQUAL_UINT8(original.dlc, decoded.dlc);
    TEST_ASSERT_EQUAL_UINT8(original.data[0], decoded.data[0]);
    TEST_ASSERT_EQUAL_UINT8(original.data[1], decoded.data[1]);
    TEST_ASSERT_EQUAL_UINT8(original.data[2], decoded.data[2]);
    TEST_ASSERT_EQUAL_UINT32(readUint32BE(&decoded.data[3]), readUint32BE(&original.data[3]));
}

void test_tunnel_rejects_bad_magic(void) {
    CanFrame frame = makeStateBroadcast(0x0016, RES_RELAY, 0x01, 1.0f);
    uint8_t datagram[kTunnelDatagramBytes];
    encodeTunnelFrame(frame, datagram, sizeof(datagram));
    datagram[0] = 0x00;  /* magia incorrecta */

    CanFrame out;
    TEST_ASSERT_FALSE(decodeTunnelFrame(datagram, sizeof(datagram), out));
}

void test_tunnel_rejects_bad_crc(void) {
    CanFrame frame = makeStateBroadcast(0x0016, RES_RELAY, 0x01, 1.0f);
    uint8_t datagram[kTunnelDatagramBytes];
    encodeTunnelFrame(frame, datagram, sizeof(datagram));
    datagram[14] ^= 0xFF;  /* corromper CRC */

    CanFrame out;
    TEST_ASSERT_FALSE(decodeTunnelFrame(datagram, sizeof(datagram), out));
}

void test_tunnel_engine_send_receive(void) {
    MockTransport transport;
    TunnelEngine engine(transport);

    const CanFrame frame = makeInputEvent(0x0016, 0x01, EVT_SHORT_CLICK);
    TEST_ASSERT_TRUE(engine.sendFrame(0x0001, frame));
    TEST_ASSERT_EQUAL_INT(1, transport.sent_count);
    TEST_ASSERT_EQUAL_UINT16(0x0001, transport.sent_peer[0]);
    TEST_ASSERT_EQUAL_UINT(kTunnelDatagramBytes, transport.sent_len[0]);

    /* Simular que el peer devuelve el datagrama. */
    transport.deliver(0x0001, transport.sent_data[0], transport.sent_len[0]);

    uint16_t peer = 0;
    CanFrame received;
    TEST_ASSERT_TRUE(engine.receiveFrame(peer, received));
    TEST_ASSERT_EQUAL_UINT16(0x0001, peer);
    TEST_ASSERT_EQUAL_UINT32(frame.id, received.id);
    TEST_ASSERT_EQUAL_UINT8(frame.data[2], received.data[2]);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_set_system_api);
    RUN_TEST(test_ring_buffer_push_pop);
    RUN_TEST(test_ring_buffer_full_and_wrap);
    RUN_TEST(test_event_loop_enqueue_and_tick);
    RUN_TEST(test_tunnel_roundtrip);
    RUN_TEST(test_tunnel_rejects_bad_magic);
    RUN_TEST(test_tunnel_rejects_bad_crc);
    RUN_TEST(test_tunnel_engine_send_receive);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
