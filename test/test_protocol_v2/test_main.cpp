/*
 * Pruebas del layout B del identificador (PCD v2):
 *
 *   bits 20..13 (8)  Destino   0x00 broadcast, 0x01..0xFF direccion
 *   bits 12..5  (8)  Origen    0x01..0xFF direccion dentro del segmento
 *   bits 4..0   (5)  Segmento  0..31
 *
 * Este archivo solo se compila en el entorno `native_v2`
 * (-DPCD_ID_LAYOUT_V2=1); ver platformio.ini.
 */

#if !PCD_ID_LAYOUT_V2
#error "test_protocol_v2 requiere -DPCD_ID_LAYOUT_V2=1"
#endif

#include <unity.h>

#include "can_node.h"
#include "can_protocol.h"
#include "hal/hal_can_native.h"

using namespace pcd;

void test_layout_v2_field_widths(void) {
    TEST_ASSERT_EQUAL_UINT8(13, kTargetShift);
    TEST_ASSERT_EQUAL_UINT8(5, kSourceShift);
    TEST_ASSERT_EQUAL_UINT8(0, kSegmentShift);
    TEST_ASSERT_EQUAL_UINT32(0xFF, kTargetMask);
    TEST_ASSERT_EQUAL_UINT32(0xFF, kSourceMask);
    TEST_ASSERT_EQUAL_UINT32(0x1F, kSegmentMask);
    TEST_ASSERT_EQUAL_UINT8(0x1F, kMaxSegment);
    TEST_ASSERT_EQUAL_UINT16(0x1FFF, kMaxSource);
}

void test_layout_v2_node_id_composition(void) {
    TEST_ASSERT_EQUAL_UINT16(0x0316, makeNodeId(3, 0x16));
    TEST_ASSERT_EQUAL_UINT8(3, nodeIdSegment(0x0316));
    TEST_ASSERT_EQUAL_UINT8(0x16, nodeIdAddress(0x0316));
    TEST_ASSERT_TRUE(isValidSource(0x0316));
    TEST_ASSERT_FALSE(isValidSource(0x0300)); /* direccion 0 reservada */
    TEST_ASSERT_FALSE(isValidSource(0));
    TEST_ASSERT_TRUE(isValidTarget(0xFF));
}

void test_layout_v2_id_roundtrip_with_segment(void) {
    const uint16_t node_id = makeNodeId(5, 0x2A);
    const uint32_t raw = encodeId(PRIO_REALTIME, MSG_EVENT, 0x16, node_id);

    /* composicion esperada: prioridad 2, tipo 0x02, destino 0x16, origen 0x2A, segmento 5 */
    const uint32_t expected = (2UL << 26) | (0x02UL << 21) | (0x16UL << 13) | (0x2AUL << 5) | 5UL;
    TEST_ASSERT_EQUAL_HEX32(expected, raw);
    TEST_ASSERT_EQUAL_HEX32(0, raw & ~kExtendedIdMask);

    const CanId id = decodeId(raw);
    TEST_ASSERT_EQUAL_UINT8(PRIO_REALTIME, id.priority);
    TEST_ASSERT_EQUAL_UINT8(MSG_EVENT, id.msg_type);
    TEST_ASSERT_EQUAL_UINT8(0x16, id.target);
    TEST_ASSERT_EQUAL_UINT8(0x2A, id.address());
    TEST_ASSERT_EQUAL_UINT8(5, id.segment);
    TEST_ASSERT_EQUAL_UINT16(node_id, id.source);
    TEST_ASSERT_EQUAL_UINT16(node_id, id.nodeId());
}

void test_layout_v2_broadcast_keeps_segment_of_source(void) {
    const CanFrame frame = makeHeartbeat(makeNodeId(7, 0x11), 120, HEALTH_OK);
    const CanId id = frame.fields();
    TEST_ASSERT_EQUAL_UINT8(kBroadcastTarget, id.target);
    TEST_ASSERT_EQUAL_UINT8(7, id.segment);
    TEST_ASSERT_EQUAL_UINT16(0x0711, id.source);

    uint32_t uptime = 0;
    uint8_t health = 0xFF;
    TEST_ASSERT_TRUE(decodeHeartbeat(frame, uptime, health));
    TEST_ASSERT_EQUAL_UINT32(120, uptime);
}

void test_layout_v2_nodes_in_different_segments_do_not_collide(void) {
    VirtualBus backbone;
    NativeCanBus bus(&backbone);

    CanNode node_a(bus, makeNodeId(1, 0x16));
    CanNode node_b(bus, makeNodeId(2, 0x16));
    TEST_ASSERT_EQUAL_UINT8(0x16, node_a.address());
    TEST_ASSERT_EQUAL_UINT8(0x16, node_b.address());
    TEST_ASSERT_EQUAL_UINT16(0x0116, node_a.nodeId());
    TEST_ASSERT_EQUAL_UINT16(0x0216, node_b.nodeId());

    /* La direccion de destino es unica en el bus fisico: el segmento distingue
     * el Node-ID, y el hub es quien entrega la trama al ramal correcto. */
    const CanFilter filter = CanFilter::forTarget(0x16);
    TEST_ASSERT_TRUE(filter.accepts(encodeId(PRIO_REALTIME, MSG_EVENT, 0x16, 0x0216)));
    TEST_ASSERT_FALSE(filter.accepts(encodeId(PRIO_REALTIME, MSG_EVENT, 0x17, 0x0216)));
}

void test_layout_v2_automatic_id_is_valid_and_stable(void) {
    const uint16_t first = deriveAutomaticNodeId(0xDEADBEEFUL);
    const uint16_t second = deriveAutomaticNodeId(0xDEADBEEFUL);
    TEST_ASSERT_EQUAL_UINT16(first, second);
    TEST_ASSERT_TRUE(isValidSource(first));
    TEST_ASSERT_NOT_EQUAL(0, nodeIdAddress(first));
    TEST_ASSERT_TRUE(nodeIdSegment(first) <= kMaxSegment);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_v2_field_widths);
    RUN_TEST(test_layout_v2_node_id_composition);
    RUN_TEST(test_layout_v2_id_roundtrip_with_segment);
    RUN_TEST(test_layout_v2_broadcast_keeps_segment_of_source);
    RUN_TEST(test_layout_v2_nodes_in_different_segments_do_not_collide);
    RUN_TEST(test_layout_v2_automatic_id_is_valid_and_stable);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
