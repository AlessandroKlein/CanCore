/*
 * Pruebas de PCD v6.2: seguridad (provider + anti-replay), identidad y
 * negociacion de version/capacidades.
 */

#include <unity.h>

#include <string.h>

#include "v6/pcd_v6.h"

using namespace pcd;

void test_crc_only_provider(void) {
    PCD_CrcOnlyProvider sec;
    TEST_ASSERT_EQUAL_UINT8(PCD_SECURITY_CRC_ONLY, sec.mode());

    const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t tag[4];
    TEST_ASSERT_TRUE(sec.sign(data, sizeof(data), tag, sizeof(tag)));
    TEST_ASSERT_TRUE(sec.verify(data, sizeof(data), tag, sizeof(tag)));

    tag[0] = static_cast<uint8_t>(tag[0] ^ 0x01);
    TEST_ASSERT_FALSE(sec.verify(data, sizeof(data), tag, sizeof(tag)));
    TEST_ASSERT_FALSE(sec.encrypt(data, sizeof(data), tag, 0));
}

void test_no_security_provider(void) {
    PCD_NoSecurityProvider sec;
    TEST_ASSERT_EQUAL_UINT8(PCD_SECURITY_NONE, sec.mode());

    uint8_t out[8];
    size_t out_len = sizeof(out);
    const uint8_t in[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    TEST_ASSERT_TRUE(sec.encrypt(in, 4, out, &out_len));
    TEST_ASSERT_EQUAL_UINT(4, out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 4);
    TEST_ASSERT_TRUE(sec.sign(in, 4, out, sizeof(out)));
}

void test_replay_accepts_new(void) {
    PCD_ReplayGuard guard;
    TEST_ASSERT_TRUE(guard.accept(0x0C, 1));
    TEST_ASSERT_TRUE(guard.accept(0x0C, 2));
    TEST_ASSERT_TRUE(guard.accept(0x0C, 3));
    TEST_ASSERT_FALSE(guard.isReplay(0x0C, 4));
}

void test_replay_rejects_duplicate(void) {
    PCD_ReplayGuard guard;
    TEST_ASSERT_TRUE(guard.accept(0x0C, 100));
    TEST_ASSERT_TRUE(guard.isReplay(0x0C, 100));
    TEST_ASSERT_FALSE(guard.accept(0x0C, 100));
}

void test_replay_rejects_too_old(void) {
    PCD_ReplayGuard guard;
    TEST_ASSERT_TRUE(guard.accept(0x0C, 100));
    TEST_ASSERT_TRUE(guard.isReplay(0x0C, 50)); /* 50 contadores atras: fuera de ventana */
}

void test_replay_within_window(void) {
    PCD_ReplayGuard guard;
    TEST_ASSERT_TRUE(guard.accept(0x0C, 100));
    TEST_ASSERT_TRUE(guard.accept(0x0C, 99)); /* dentro de ventana, no visto */
    TEST_ASSERT_TRUE(guard.isReplay(0x0C, 99)); /* ahora si visto */
    TEST_ASSERT_TRUE(guard.accept(0x0C, 98));
    TEST_ASSERT_TRUE(guard.accept(0x0C, 150)); /* salta adelante */
    TEST_ASSERT_TRUE(guard.isReplay(0x0C, 100));
}

void test_identity_equals(void) {
    PCD_DeviceIdentity a;
    PCD_DeviceIdentity b;
    uint8_t uuid[16];
    uint8_t key[32];
    for (uint8_t i = 0; i < 16; ++i) uuid[i] = i;
    for (uint8_t i = 0; i < 32; ++i) key[i] = i;

    a.setUuid(uuid);
    a.setKey(key);
    b.setUuid(uuid);
    b.setKey(key);
    TEST_ASSERT_TRUE(pcdIdentityEquals(a, b));

    key[0] = 0xFF;
    PCD_DeviceIdentity c;
    c.setUuid(uuid);
    c.setKey(key);
    TEST_ASSERT_FALSE(pcdIdentityEquals(a, c));
}

void test_negotiate_protocol(void) {
    uint8_t major = 0, minor = 0;
    TEST_ASSERT_TRUE(pcdNegotiateProtocol(6, 2, 6, 5, major, minor));
    TEST_ASSERT_EQUAL_UINT8(6, major);
    TEST_ASSERT_EQUAL_UINT8(2, minor); /* min(2,5) */

    TEST_ASSERT_FALSE(pcdNegotiateProtocol(6, 2, 7, 0, major, minor));
}

void test_common_features(void) {
    const uint32_t a = PCD_FEATURE_DISCOVERY | PCD_FEATURE_RESOURCE | PCD_FEATURE_OTA;
    const uint32_t b = PCD_FEATURE_RESOURCE | PCD_FEATURE_OTA | PCD_FEATURE_SECURITY;
    TEST_ASSERT_EQUAL_UINT32(PCD_FEATURE_RESOURCE | PCD_FEATURE_OTA, pcdCommonFeatures(a, b));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_only_provider);
    RUN_TEST(test_no_security_provider);
    RUN_TEST(test_replay_accepts_new);
    RUN_TEST(test_replay_rejects_duplicate);
    RUN_TEST(test_replay_rejects_too_old);
    RUN_TEST(test_replay_within_window);
    RUN_TEST(test_identity_equals);
    RUN_TEST(test_negotiate_protocol);
    RUN_TEST(test_common_features);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
