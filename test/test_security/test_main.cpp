/*
 * Pruebas de SHA-256 y HMAC-SHA256 contra los vectores oficiales:
 *
 *   - FIPS 180-4 / NIST: "abc", el mensaje de 448 bits y la cadena vacia,
 *   - RFC 4231: casos 1, 2, 6 y 7 (claves de 20, 4 y 131 bytes).
 *
 * Como el tag del tunel se trunca, se comprueba ademas que un tag manipulado se
 * rechaza y que el datagrama firmado sigue siendo decodificable.
 */

#include <unity.h>

#include <string.h>

#include "security/hmac_authenticator.h"
#include "security/sha256.h"
#include "tunnel/tunnel_engine.h"

using namespace pcd;

/* Convierte "ba7816bf..." en bytes. */
static void fromHex(const char *hex, uint8_t *out, size_t out_len) {
    for (size_t i = 0; i < out_len; ++i) {
        const char hi = hex[i * 2];
        const char lo = hex[i * 2 + 1];
        const uint8_t vh = static_cast<uint8_t>(hi <= '9' ? hi - '0' : (hi | 0x20) - 'a' + 10);
        const uint8_t vl = static_cast<uint8_t>(lo <= '9' ? lo - '0' : (lo | 0x20) - 'a' + 10);
        out[i] = static_cast<uint8_t>((vh << 4) | vl);
    }
}

void test_sha256_fips_abc(void) {
    static const char *kExpected =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    uint8_t expected[kSha256DigestBytes];
    uint8_t digest[kSha256DigestBytes];
    fromHex(kExpected, expected, sizeof(expected));

    Sha256::hash(reinterpret_cast<const uint8_t *>("abc"), 3, digest);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, digest, kSha256DigestBytes);
}

void test_sha256_fips_448_bit_message(void) {
    static const char *kExpected =
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
    static const uint8_t kMessage[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t expected[kSha256DigestBytes];
    uint8_t digest[kSha256DigestBytes];
    fromHex(kExpected, expected, sizeof(expected));

    Sha256::hash(kMessage, sizeof(kMessage) - 1, digest);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, digest, kSha256DigestBytes);
}

void test_sha256_empty_and_incremental_match(void) {
    static const char *kExpected =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    uint8_t expected[kSha256DigestBytes];
    uint8_t digest[kSha256DigestBytes];
    fromHex(kExpected, expected, sizeof(expected));

    Sha256::hash(0, 0, digest);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, digest, kSha256DigestBytes);

    /* Actualizacion troceada (65 bytes en tres llamadas) debe dar el mismo digest
     * que la version de una sola pasada. */
    static const char kText[] =
        "El par trenzado transporta CAN_H y CAN_L con 120 ohm en los dos extremos.";
    uint8_t one_shot[kSha256DigestBytes];
    uint8_t chunked[kSha256DigestBytes];
    const size_t len = sizeof(kText) - 1;
    Sha256::hash(reinterpret_cast<const uint8_t *>(kText), len, one_shot);

    Sha256 context;
    context.begin();
    context.update(reinterpret_cast<const uint8_t *>(kText), 7);
    context.update(reinterpret_cast<const uint8_t *>(kText) + 7, 58);
    context.update(reinterpret_cast<const uint8_t *>(kText) + 65, len - 65);
    context.final(chunked);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(one_shot, chunked, kSha256DigestBytes);
}
void test_hmac_rfc4231_case1(void) {
    /* key = 0x0b x 20, data = "Hi There" */
    static const char *kExpected =
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7";
    uint8_t key[20];
    uint8_t expected[kHmacFullTagBytes];
    uint8_t tag[kHmacFullTagBytes];
    memset(key, 0x0b, sizeof(key));
    fromHex(kExpected, expected, sizeof(expected));

    HmacAuthenticator auth(key, sizeof(key));
    auth.computeFull(reinterpret_cast<const uint8_t *>("Hi There"), 8, tag);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tag, kHmacFullTagBytes);
    TEST_ASSERT_EQUAL_UINT(20, auth.keyBytes());
    TEST_ASSERT_EQUAL_UINT(16, auth.tagBytes());
}

void test_hmac_rfc4231_case2_short_key(void) {
    /* key = "Jefe", data = "what do ya want for nothing?" */
    static const char *kExpected =
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
    uint8_t expected[kHmacFullTagBytes];
    uint8_t tag[kHmacFullTagBytes];
    fromHex(kExpected, expected, sizeof(expected));

    HmacAuthenticator auth(reinterpret_cast<const uint8_t *>("Jefe"), 4);
    auth.computeFull(reinterpret_cast<const uint8_t *>("what do ya want for nothing?"), 28, tag);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tag, kHmacFullTagBytes);
}

void test_hmac_rfc4231_case6_long_key(void) {
    /* RFC 4231 caso 6: key = 0xaa x 131 (mas larga que el bloque, se reemplaza
     * por su SHA-256), data = "Test Using Larger Than Block-Size Key - Hash Key First". */
    static const char *kExpected =
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54";
    static const char *kData = "Test Using Larger Than Block-Size Key - Hash Key First";
    uint8_t key[131];
    uint8_t expected[kHmacFullTagBytes];
    uint8_t tag[kHmacFullTagBytes];
    memset(key, 0xaa, sizeof(key));
    fromHex(kExpected, expected, sizeof(expected));

    HmacAuthenticator auth(key, sizeof(key));
    TEST_ASSERT_EQUAL_UINT(kSha256DigestBytes, auth.keyBytes());
    auth.computeFull(reinterpret_cast<const uint8_t *>(kData), strlen(kData), tag);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tag, kHmacFullTagBytes);
}

void test_hmac_rfc4231_case7_long_key_and_data(void) {
    static const char *kExpected =
        "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2";
    static const char *kData =
        "This is a test using a larger than block-size key and a larger than block-size data. "
        "The key needs to be hashed before being used by the HMAC algorithm.";
    uint8_t key[131];
    uint8_t expected[kHmacFullTagBytes];
    uint8_t tag[kHmacFullTagBytes];
    memset(key, 0xaa, sizeof(key));
    fromHex(kExpected, expected, sizeof(expected));

    HmacAuthenticator auth(key, sizeof(key));
    auth.computeFull(reinterpret_cast<const uint8_t *>(kData), strlen(kData), tag);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tag, kHmacFullTagBytes);
}
void test_hmac_truncated_tag_sign_and_verify(void) {
    const uint8_t key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    HmacAuthenticator auth(key, sizeof(key), 16);
    const uint8_t data[3] = {0xAA, 0xBB, 0xCC};
    uint8_t tag[16] = {0};

    TEST_ASSERT_TRUE(auth.sign(data, sizeof(data), tag, sizeof(tag)));
    TEST_ASSERT_TRUE(auth.verify(data, sizeof(data), tag, sizeof(tag)));

    tag[15] = static_cast<uint8_t>(tag[15] ^ 0x01);
    TEST_ASSERT_FALSE(auth.verify(data, sizeof(data), tag, sizeof(tag)));

    /* Un tag de longitud distinta a la configurada se rechaza. */
    TEST_ASSERT_FALSE(auth.sign(data, sizeof(data), tag, 8));
    TEST_ASSERT_FALSE(auth.verify(data, sizeof(data), tag, 8));
}

void test_hmac_rejects_without_key(void) {
    HmacAuthenticator auth(0, 0);
    uint8_t tag[16] = {0};
    const uint8_t data[1] = {0x01};
    TEST_ASSERT_FALSE(auth.ready());
    TEST_ASSERT_FALSE(auth.sign(data, sizeof(data), tag, sizeof(tag)));
    TEST_ASSERT_FALSE(auth.verify(data, sizeof(data), tag, sizeof(tag)));
}

void test_hmac_tag_length_defaults(void) {
    const uint8_t key[8] = {0};
    TEST_ASSERT_EQUAL_UINT(kHmacDefaultTagBytes, HmacAuthenticator(key, sizeof(key), 3).tagBytes());
    TEST_ASSERT_EQUAL_UINT(kHmacDefaultTagBytes,
                           HmacAuthenticator(key, sizeof(key), 64).tagBytes());
    TEST_ASSERT_EQUAL_UINT(8, HmacAuthenticator(key, sizeof(key), 8).tagBytes());
    TEST_ASSERT_EQUAL_UINT(kHmacFullTagBytes,
                           HmacAuthenticator(key, sizeof(key), 32).tagBytes());
}

void test_authenticated_tunnel_datagram_roundtrip(void) {
    uint8_t key[32];
    memset(key, 0x10, sizeof(key));
    HmacAuthenticator auth(key, sizeof(key), 16);

    const CanFrame frame = makeStateBroadcast(0x0016, RES_RELAY, 2, 42.5f);
    uint8_t datagram[kTunnelDatagramBytes];
    TEST_ASSERT_TRUE(encodeTunnelFrame(frame, datagram, sizeof(datagram)));

    /* Datagrama + tag. */
    uint8_t signed_datagram[kTunnelDatagramBytes + 16];
    const size_t signed_len =
        auth.signDatagram(datagram, sizeof(datagram), signed_datagram, sizeof(signed_datagram));
    TEST_ASSERT_EQUAL_UINT(kTunnelDatagramBytes + 16, signed_len);

    /* Verificacion y decodificacion posterior. */
    size_t payload_len = 0;
    TEST_ASSERT_TRUE(auth.verifyDatagram(signed_datagram, signed_len, payload_len));
    TEST_ASSERT_EQUAL_UINT(kTunnelDatagramBytes, payload_len);

    CanFrame decoded;
    TEST_ASSERT_TRUE(decodeTunnelFrame(signed_datagram, payload_len, decoded));
    TEST_ASSERT_EQUAL_HEX32(frame.id, decoded.id);
    TEST_ASSERT_EQUAL_FLOAT(42.5f, frameValue(decoded));

    /* Un byte alterado del datagrama invalida el tag. */
    signed_datagram[7] = static_cast<uint8_t>(signed_datagram[7] ^ 0x40);
    size_t ignored = 0;
    TEST_ASSERT_FALSE(auth.verifyDatagram(signed_datagram, signed_len, ignored));

    /* Un datagrama sin tag se rechaza. */
    TEST_ASSERT_FALSE(auth.verifyDatagram(datagram, sizeof(datagram), ignored));
}

void test_constant_time_compare(void) {
    const uint8_t a[4] = {1, 2, 3, 4};
    const uint8_t b[4] = {1, 2, 3, 4};
    const uint8_t c[4] = {1, 2, 3, 5};
    TEST_ASSERT_TRUE(HmacAuthenticator::constantTimeEquals(a, b, sizeof(a)));
    TEST_ASSERT_FALSE(HmacAuthenticator::constantTimeEquals(a, c, sizeof(a)));
    TEST_ASSERT_FALSE(HmacAuthenticator::constantTimeEquals(0, b, sizeof(a)));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_sha256_fips_abc);
    RUN_TEST(test_sha256_fips_448_bit_message);
    RUN_TEST(test_sha256_empty_and_incremental_match);
    RUN_TEST(test_hmac_rfc4231_case1);
    RUN_TEST(test_hmac_rfc4231_case2_short_key);
    RUN_TEST(test_hmac_rfc4231_case6_long_key);
    RUN_TEST(test_hmac_rfc4231_case7_long_key_and_data);
    RUN_TEST(test_hmac_truncated_tag_sign_and_verify);
    RUN_TEST(test_hmac_rejects_without_key);
    RUN_TEST(test_hmac_tag_length_defaults);
    RUN_TEST(test_authenticated_tunnel_datagram_roundtrip);
    RUN_TEST(test_constant_time_compare);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}