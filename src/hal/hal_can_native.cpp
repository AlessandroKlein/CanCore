#if !defined(ARDUINO_ARCH_ESP32) && !defined(__AVR__)

#include "hal/hal_can_native.h"

namespace pcd {

VirtualBus::VirtualBus() : node_count_(0), traffic_(0) {
    for (size_t i = 0; i < kMaxNodes; ++i) {
        nodes_[i] = NULL;
    }
}

void VirtualBus::attach(NativeCanBus *node) {
    if (node_count_ < kMaxNodes) {
        nodes_[node_count_++] = node;
    }
}

void VirtualBus::detach(NativeCanBus *node) {
    for (size_t i = 0; i < node_count_; ++i) {
        if (nodes_[i] == node) {
            for (size_t j = i; j + 1 < node_count_; ++j) {
                nodes_[j] = nodes_[j + 1];
            }
            nodes_[--node_count_] = NULL;
            return;
        }
    }
}

void VirtualBus::broadcast(const CanFrame &frame, NativeCanBus *sender) {
    ++traffic_;
    for (size_t i = 0; i < node_count_; ++i) {
        if (nodes_[i] != sender) {
            nodes_[i]->deliver(frame);
        }
    }
}

NativeCanBus::NativeCanBus(VirtualBus *bus)
    : bus_(bus),
      filter_(CanFilter::acceptAll()),
      started_(false),
      bus_off_(false),
      rx_head_(0),
      rx_count_(0),
      sent_count_(0) {}

NativeCanBus::~NativeCanBus() {
    end();
}

CanStatus NativeCanBus::begin(uint32_t bitrate) {
    (void)bitrate;
    if (!started_ && bus_ != NULL) {
        bus_->attach(this);
    }
    started_ = true;
    bus_off_ = false;
    return CAN_OK;
}

void NativeCanBus::end() {
    if (started_ && bus_ != NULL) {
        bus_->detach(this);
    }
    started_ = false;
    rx_head_ = 0;
    rx_count_ = 0;
}

CanStatus NativeCanBus::send(const CanFrame &frame, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    if (bus_off_) {
        return CAN_ERR_BUS_OFF;
    }
    last_sent_ = frame;
    ++sent_count_;
    if (bus_ != NULL) {
        bus_->broadcast(frame, this);
    }
    return CAN_OK;
}

CanStatus NativeCanBus::receive(CanFrame &frame, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    if (rx_count_ == 0) {
        return CAN_ERR_NO_DATA;
    }
    frame = rx_queue_[rx_head_];
    rx_head_ = (rx_head_ + 1) % kNativeQueueSize;
    --rx_count_;
    return CAN_OK;
}

void NativeCanBus::setFilter(const CanFilter &filter) {
    filter_ = filter;
}

bool NativeCanBus::isBusOff() const {
    return bus_off_;
}

CanStatus NativeCanBus::recover() {
    bus_off_ = false;
    return CAN_OK;
}

void NativeCanBus::deliver(const CanFrame &frame) {
    if (!started_ || !filter_.accepts(frame.id)) {
        return;
    }
    if (rx_count_ >= kNativeQueueSize) {
        /* Cola llena: se descarta la trama mas antigua. */
        rx_head_ = (rx_head_ + 1) % kNativeQueueSize;
        --rx_count_;
    }
    rx_queue_[(rx_head_ + rx_count_) % kNativeQueueSize] = frame;
    ++rx_count_;
}

}  // namespace pcd

#endif
