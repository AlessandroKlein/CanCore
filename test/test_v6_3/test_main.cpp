/*
 * Pruebas de PCD v6.3: eventos, topologia, store-and-forward y estadisticas.
 */

#include <unity.h>

#include "v6/pcd_v6.h"

using namespace pcd;

void test_event_store_ring(void) {
    PCD_EventStore store;
    for (uint16_t i = 0; i < PCD_EventStore::kCapacity + 5; ++i) {
        PCD_Event e;
        e.type = PCD_EVENT_STATE;
        e.nodeId = static_cast<uint16_t>(i);
        store.push(e);
    }
    TEST_ASSERT_EQUAL_UINT16(PCD_EventStore::kCapacity, store.count());
    /* El evento 0 fue sobrescrito; el mas antiguo es ahora el 5. */
    TEST_ASSERT_EQUAL_UINT16(5, store.at(0).nodeId);
}

void test_topology_online_offline(void) {
    PCD_TopologyDiscovery topo;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, topo.update(0x0001, 0, 1000));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, topo.update(0x0002, 0, 1000));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, topo.update(0x0310, 3, 1000));

    TEST_ASSERT_EQUAL_UINT8(3, topo.nodeCount());
    TEST_ASSERT_EQUAL_UINT8(3, topo.onlineCount());
    TEST_ASSERT_EQUAL_UINT8(2, topo.networkCount());

    /* Tras el timeout, los nodos sin refresco caen offline. */
    TEST_ASSERT_EQUAL_UINT8(3, topo.poll(1000 + 5000, 5000));
    TEST_ASSERT_EQUAL_UINT8(0, topo.onlineCount());
    TEST_ASSERT_EQUAL_UINT8(3, topo.offlineCount());
    TEST_ASSERT_EQUAL_UINT8(PCD_NODE_OFFLINE, topo.state(0x0001));
}

void test_store_and_forward(void) {
    PCD_StoreAndForward saf;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, saf.store(0x0C, 4, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, saf.store(0x0C, 5, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, saf.store(0x0D, 1, 1, 0));

    TEST_ASSERT_EQUAL_UINT8(2, saf.pendingFor(0x0C));
    TEST_ASSERT_EQUAL_UINT8(1, saf.pendingFor(0x0D));

    PCD_PendingMessage msg;
    TEST_ASSERT_TRUE(saf.popFor(0x0C, msg));
    TEST_ASSERT_EQUAL_UINT8(4, msg.resourceId);
    TEST_ASSERT_EQUAL_UINT8(1, saf.pendingFor(0x0C));
}

void test_bus_utilization(void) {
    PCD_BusStatistics stats;
    stats.framesPerSec = 1000;
    TEST_ASSERT_TRUE(stats.utilizationPercent(500000) > 0);
    TEST_ASSERT_EQUAL_UINT8(0, PCD_BusStatistics().utilizationPercent(500000));
}

void test_rate_limiter(void) {
    PCD_RateLimiter limiter;
    limiter.configure(10, 10); /* 10 fps, burst 10 */
    limiter.reset(0);

    for (uint16_t i = 0; i < 10; ++i) {
        TEST_ASSERT_TRUE(limiter.allow(0));
    }
    TEST_ASSERT_FALSE(limiter.allow(0)); /* sin tokens */

    limiter.reset(1000);
    TEST_ASSERT_TRUE(limiter.allow(1000));
}

void test_network_health(void) {
    TEST_ASSERT_EQUAL_UINT8(PCD_HEALTH_HEALTHY, pcdComputeNetworkHealth(10, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_HEALTH_DEGRADED, pcdComputeNetworkHealth(60, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_HEALTH_CRITICAL, pcdComputeNetworkHealth(90, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_HEALTH_CRITICAL, pcdComputeNetworkHealth(10, 20, 0));
    TEST_ASSERT_EQUAL_STRING("CRITICAL", pcdNetworkHealthName(PCD_HEALTH_CRITICAL));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_event_store_ring);
    RUN_TEST(test_topology_online_offline);
    RUN_TEST(test_store_and_forward);
    RUN_TEST(test_bus_utilization);
    RUN_TEST(test_rate_limiter);
    RUN_TEST(test_network_health);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
