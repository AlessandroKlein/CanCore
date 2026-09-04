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
      state_(OTA_STATE_IDLE) {}

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
