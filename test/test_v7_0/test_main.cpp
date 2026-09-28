/*
 * Pruebas de PCD v7.0: analizador de trafico, simulador de red y migracion.
 */

#include <unity.h>

#include "v6/pcd_v6.h"

using namespace pcd;

void test_analyzer_counts(void) {
    PCD_TrafficAnalyzer analyzer;
    PCD_CAN_ID id(2, 6, 0xFF, 0x0A, 0);
    analyzer.record(id, 8, 0);
    analyzer.record(id, 8, 100);
    PCD_CAN_ID id2(2, 6, 0xFF, 0x0B, 0);
    analyzer.record(id2, 4, 200);

    TEST_ASSERT_EQUAL_UINT32(3, analyzer.totalFrames());
    TEST_ASSERT_EQUAL_UINT32(20, analyzer.totalBytes());
    TEST_ASSERT_EQUAL_UINT8(2, analyzer.activeNodeCount());
    TEST_ASSERT_TRUE(analyzer.hasSeenNode(0x0A));
    TEST_ASSERT_FALSE(analyzer.hasSeenNode(0x0C));
}

void test_analyzer_window(void) {
    PCD_TrafficAnalyzer analyzer;
    PCD_CAN_ID id(2, 6, 0xFF, 0x0A, 0);
    analyzer.record(id, 8, 0);
    analyzer.record(id, 8, 500);
    TEST_ASSERT_EQUAL_UINT32(2, analyzer.framesPerSec());

    /* La ventana rueda al pasar un segundo. */
    analyzer.record(id, 8, 1500);
    TEST_ASSERT_EQUAL_UINT32(1, analyzer.framesPerSec());
}

void test_analyzer_errors_and_utilization(void) {
    PCD_TrafficAnalyzer analyzer;
    analyzer.recordError(0);
    TEST_ASSERT_EQUAL_UINT32(1, analyzer.totalErrors());

    for (uint8_t i = 0; i < 100; ++i) {
        PCD_CAN_ID id(2, 6, 0xFF, 0x01, 0);
        analyzer.record(id, 8, 0);
    }
    TEST_ASSERT_TRUE(analyzer.utilizationPercent(500000) > 0);
}

void test_simulator_nodes(void) {
    PCD_NetworkSimulator sim;
    PCD_MockCAN *a = sim.addNode();
    PCD_MockCAN *b = sim.addNode();
    PCD_MockCAN *c = sim.addNode();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_UINT8(3, sim.nodeCount());

    sim.step(0);

    CanFrame frame;
    frame.id = 0x00000001UL;
    frame.dlc = 2;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a->send(frame));
    sim.step(0);

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b->receive(rx));
    TEST_ASSERT_EQUAL_UINT32(frame.id, rx.id);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, c->receive(rx));
}

void test_migration(void) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PCD_ResourceType::RELAY),
                            static_cast<uint8_t>(pcdMigrateLegacyResource(0x10)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PCD_ResourceType::DALI),
                            static_cast<uint8_t>(pcdMigrateLegacyResource(0x86)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PCD_ResourceType::KNX),
                            static_cast<uint8_t>(pcdMigrateLegacyResource(0x87)));
    TEST_ASSERT_TRUE(pcdHasLegacyMigration(0x10));
    TEST_ASSERT_FALSE(pcdHasLegacyMigration(0x99));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_analyzer_counts);
    RUN_TEST(test_analyzer_window);
    RUN_TEST(test_analyzer_errors_and_utilization);
    RUN_TEST(test_simulator_nodes);
    RUN_TEST(test_migration);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
