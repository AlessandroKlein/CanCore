#include "security/hmac_authenticator.h"

#include <string.h>

#include "security/sha256.h"

namespace pcd {

static const size_t kBlockBytes = kSha256BlockBytes; /* 64 */

HmacAuthenticator::HmacAuthenticator(const uint8_t *key, size_t key_len, size_t tag_len)
    : key_len_(0), tag_len_(kHmacDefaultTagBytes) {
    memset(key_, 0, sizeof(key_));
    if (tag_len < kHmacMinTagBytes || tag_len > kHmacFullTagBytes) {
        tag_len_ = kHmacDefaultTagBytes;
    } else {
        tag_len_ = tag_len;
    }
    if (key == 0 || key_len == 0) {
        return;
    }
    if (key_len > kBlockBytes) {
        /* RFC 2104: claves largas se reemplazan por su digest. */
        Sha256::hash(key, key_len, key_);
        key_len_ = kSha256DigestBytes;
        return;
    }
    memcpy(key_, key, key_len);
    key_len_ = key_len;
}

void HmacAuthenticator::computeFull(const uint8_t *data, size_t len,
                                    uint8_t out[kHmacFullTagBytes]) const {
    uint8_t key_block[kBlockBytes];
    memset(key_block, 0, sizeof(key_block));
    memcpy(key_block, key_, key_len_);

    uint8_t inner_pad[kBlockBytes];
    uint8_t outer_pad[kBlockBytes];
    for (size_t i = 0; i < kBlockBytes; ++i) {
        inner_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x36);
        outer_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x5C);
    }

    uint8_t inner_digest[kSha256DigestBytes];
    Sha256 inner;
    inner.begin();
    inner.update(inner_pad, sizeof(inner_pad));
    inner.update(data, len);
    inner.final(inner_digest);

    Sha256 outer;
    outer.begin();
    outer.update(outer_pad, sizeof(outer_pad));
    outer.update(inner_digest, sizeof(inner_digest));
    outer.final(out);
}

bool HmacAuthenticator::sign(const uint8_t *data, size_t len, uint8_t *tag, size_t tag_len) {
    if (tag == 0 || tag_len != tag_len_ || !ready()) {
        return false;
    }
    uint8_t full[kHmacFullTagBytes];
    computeFull(data, len, full);
    memcpy(tag, full, tag_len);
    memset(full, 0, sizeof(full));
    return true;
}

bool HmacAuthenticator::verify(const uint8_t *data, size_t len, const uint8_t *tag,
                               size_t tag_len) {
    if (tag == 0 || tag_len != tag_len_ || !ready()) {
        return false;
    }
    uint8_t full[kHmacFullTagBytes];
    computeFull(data, len, full);
    const bool ok = constantTimeEquals(full, tag, tag_len);
    memset(full, 0, sizeof(full));
    return ok;
}

bool HmacAuthenticator::constantTimeEquals(const uint8_t *a, const uint8_t *b, size_t len) {
    if (a == 0 || b == 0) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff = static_cast<uint8_t>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

size_t HmacAuthenticator::signDatagram(const uint8_t *datagram, size_t len, uint8_t *out,
                                       size_t out_len) const {
    if (datagram == 0 || out == 0 || len == 0 || !ready() || out_len < len + tag_len_) {
        return 0;
    }
    uint8_t full[kHmacFullTagBytes];
    computeFull(datagram, len, full);
    memcpy(out, datagram, len);
    memcpy(&out[len], full, tag_len_);
    memset(full, 0, sizeof(full));
    return len + tag_len_;
}

bool HmacAuthenticator::verifyDatagram(const uint8_t *in, size_t len,
                                       size_t &datagram_len) const {
    if (in == 0 || !ready() || len <= tag_len_) {
        return false;
    }
    const size_t payload_len = len - tag_len_;
    uint8_t full[kHmacFullTagBytes];
    computeFull(in, payload_len, full);
    const bool ok = constantTimeEquals(full, &in[payload_len], tag_len_);
    memset(full, 0, sizeof(full));
    if (ok) {
        datagram_len = payload_len;
    }
    return ok;
}

}  // namespace pcd
