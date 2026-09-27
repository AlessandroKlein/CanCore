#include "ota_receiver.h"

#include <string.h>

#include "config_storage.h"

namespace pcd {

OtaReceiver::OtaReceiver(uint16_t node_id, uint8_t hardware_id, IOtaTarget *target)
    : node_id_(node_id),
      hardware_id_(hardware_id),
      target_(target),
      state_(OTA_RX_IDLE),
      timeout_ms_(2000),
      sender_address_(0),
      version_major_(0),
      version_minor_(0),
      image_size_(0),
      total_blocks_(0),
      expected_crc_(0),
      window_size_(CAN_OTA_WINDOW_SLOTS > 16 ? 16 : CAN_OTA_WINDOW_SLOTS),
      window_index_(0),
      window_base_(0),
      window_mask_(0),
      blocks_received_(0),
      bytes_written_(0),
      computed_crc_(0xFFFF),
      last_activity_ms_(0),
      timeouts_(0),
      last_status_(OTA_STATUS_OK),
      image_verified_(false) {
    memset(slots_, 0, sizeof(slots_));
    memset(slot_len_, 0, sizeof(slot_len_));
}

const char *OtaReceiver::stateName() const {
    switch (state_) {
        case OTA_RX_IDLE: return "idle";
        case OTA_RX_VALIDATING: return "validating";
        case OTA_RX_RECEIVING: return "receiving";
        case OTA_RX_VERIFYING: return "verifying";
        case OTA_RX_DONE: return "done";
        default: return "failed";
    }
}

void OtaReceiver::reset() {
    if (target_ != 0 && (state_ == OTA_RX_RECEIVING || state_ == OTA_RX_VERIFYING)) {
        target_->abort();
    }
    state_ = OTA_RX_IDLE;
    sender_address_ = 0;
    version_major_ = 0;
    version_minor_ = 0;
    image_size_ = 0;
    total_blocks_ = 0;
    expected_crc_ = 0;
    window_index_ = 0;
    window_base_ = 0;
    window_mask_ = 0;
    blocks_received_ = 0;
    bytes_written_ = 0;
    computed_crc_ = 0xFFFF;
    timeouts_ = 0;
    last_status_ = OTA_STATUS_OK;
    image_verified_ = false;
    memset(slot_len_, 0, sizeof(slot_len_));
}

uint16_t OtaReceiver::fullWindowMask() const {
    const uint8_t size = window_size_ >= 16 ? 16 : window_size_;
    return size >= 16 ? 0xFFFF : static_cast<uint16_t>((1U << size) - 1U);
}

uint16_t OtaReceiver::expectedSlotCount() const {
    if (total_blocks_ <= window_base_) {
        return 0;
    }
    const uint16_t remaining = static_cast<uint16_t>(total_blocks_ - window_base_);
    return remaining > window_size_ ? window_size_ : remaining;
}

uint16_t OtaReceiver::missingMask() const {
    uint16_t expected = 0;
    const uint16_t slots = expectedSlotCount();
    for (uint8_t i = 0; i < slots; ++i) {
        expected |= static_cast<uint16_t>(1U << i);
    }
    return static_cast<uint16_t>(expected & ~window_mask_);
}

void OtaReceiver::fail(uint8_t status) {
    last_status_ = status;
    state_ = OTA_RX_FAILED;
    if (target_ != 0) {
        target_->abort();
    }
}

bool OtaReceiver::flushSlots(uint16_t count) {
    if (target_ == 0) {
        return false;
    }
    for (uint16_t i = 0; i < count; ++i) {
        if (slot_len_[i] == 0) {
            return false; /* hueco: la flash no se puede escribir en desorden */
        }
        if (!target_->writeBlock(static_cast<uint16_t>(window_base_ + i), slots_[i],
                                 slot_len_[i])) {
            return false;
        }
        computed_crc_ = crc16(slots_[i], slot_len_[i], computed_crc_);
        bytes_written_ += slot_len_[i];
        slot_len_[i] = 0;
    }
    return true;
}

bool OtaReceiver::acceptBlock(uint16_t sequence, const uint8_t *data, uint8_t len, uint32_t now_ms,
                              CanFrame &response, bool &needs_response) {
    if (len == 0 || len > kPayloadSize - 1) {
        return false;
    }
    if (sequence < window_base_) {
        return true; /* bloque ya confirmado en una ventana anterior */
    }
    if (sequence >= static_cast<uint16_t>(window_base_ + window_size_)) {
        uint8_t payload[6] = {OTA_STATUS_SEQUENCE_ERROR, 0, 0, 0, 0, 0};
        response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, payload, sizeof(payload));
        needs_response = true;
        return true;
    }

    const uint8_t slot = static_cast<uint8_t>(sequence - window_base_);
    const uint16_t bit = static_cast<uint16_t>(1U << slot);
    if ((window_mask_ & bit) == 0) {
        memcpy(slots_[slot], data, len);
        slot_len_[slot] = len;
        window_mask_ |= bit;
        ++blocks_received_;
    }
    last_activity_ms_ = now_ms;
    timeouts_ = 0;

    if (expectedSlotCount() > 0 && window_mask_ == fullWindowMask()) {
        const uint16_t slots = expectedSlotCount();
        if (!flushSlots(slots)) {
            fail(OTA_STATUS_FLASH_ERROR);
            uint8_t aborted[1] = {OTA_STATUS_FLASH_ERROR};
            response =
                makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted, sizeof(aborted));
            needs_response = true;
            return true;
        }
        const uint8_t window_done = static_cast<uint8_t>(window_index_);
        window_base_ = static_cast<uint16_t>(window_base_ + window_size_);
        window_mask_ = 0;
        ++window_index_;
        uint8_t payload[6] = {window_done, 0, 0, OTA_STATUS_OK, 0, 0};
        response =
            makeOtaControl(node_id_, sender_address_, OTA_WINDOW_ACK, payload, sizeof(payload));
        needs_response = true;
    }
    return true;
}

bool OtaReceiver::handleControl(const CanFrame &frame, uint8_t command, const uint8_t *payload,
                                uint32_t now_ms, CanFrame &response, bool &needs_response) {
    (void)frame;
    switch (command) {
        case OTA_START: {
            image_size_ = readUint32BE(&payload[2]);
            version_major_ = payload[0];
            version_minor_ = payload[1];
            window_index_ = 0;
            window_base_ = 0;
            window_mask_ = 0;
            blocks_received_ = 0;
            bytes_written_ = 0;
            computed_crc_ = 0xFFFF;
            timeouts_ = 0;
            image_verified_ = false;
            last_activity_ms_ = now_ms;
            if (image_size_ == 0) {
                fail(OTA_STATUS_BAD_REQUEST);
                uint8_t aborted[1] = {OTA_STATUS_BAD_REQUEST};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            state_ = OTA_RX_VALIDATING;
            uint8_t status[6] = {OTA_STATUS_OK, 0, 0, 0, 0, 0};
            response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, status,
                                      sizeof(status));
            needs_response = true;
            return true;
        }

        case OTA_META: {
            if (state_ != OTA_RX_VALIDATING) {
                uint8_t status[6] = {OTA_STATUS_BAD_REQUEST, 0, 0, 0, 0, 0};
                response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, status,
                                          sizeof(status));
                needs_response = true;
                return true;
            }
            const uint8_t hardware = payload[0];
            expected_crc_ = readUint16BE(&payload[1]);
            total_blocks_ = readUint16BE(&payload[3]);
            const uint8_t requested_window = payload[5];
            if (requested_window > 0 && requested_window < window_size_) {
                window_size_ = requested_window;
            }
            const uint16_t expected_blocks =
                static_cast<uint16_t>((image_size_ + (kPayloadSize - 2)) / (kPayloadSize - 1));
            if (hardware != hardware_id_) {
                fail(OTA_STATUS_HW_MISMATCH);
                uint8_t aborted[1] = {OTA_STATUS_HW_MISMATCH};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            if (total_blocks_ == 0 || total_blocks_ != expected_blocks ||
                total_blocks_ > kOtaMaxDataBlocks) {
                fail(OTA_STATUS_BAD_REQUEST);
                uint8_t aborted[1] = {OTA_STATUS_BAD_REQUEST};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            if (target_ == 0 || !target_->begin(hardware_id_, image_size_)) {
                fail(OTA_STATUS_NO_SPACE);
                uint8_t aborted[1] = {OTA_STATUS_NO_SPACE};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            state_ = OTA_RX_RECEIVING;
            last_activity_ms_ = now_ms;
            timeouts_ = 0;
            uint8_t ready[6] = {OTA_STATUS_OK, window_size_, 0, 0, 0, 0};
            response =
                makeOtaControl(node_id_, sender_address_, OTA_READY_ACK, ready, sizeof(ready));
            needs_response = true;
            return true;
        }

        default:
            break;
    }
    return handleFinishOrAbort(command, payload, response, needs_response);
}

bool OtaReceiver::handleFinishOrAbort(uint8_t command, const uint8_t *payload, CanFrame &response,
                                      bool &needs_response) {
    switch (command) {
        case OTA_FINISH: {
            if (state_ != OTA_RX_RECEIVING) {
                uint8_t status[6] = {OTA_STATUS_NOT_READY, 0, 0, 0, 0, 0};
                response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, status,
                                          sizeof(status));
                needs_response = true;
                return true;
            }
            if (blocks_received_ != total_blocks_) {
                const uint8_t window_done = static_cast<uint8_t>(window_index_);
                const uint16_t missing = missingMask();
                uint8_t ack[6] = {window_done, static_cast<uint8_t>(missing >> 8),
                                  static_cast<uint8_t>(missing & 0xFF), OTA_STATUS_OK, 0, 0};
                response = makeOtaControl(node_id_, sender_address_, OTA_WINDOW_ACK, ack,
                                          sizeof(ack));
                needs_response = true;
                return true;
            }
            if (!flushSlots(expectedSlotCount())) {
                fail(OTA_STATUS_FLASH_ERROR);
                uint8_t aborted[1] = {OTA_STATUS_FLASH_ERROR};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            state_ = OTA_RX_VERIFYING;
            if (computed_crc_ != expected_crc_) {
                fail(OTA_STATUS_CRC_ERROR);
                uint8_t aborted[1] = {OTA_STATUS_CRC_ERROR};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            if (target_ == 0 || !target_->finish()) {
                fail(OTA_STATUS_FLASH_ERROR);
                uint8_t aborted[1] = {OTA_STATUS_FLASH_ERROR};
                response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted,
                                          sizeof(aborted));
                needs_response = true;
                return true;
            }
            image_verified_ = true;
            last_status_ = OTA_STATUS_OK;
            state_ = OTA_RX_DONE;
            uint8_t ok[1] = {OTA_STATUS_OK};
            response = makeOtaControl(node_id_, sender_address_, OTA_FINISH, ok, sizeof(ok));
            needs_response = true;
            return true;
        }

        case OTA_ABORT: {
            /* El gateway cancela: se libera el medio y se confirma el aborto. */
            const bool active = state_ == OTA_RX_RECEIVING || state_ == OTA_RX_VERIFYING ||
                                state_ == OTA_RX_VALIDATING;
            if (target_ != 0 && active) {
                target_->abort();
            }
            state_ = OTA_RX_IDLE;
            last_status_ = payload[0];
            uint8_t status[6] = {payload[0], static_cast<uint8_t>(blocks_received_ >> 8),
                                 static_cast<uint8_t>(blocks_received_ & 0xFF), 0, 0, 0};
            response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, status,
                                      sizeof(status));
            needs_response = true;
            return true;
        }

        default:
            return false;
    }
}

bool OtaReceiver::handleFrame(const CanFrame &frame, uint32_t now_ms, CanFrame &response,
                              bool &needs_response) {
    needs_response = false;
    const CanId id = frame.fields();
    if (id.msg_type != MSG_OTA || frame.dlc == 0) {
        return false;
    }
    if (id.target != address() && id.target != kBroadcastTarget) {
        return false;
    }
    if (!isValidSource(id.source)) {
        return false;
    }
    sender_address_ = nodeIdAddress(id.source);

    if (isOtaControlFrame(frame)) {
        uint8_t payload[6];
        uint8_t command = 0;
        if (!decodeOtaControl(frame, command, payload)) {
            return false;
        }
        return handleControl(frame, command, payload, now_ms, response, needs_response);
    }

    if (state_ != OTA_RX_RECEIVING) {
        return false;
    }
    const uint16_t sequence = frame.data[0];
    const uint8_t len = static_cast<uint8_t>(frame.dlc - 1);
    return acceptBlock(sequence, &frame.data[1], len, now_ms, response, needs_response);
}

bool OtaReceiver::poll(uint32_t now_ms, CanFrame &response) {
    if (state_ != OTA_RX_RECEIVING) {
        return false;
    }
    if ((now_ms - last_activity_ms_) < timeout_ms_) {
        return false;
    }
    last_activity_ms_ = now_ms;
    ++timeouts_;
    if (timeouts_ >= CAN_OTA_MAX_TIMEOUTS) {
        fail(OTA_STATUS_TIMEOUT);
        uint8_t aborted[1] = {OTA_STATUS_TIMEOUT};
        response = makeOtaControl(node_id_, sender_address_, OTA_ABORT, aborted, sizeof(aborted));
        return true;
    }
    uint8_t payload[6] = {OTA_STATUS_TIMEOUT, static_cast<uint8_t>(blocks_received_ >> 8),
                          static_cast<uint8_t>(blocks_received_ & 0xFF), 0, 0, 0};
    response = makeOtaControl(node_id_, sender_address_, OTA_STATUS, payload, sizeof(payload));
    return true;
}

}  // namespace pcd
