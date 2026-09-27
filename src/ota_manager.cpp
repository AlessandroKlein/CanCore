#include "ota_manager.h"

#include "config_storage.h"

namespace pcd {

OtaManager::OtaManager(uint16_t source, uint8_t target)
    : source_(source),
      target_(target),
      image_(0),
      image_size_(0),
      offset_(0),
      crc_(0xFFFF),
      state_(OTA_STATE_IDLE),
      version_major_(0),
      version_minor_(0),
      hardware_id_(0) {}

bool OtaManager::begin(const uint8_t *image, size_t image_size) {
    if (image == 0 || image_size == 0 || image_size > kMaxImageBytes ||
        !isValidSource(source_) || !isValidTarget(target_)) {
        state_ = OTA_STATE_ABORTED;
        return false;
    }

    image_ = image;
    image_size_ = image_size;
    offset_ = 0;
    crc_ = crc16(image_, static_cast<uint16_t>(image_size_));
    state_ = OTA_STATE_STREAMING;
    return true;
}

bool OtaManager::nextFrame(CanFrame &out) {
    if (state_ != OTA_STATE_STREAMING || offset_ >= image_size_) {
        return false;
    }

    const size_t remaining = image_size_ - offset_;
    const uint8_t chunk_size = static_cast<uint8_t>(
        remaining > kBytesPerFrame ? kBytesPerFrame : remaining);
    const uint8_t sequence = static_cast<uint8_t>(offset_ / kBytesPerFrame);
    out = makeOtaData(source_, target_, sequence, &image_[offset_], chunk_size);
    offset_ += chunk_size;

    if (offset_ == image_size_) {
        state_ = OTA_STATE_COMPLETE;
    }
    return true;
}

void OtaManager::setFirmwareInfo(uint8_t version_major, uint8_t version_minor, uint8_t hardware_id) {
    version_major_ = version_major;
    version_minor_ = version_minor;
    hardware_id_ = hardware_id;
}

CanFrame OtaManager::startFrame() const {
    uint8_t payload[6];
    payload[0] = version_major_;
    payload[1] = version_minor_;
    writeUint32BE(&payload[2], static_cast<uint32_t>(image_size_));
    return makeOtaControl(source_, target_, OTA_START, payload, sizeof(payload));
}

CanFrame OtaManager::metaFrame(uint8_t window_size) const {
    uint8_t payload[6];
    payload[0] = hardware_id_;
    writeUint16BE(&payload[1], crc_);
    writeUint16BE(&payload[3], frameCount());
    payload[5] = window_size;
    return makeOtaControl(source_, target_, OTA_META, payload, sizeof(payload));
}

CanFrame OtaManager::finishFrame() const {
    uint8_t payload[1] = {OTA_STATUS_OK};
    return makeOtaControl(source_, target_, OTA_FINISH, payload, sizeof(payload));
}

CanFrame OtaManager::abortFrame(uint8_t status) const {
    uint8_t payload[1] = {status};
    return makeOtaControl(source_, target_, OTA_ABORT, payload, sizeof(payload));
}

bool OtaManager::applyReadyAck(const CanFrame &frame, uint8_t &status_out,
                              uint8_t &window_size_out) const {
    uint8_t payload[6];
    uint8_t command = 0;
    if (!decodeOtaControl(frame, command, payload) || command != OTA_READY_ACK) {
        return false;
    }
    status_out = payload[0];
    window_size_out = payload[1] == 0 ? 16 : payload[1];
    return true;
}

bool OtaManager::applyWindowAck(const CanFrame &frame, uint8_t &window_out,
                                uint16_t &missing_mask_out) const {
    uint8_t payload[6];
    uint8_t command = 0;
    if (!decodeOtaControl(frame, command, payload) || command != OTA_WINDOW_ACK) {
        return false;
    }
    window_out = payload[0];
    missing_mask_out = readUint16BE(&payload[1]);
    return true;
}

bool OtaManager::applyStatus(const CanFrame &frame, uint8_t &status_out,
                             uint16_t &blocks_received_out) const {
    uint8_t payload[6];
    uint8_t command = 0;
    if (!decodeOtaControl(frame, command, payload) || command != OTA_STATUS) {
        return false;
    }
    status_out = payload[0];
    blocks_received_out = readUint16BE(&payload[1]);
    return true;
}

bool OtaManager::frameAt(uint16_t sequence, CanFrame &out) const {
    if (image_ == 0 || image_size_ == 0) {
        return false;
    }
    const size_t offset = static_cast<size_t>(sequence) * kBytesPerFrame;
    if (offset >= image_size_) {
        return false;
    }
    const size_t remaining = image_size_ - offset;
    const uint8_t chunk_size =
        static_cast<uint8_t>(remaining > kBytesPerFrame ? kBytesPerFrame : remaining);
    out = makeOtaData(source_, target_, static_cast<uint8_t>(sequence), &image_[offset], chunk_size);
    return true;
}

void OtaManager::abort() {
    if (state_ == OTA_STATE_STREAMING) {
        state_ = OTA_STATE_ABORTED;
    }
}

bool OtaManager::active() const {
    return state_ == OTA_STATE_STREAMING;
}

bool OtaManager::complete() const {
    return state_ == OTA_STATE_COMPLETE;
}

OtaState OtaManager::state() const {
    return state_;
}

size_t OtaManager::imageSize() const {
    return image_size_;
}

size_t OtaManager::bytesSent() const {
    return offset_;
}

uint16_t OtaManager::imageCrc() const {
    return crc_;
}

uint16_t OtaManager::frameCount() const {
    if (image_size_ == 0) {
        return 0;
    }
    return static_cast<uint16_t>((image_size_ + kBytesPerFrame - 1) / kBytesPerFrame);
}

}  // namespace pcd
