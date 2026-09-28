/*
 * Pruebas de la capa PCD v6 (aditiva, sin hardware).
 *
 * Cubre: CRC8/16/32, codec de tipos Little Endian, CAN ID v6, tipos y
 * descriptores de recurso, diagnostico, manifiesto de firmware y PCD_MockCAN
 * con inyeccion de fallas.
 */

#include <unity.h>

#include <string.h>

#include "v6/pcd_v6.h"

using namespace pcd;

/* ------------------------------------------------------------------ */
/* CRC                                                                 */
/* ------------------------------------------------------------------ */

static const uint8_t kCrcVector[] = "123456789"; /* 9 bytes ASCII */

void test_crc8_vector(void) {
    TEST_ASSERT_EQUAL_UINT8(0xF4, pcd_crc8(kCrcVector, sizeof(kCrcVector) - 1));
}

void test_crc16_vector(void) {
    TEST_ASSERT_EQUAL_UINT16(0x29B1, pcd_crc16(kCrcVector, sizeof(kCrcVector) - 1));
}

void test_crc32_vector(void) {
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926UL, pcd_crc32(kCrcVector, sizeof(kCrcVector) - 1));
}

void test_crc_changes_with_data(void) {
    const uint8_t a[1] = {0x00};
    const uint8_t b[1] = {0x01};
    TEST_ASSERT_TRUE(pcd_crc16(a, 1) != pcd_crc16(b, 1));
}

/* ------------------------------------------------------------------ */
/* Tipos de datos y codec LE                                           */
/* ------------------------------------------------------------------ */

void test_datatype_sizes(void) {
    TEST_ASSERT_EQUAL_UINT(1, pcdTypeSize(PCD_TYPE_BOOL));
    TEST_ASSERT_EQUAL_UINT(1, pcdTypeSize(PCD_TYPE_UINT8));
    TEST_ASSERT_EQUAL_UINT(2, pcdTypeSize(PCD_TYPE_UINT16));
    TEST_ASSERT_EQUAL_UINT(4, pcdTypeSize(PCD_TYPE_UINT32));
    TEST_ASSERT_EQUAL_UINT(4, pcdTypeSize(PCD_TYPE_FLOAT32));
    TEST_ASSERT_EQUAL_UINT(8, pcdTypeSize(PCD_TYPE_UINT64));
    TEST_ASSERT_EQUAL_UINT(8, pcdTypeSize(PCD_TYPE_FLOAT64));
    TEST_ASSERT_EQUAL_UINT(0, pcdTypeSize(PCD_TYPE_STRING));
    TEST_ASSERT_EQUAL_UINT(0, pcdTypeSize(PCD_TYPE_BYTES));
    TEST_ASSERT_TRUE(pcdTypeIsIntegral(PCD_TYPE_INT32));
    TEST_ASSERT_TRUE(pcdTypeIsFloat(PCD_TYPE_FLOAT64));
    TEST_ASSERT_FALSE(pcdTypeIsFloat(PCD_TYPE_UINT16));
}

void test_le_uint16_roundtrip(void) {
    uint8_t buf[2];
    pcdWriteUint16LE(buf, 0x1234);
    TEST_ASSERT_EQUAL_UINT8(0x34, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x12, buf[1]);
    TEST_ASSERT_EQUAL_UINT16(0x1234, pcdReadUint16LE(buf));
}

void test_le_uint32_roundtrip(void) {
    uint8_t buf[4];
    pcdWriteUint32LE(buf, 0xDEADBEEFUL);
    TEST_ASSERT_EQUAL_UINT8(0xEF, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, buf[1]);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFUL, pcdReadUint32LE(buf));
}

void test_le_float32_roundtrip(void) {
    uint8_t buf[4];
    const float value = 24.5f;
    pcdWriteFloat32LE(buf, value);
    TEST_ASSERT_EQUAL_FLOAT(value, pcdReadFloat32LE(buf));
}

void test_write_read_value(void) {
    uint8_t buf[8];
    uint32_t u32 = 0x01020304UL;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, pcdWriteValue(buf, sizeof(buf), PCD_TYPE_UINT32, &u32));
    TEST_ASSERT_EQUAL_UINT8(0x04, buf[0]);
    uint32_t out = 0;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, pcdReadValue(buf, 4, PCD_TYPE_UINT32, &out));
    TEST_ASSERT_EQUAL_UINT32(u32, out);

    float f = -3.5f;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, pcdWriteValue(buf, sizeof(buf), PCD_TYPE_FLOAT32, &f));
    float fout = 0;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, pcdReadValue(buf, 4, PCD_TYPE_FLOAT32, &fout));
    TEST_ASSERT_EQUAL_FLOAT(f, fout);

    /* Tipo variable -> error. */
    TEST_ASSERT_EQUAL_UINT8(PCD_ERR_INVALID_ARGUMENT,
                            pcdWriteValue(buf, sizeof(buf), PCD_TYPE_STRING, &u32));
}

/* ------------------------------------------------------------------ */
/* CAN ID v6                                                           */
/* ------------------------------------------------------------------ */

void test_can_id_roundtrip(void) {
    PCD_CAN_ID id(3, 17, 0x42, 0x16, 5);
    const uint32_t raw = id.encode();
    PCD_CAN_ID dec = PCD_CAN_ID::decode(raw);

    TEST_ASSERT_EQUAL_UINT8(3, dec.priority);
    TEST_ASSERT_EQUAL_UINT8(17, dec.messageType);
    TEST_ASSERT_EQUAL_UINT8(0x42, dec.destination);
    TEST_ASSERT_EQUAL_UINT8(0x16, dec.source);
    TEST_ASSERT_EQUAL_UINT8(5, dec.network);
    TEST_ASSERT_TRUE(raw <= 0x1FFFFFFFUL);
}

void test_can_id_addressing(void) {
    TEST_ASSERT_TRUE(PCD_CAN_ID(0, 0, PCD_NODE_BROADCAST, 1, 0).isBroadcast());
    TEST_ASSERT_FALSE(PCD_CAN_ID(0, 0, 0x42, 1, 0).isBroadcast());
    TEST_ASSERT_TRUE(PCD_CAN_ID(0, 0, PCD_MULTICAST_BASE, 1, 0).isMulticast());
    TEST_ASSERT_FALSE(PCD_CAN_ID(0, 0, 0x10, 1, 0).isMulticast());

    const PCD_CAN_ID unicast(0, 0, 0x42, 1, 0);
    TEST_ASSERT_TRUE(unicast.isForNode(0x42));
    TEST_ASSERT_FALSE(unicast.isForNode(0x43));
    const PCD_CAN_ID broadcast(0, 0, PCD_NODE_BROADCAST, 1, 0);
    TEST_ASSERT_TRUE(broadcast.isForNode(0x77));

    TEST_ASSERT_TRUE(PCD_CAN_ID(0, 0, 0x01, 0x01, 3).isForNetwork(3));
    TEST_ASSERT_FALSE(PCD_CAN_ID(0, 0, 0x01, 0x01, 3).isForNetwork(4));
}

void test_can_id_node_id(void) {
    PCD_CAN_ID id(0, 0, 0x01, 0x16, 0x03);
    TEST_ASSERT_EQUAL_UINT16(0x0316, id.nodeId());
    TEST_ASSERT_EQUAL_UINT16(0x0316, pcdMakeNodeId(3, 0x16));
    TEST_ASSERT_EQUAL_UINT8(3, pcdNodeNetwork(0x0316));
    TEST_ASSERT_EQUAL_UINT8(0x16, pcdNodeAddress(0x0316));
}

void test_node_address_validation(void) {
    TEST_ASSERT_TRUE(pcdIsValidNodeAddress(0x01));
    TEST_ASSERT_TRUE(pcdIsValidNodeAddress(0xFE));
    TEST_ASSERT_FALSE(pcdIsValidNodeAddress(PCD_NODE_INVALID));
    TEST_ASSERT_FALSE(pcdIsValidNodeAddress(PCD_NODE_BROADCAST));
}
/* ------------------------------------------------------------------ */
/* Recursos y descriptor                                               */
/* ------------------------------------------------------------------ */

void test_resource_type_names(void) {
    TEST_ASSERT_EQUAL_STRING("relay", pcdResourceTypeName(PCD_ResourceType::RELAY));
    TEST_ASSERT_EQUAL_STRING("temperature", pcdResourceTypeName(PCD_ResourceType::TEMPERATURE));
    TEST_ASSERT_EQUAL_STRING("dali", pcdResourceTypeName(PCD_ResourceType::DALI));
}

void test_resource_partitions(void) {
    TEST_ASSERT_TRUE(pcdIsStandardResource(0x10));
    TEST_ASSERT_FALSE(pcdIsStandardResource(0x00));
    TEST_ASSERT_FALSE(pcdIsStandardResource(0x80));

    TEST_ASSERT_TRUE(pcdIsExtensionResource(0x80));
    TEST_ASSERT_TRUE(pcdIsExtensionResource(0xEF));
    TEST_ASSERT_FALSE(pcdIsExtensionResource(0x7F));

    TEST_ASSERT_TRUE(pcdIsManufacturerResource(0xF0));
    TEST_ASSERT_TRUE(pcdIsManufacturerResource(0xFE));
    TEST_ASSERT_FALSE(pcdIsManufacturerResource(0xFF));
}

void test_resource_descriptor_flags(void) {
    PCD_ResourceDescriptor d;
    d.resourceType = static_cast<uint8_t>(PCD_ResourceType::TEMPERATURE);
    d.dataType = PCD_TYPE_FLOAT32;
    d.flags = PCD_RES_FLAG_READABLE | PCD_RES_FLAG_EVENT | PCD_RES_FLAG_POLL;
    d.precision = 1;

    TEST_ASSERT_TRUE(d.readable());
    TEST_ASSERT_FALSE(d.writable());
    TEST_ASSERT_TRUE(d.emitsEvents());
    TEST_ASSERT_TRUE(d.pollable());
    TEST_ASSERT_FALSE(d.persistent());

    /* Rango float reempaquetado desde bits IEEE-754. */
    d.minValue = 0x41F00000UL; /* 30.0f */
    TEST_ASSERT_EQUAL_FLOAT(30.0f, d.minFloat());
}

/* ------------------------------------------------------------------ */
/* Diagnostico y manifiesto                                           */
/* ------------------------------------------------------------------ */

void test_diagnostics_defaults(void) {
    PCD_CAN_Diagnostics d;
    TEST_ASSERT_EQUAL_UINT32(0, d.txCount);
    TEST_ASSERT_EQUAL_UINT8(PCD_DIAG_UNAVAILABLE, d.tec);
    TEST_ASSERT_EQUAL_UINT8(PCD_DIAG_UNAVAILABLE, d.rec);
    TEST_ASSERT_FALSE(d.hasTecRec());
    d.tec = 0;
    d.rec = 0;
    TEST_ASSERT_TRUE(d.hasTecRec());
}

void test_can_state_names(void) {
    TEST_ASSERT_EQUAL_STRING("BUS_OFF", pcdCanStateName(PCD_CAN_STATE_BUS_OFF));
    TEST_ASSERT_EQUAL_STRING("BUS_OK", pcdCanStateName(PCD_CAN_STATE_BUS_OK));
}

void test_firmware_manifest_compatible(void) {
    PCD_FirmwareManifest m;
    m.deviceType = 0x0201;
    m.hardwareRevision = 0x0003;
    m.imageSize = 4096;
    m.imageCRC32 = 0xABCD1234UL;

    TEST_ASSERT_TRUE(pcdManifestCompatible(m, 0x0201, 0x0003));
    TEST_ASSERT_FALSE(pcdManifestCompatible(m, 0x0201, 0x0004));
}

/* ------------------------------------------------------------------ */
/* PCD_MockCAN                                                         */
/* ------------------------------------------------------------------ */

void test_mock_two_nodes_unicast(void) {
    PCD_MockNetwork net;
    PCD_MockCAN a(&net);
    PCD_MockCAN b(&net);
    a.begin();
    b.begin();
    a.tick(0);
    b.tick(0);

    CanFrame frame;
    frame.id = 0x00000001UL;
    frame.dlc = 2;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;

    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a.send(frame));
    b.tick(0);

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b.receive(rx));
    TEST_ASSERT_EQUAL_UINT32(frame.id, rx.id);
    TEST_ASSERT_EQUAL_UINT8(0xAA, rx.data[0]);
    /* El emisor no recibe su propia trama. */
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, a.receive(rx));
}

void test_mock_drop(void) {
    PCD_MockNetwork net;
    PCD_MockCAN a(&net);
    PCD_MockCAN b(&net);
    PCD_MockCAN::FaultConfig drop;
    drop.dropProbability = 1.0f;
    b.setFaults(drop);
    a.begin();
    b.begin();
    a.tick(0);
    b.tick(0);

    CanFrame frame;
    frame.id = 1;
    frame.dlc = 1;
    frame.data[0] = 0x01;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a.send(frame));
    b.tick(0);

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, b.receive(rx));
}

void test_mock_duplicate(void) {
    PCD_MockNetwork net;
    PCD_MockCAN a(&net);
    PCD_MockCAN b(&net);
    PCD_MockCAN::FaultConfig dup;
    dup.duplicateProbability = 1.0f;
    b.setFaults(dup);
    a.begin();
    b.begin();
    a.tick(0);
    b.tick(0);

    CanFrame frame;
    frame.id = 2;
    frame.dlc = 1;
    frame.data[0] = 0x01;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a.send(frame));
    b.tick(0);

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b.receive(rx));
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b.receive(rx)); /* duplicado */
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, b.receive(rx));
}

void test_mock_delay(void) {
    PCD_MockNetwork net;
    PCD_MockCAN a(&net);
    PCD_MockCAN b(&net);
    PCD_MockCAN::FaultConfig d;
    d.delayMs = 100;
    b.setFaults(d);
    a.begin();
    b.begin();
    a.tick(0);
    b.tick(0);

    CanFrame frame;
    frame.id = 3;
    frame.dlc = 1;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a.send(frame));

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, b.receive(rx)); /* todavia no llega */

    b.tick(100);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b.receive(rx)); /* ya llego */
}

void test_mock_bus_off_and_recovery(void) {
    PCD_MockCAN solo(0);
    solo.begin();
    solo.simulateBusOff();
    CanFrame frame;
    frame.id = 1;
    frame.dlc = 0;
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_BUS_OFF, solo.send(frame));
    TEST_ASSERT_TRUE(solo.isBusOff());
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, solo.recover());
    TEST_ASSERT_FALSE(solo.isBusOff());
    TEST_ASSERT_EQUAL_UINT32(1, solo.diagnostics().busOffCount);
}

void test_mock_crc_error_corrupts(void) {
    PCD_MockNetwork net;
    PCD_MockCAN a(&net);
    PCD_MockCAN b(&net);
    PCD_MockCAN::FaultConfig f;
    f.crcErrorProbability = 1.0f;
    b.setFaults(f);
    a.begin();
    b.begin();
    a.tick(0);
    b.tick(0);

    CanFrame frame;
    frame.id = 4;
    frame.dlc = 2;
    frame.data[0] = 0x00;
    frame.data[1] = 0xFF;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, a.send(frame));
    b.tick(0);

    CanFrame rx;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, b.receive(rx));
    TEST_ASSERT_EQUAL_UINT8(0x80, rx.data[0]); /* byte 0 corrupto */
    TEST_ASSERT_EQUAL_UINT8(0xFF, rx.data[1]);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_crc8_vector);
    RUN_TEST(test_crc16_vector);
    RUN_TEST(test_crc32_vector);
    RUN_TEST(test_crc_changes_with_data);
    RUN_TEST(test_datatype_sizes);
    RUN_TEST(test_le_uint16_roundtrip);
    RUN_TEST(test_le_uint32_roundtrip);
    RUN_TEST(test_le_float32_roundtrip);
    RUN_TEST(test_write_read_value);
    RUN_TEST(test_can_id_roundtrip);
    RUN_TEST(test_can_id_addressing);
    RUN_TEST(test_can_id_node_id);
    RUN_TEST(test_node_address_validation);
    RUN_TEST(test_resource_type_names);
    RUN_TEST(test_resource_partitions);
    RUN_TEST(test_resource_descriptor_flags);
    RUN_TEST(test_diagnostics_defaults);
    RUN_TEST(test_can_state_names);
    RUN_TEST(test_firmware_manifest_compatible);
    RUN_TEST(test_mock_two_nodes_unicast);
    RUN_TEST(test_mock_drop);
    RUN_TEST(test_mock_duplicate);
    RUN_TEST(test_mock_delay);
    RUN_TEST(test_mock_bus_off_and_recovery);
    RUN_TEST(test_mock_crc_error_corrupts);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}

