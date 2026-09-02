#include "tunnel/tunnel_engine.h"

#include <string.h>

#include "config_storage.h"  /* crc16() */

namespace pcd {

static const size_t kTunnelHeaderBytes = 6;  /* magia + id + dlc */
static const size_t kTunnelPayloadMax = 8;

bool encodeTunnelFrame(const CanFrame &frame, uint8_t *out, size_t out_len) {
    if (out_len < kTunnelDatagramBytes || out == 0) {
        return false;
    }
    memset(out, 0, kTunnelDatagramBytes);

    out[0] = kTunnelMagic;
    out[1] = static_cast<uint8_t>(frame.id & 0xFF);
    out[2] = static_cast<uint8_t>((frame.id >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>((frame.id >> 16) & 0xFF);
    out[4] = static_cast<uint8_t>((frame.id >> 24) & 0xFF);

    const uint8_t dlc = (frame.dlc > kTunnelPayloadMax) ? kTunnelPayloadMax : frame.dlc;
    out[5] = frame.dlc & 0x0F;

    memcpy(&out[6], frame.data, dlc);

    const uint16_t crc = crc16(out, kTunnelHeaderBytes + dlc);
    out[14] = static_cast<uint8_t>(crc & 0xFF);
    out[15] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return true;
}

bool decodeTunnelFrame(const uint8_t *in, size_t in_len, CanFrame &frame) {
    if (in == 0 || in_len != kTunnelDatagramBytes) {
        return false;
    }
    if (in[0] != kTunnelMagic) {
        return false;
    }
    const uint8_t dlc = in[5] & 0x0F;
    if (dlc > kTunnelPayloadMax) {
        return false;
    }

    const uint16_t expected_crc = crc16(in, kTunnelHeaderBytes + dlc);
    const uint16_t stored_crc = static_cast<uint16_t>(in[14]) |
                                (static_cast<uint16_t>(in[15]) << 8);
    if (expected_crc != stored_crc) {
        return false;
    }

    frame.id = static_cast<uint32_t>(in[1]) | (static_cast<uint32_t>(in[2]) << 8) |
               (static_cast<uint32_t>(in[3]) << 16) | (static_cast<uint32_t>(in[4]) << 24);
    frame.id &= kExtendedIdMask;
    frame.dlc = dlc;
    memset(frame.data, 0, sizeof(frame.data));
    memcpy(frame.data, &in[6], dlc);
    return true;
}

TunnelEngine::TunnelEngine(ITunnelTransport &transport) : transport_(transport) {}

bool TunnelEngine::sendFrame(uint16_t peer, const CanFrame &frame) {
    uint8_t datagram[kTunnelDatagramBytes];
    if (!encodeTunnelFrame(frame, datagram, sizeof(datagram))) {
        return false;
    }
    return transport_.send(peer, datagram, sizeof(datagram));
}

bool TunnelEngine::receiveFrame(uint16_t &peer_out, CanFrame &frame) {
    uint8_t datagram[kTunnelDatagramBytes];
    uint16_t peer = kBroadcastPeer;
    size_t len = 0;
    if (!transport_.receive(peer, datagram, sizeof(datagram), len)) {
        return false;
    }
    if (!decodeTunnelFrame(datagram, len, frame)) {
        /* Datagrama corrupto o ajeno: se descarta. */
        return false;
    }
    peer_out = peer;
    return true;
}

bool TunnelEngine::available() const {
    return transport_.available();
}

}  // namespace pcd
