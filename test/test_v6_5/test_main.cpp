/*
 * Pruebas de PCD v6.5: router multi-CAN y gateway/extensions.
 */

#include <unity.h>

#include "v6/pcd_v6.h"

using namespace pcd;

/* ------------------------------------------------------------------ */
/* Mock de interfaz CAN                                                */
/* ------------------------------------------------------------------ */

static uint8_t g_last_payload[8];
static uint8_t g_last_len = 0;
static PCD_CAN_ID g_last_id;
static uint8_t g_send_count = 0;

class MockInterface : public PCD_CanInterface {
  public:
    explicit MockInterface(uint8_t network) : network_(network) {}
    uint8_t networkId() const override { return network_; }
    bool send(const PCD_CAN_ID &id, const uint8_t *payload, uint8_t len) override {
        g_last_id = id;
        g_last_len = len;
        for (uint8_t i = 0; i < len; ++i) {
            g_last_payload[i] = payload[i];
        }
        ++g_send_count;
        return true;
    }

  private:
    uint8_t network_;
};

void test_router_forward(void) {
    PCD_CanRouter router;
    MockInterface seg0(0);
    MockInterface seg1(1);
    router.addInterface(&seg0);
    router.addInterface(&seg1);

    PCD_RoutingRule rule;
    rule.sourceNetwork = 0;
    rule.destinationNetwork = 1;
    rule.messageType = 2;
    rule.action = PCD_ROUTE_FORWARD;
    router.addRule(rule);

    PCD_CAN_ID id(0, 2, 0x10, 0x01, 0);
    uint8_t payload[2] = {0xAA, 0xBB};
    g_send_count = 0;

    TEST_ASSERT_EQUAL_UINT8(1, router.route(id, payload, 2, 0));
    TEST_ASSERT_EQUAL_UINT8(1, g_send_count);
    TEST_ASSERT_EQUAL_UINT8(0, g_last_id.network); /* FORWARD no reescribe la red */
}

void test_router_drop(void) {
    PCD_CanRouter router;
    MockInterface seg0(0);
    MockInterface seg1(1);
    router.addInterface(&seg0);
    router.addInterface(&seg1);

    PCD_RoutingRule rule;
    rule.sourceNetwork = 0;
    rule.messageType = 5;
    rule.action = PCD_ROUTE_DROP;
    router.addRule(rule);

    PCD_CAN_ID id(0, 5, 0x10, 0x01, 0);
    uint8_t payload[1] = {0x01};
    g_send_count = 0;
    TEST_ASSERT_EQUAL_UINT8(0, router.route(id, payload, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(0, g_send_count);
}

void test_router_broadcast(void) {
    PCD_CanRouter router;
    MockInterface seg0(0);
    MockInterface seg1(1);
    MockInterface seg2(2);
    router.addInterface(&seg0);
    router.addInterface(&seg1);
    router.addInterface(&seg2);

    PCD_RoutingRule rule;
    rule.sourceNetwork = 0;
    rule.action = PCD_ROUTE_BROADCAST;
    router.addRule(rule);

    PCD_CAN_ID id(0, 6, 0xFF, 0x01, 0);
    uint8_t payload[1] = {0x07};
    g_send_count = 0;
    TEST_ASSERT_EQUAL_UINT8(2, router.route(id, payload, 1, 0)); /* a seg1 y seg2 */
    TEST_ASSERT_EQUAL_UINT8(2, g_send_count);
}

/* ------------------------------------------------------------------ */
/* Gateway + extension                                                 */
/* ------------------------------------------------------------------ */

static uint8_t g_updates = 0;

class TestExtension : public PCD_Extension {
  public:
    bool begin() override {
        ++g_updates;
        return true;
    }
    void update() override { ++g_updates; }
    const char *name() const override { return "test"; }
    uint16_t version() const override { return 100; }
};

void test_gateway_extension_lifecycle(void) {
    PCD_CanRouter router;
    PCD_Gateway gateway(router);
    TestExtension ext;
    TEST_ASSERT_TRUE(gateway.registerExtension(&ext));
    TEST_ASSERT_EQUAL_UINT8(1, gateway.extensionCount());

    g_updates = 0;
    gateway.begin();
    TEST_ASSERT_EQUAL_UINT8(1, g_updates); /* begin */
    gateway.poll();
    TEST_ASSERT_EQUAL_UINT8(2, g_updates); /* update */
    TEST_ASSERT_EQUAL_STRING("test", ext.name());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_router_forward);
    RUN_TEST(test_router_drop);
    RUN_TEST(test_router_broadcast);
    RUN_TEST(test_gateway_extension_lifecycle);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
