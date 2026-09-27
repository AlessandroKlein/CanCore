#include "security/sha256.h"

#include <string.h>

namespace pcd {

namespace {

/* Constantes de ronda de SHA-256 (FIPS 180-4, seccion 4.2.2). */
const uint32_t kRoundConstants[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL,
    0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL,
    0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL, 0xa2bfe8a1UL, 0xa81a664bUL,
    0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL,
    0x5b9cca4fUL, 0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

const uint32_t kInitialState[8] = {0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
                                   0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL};

inline uint32_t rotr(uint32_t value, uint8_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

}  // namespace

Sha256::Sha256() : bit_length_(0), buffer_len_(0) {
    memcpy(state_, kInitialState, sizeof(state_));
    memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::begin() {
    memcpy(state_, kInitialState, sizeof(state_));
    bit_length_ = 0;
    buffer_len_ = 0;
    memset(buffer_, 0, sizeof(buffer_));
}

void Sha256::transform(const uint8_t block[kSha256BlockBytes]) {
    uint32_t w[64];
    for (uint8_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (uint8_t i = 16; i < 64; ++i) {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (uint8_t i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
        const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::absorb(const uint8_t *data, size_t len) {
    while (len > 0) {
        const size_t free_bytes = kSha256BlockBytes - buffer_len_;
        const size_t take = len < free_bytes ? len : free_bytes;
        memcpy(&buffer_[buffer_len_], data, take);
        buffer_len_ += take;
        data += take;
        len -= take;
        if (buffer_len_ == kSha256BlockBytes) {
            transform(buffer_);
            buffer_len_ = 0;
        }
    }
}

void Sha256::update(const uint8_t *data, size_t len) {
    if (data == 0 || len == 0) {
        return;
    }
    bit_length_ += static_cast<uint64_t>(len) * 8ULL;
    absorb(data, len);
}

void Sha256::final(uint8_t digest[kSha256DigestBytes]) {
    const uint64_t total_bits = bit_length_;

    /* 0x80, ceros y la longitud en bits en big-endian (FIPS 180-4, 5.1.1). */
    uint8_t padding[kSha256BlockBytes];
    memset(padding, 0, sizeof(padding));
    padding[0] = 0x80;

    const size_t remainder = static_cast<size_t>(buffer_len_);
    const size_t padding_len = (remainder < 56) ? (56 - remainder) : (120 - remainder);
    absorb(padding, padding_len);

    uint8_t length_bytes[8];
    for (uint8_t i = 0; i < 8; ++i) {
        length_bytes[i] = static_cast<uint8_t>((total_bits >> (56 - i * 8)) & 0xFF);
    }
    absorb(length_bytes, sizeof(length_bytes));

    bit_length_ = total_bits;
    if (digest == 0) {
        return;
    }
    for (uint8_t i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
    }
}

void Sha256::hash(const uint8_t *data, size_t len, uint8_t digest[kSha256DigestBytes]) {
    Sha256 context;
    context.begin();
    context.update(data, len);
    context.final(digest);
}

}  // namespace pcd
