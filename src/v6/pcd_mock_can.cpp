#include "v6/pcd_mock_can.h"

#include <string.h>

namespace pcd {

PCD_MockNetwork::PCD_MockNetwork() : node_count_(0) {
    for (size_t i = 0; i < kMaxNodes; ++i) {
        nodes_[i] = 0;
    }
}

void PCD_MockNetwork::attach(PCD_MockCAN *node) {
    if (node == 0 || node_count_ >= kMaxNodes) {
        return;
    }
    nodes_[node_count_++] = node;
}

void PCD_MockNetwork::detach(PCD_MockCAN *node) {
    for (size_t i = 0; i < node_count_; ++i) {
        if (nodes_[i] == node) {
            nodes_[i] = nodes_[node_count_ - 1];
            nodes_[node_count_ - 1] = 0;
            --node_count_;
            return;
        }
    }
}

void PCD_MockNetwork::deliver(const CanFrame &frame, PCD_MockCAN *sender, uint32_t base_ms) {
    for (size_t i = 0; i < node_count_; ++i) {
        if (nodes_[i] != 0 && nodes_[i] != sender) {
            nodes_[i]->deliverFromNetwork(frame, base_ms);
        }
    }
}

PCD_MockCAN::PCD_MockCAN(PCD_MockNetwork *network)
    : net_(network), started_(false), bus_off_(false), now_ms_(0), rng_(1), rx_count_(0) {
    if (net_ != 0) {
        net_->attach(this);
    }
    for (size_t i = 0; i < kRxQueue; ++i) {
        rx_[i].ready = false;
        rx_[i].deliver_ms = 0;
    }
}

CanStatus PCD_MockCAN::begin(uint32_t bitrate) {
    (void)bitrate;
    started_ = true;
    bus_off_ = false;
    rng_ = faults_.seed == 0 ? 1u : faults_.seed;
    return CAN_OK;
}

void PCD_MockCAN::end() {
    started_ = false;
    rx_count_ = 0;
}

CanStatus PCD_MockCAN::send(const CanFrame &frame, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    if (bus_off_) {
        return CAN_ERR_BUS_OFF;
    }
    ++diag_.txCount;

    if (shouldDrop()) {
        return CAN_OK; /* perdida de paquete simulada */
    }

    const uint32_t base_ms = now_ms_;
    if (net_ != 0) {
        net_->deliver(frame, this, base_ms);
    } else {
        enqueue(frame, base_ms + faults_.delayMs); /* loopback aplica su latencia */
    }

    if (shouldDuplicate()) {
        if (net_ != 0) {
            net_->deliver(frame, this, base_ms);
        } else {
            enqueue(frame, base_ms + faults_.delayMs);
        }
    }
    return CAN_OK;
}

CanStatus PCD_MockCAN::receive(CanFrame &frame, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!started_) {
        return CAN_ERR_NOT_STARTED;
    }
    if (bus_off_) {
        return CAN_ERR_BUS_OFF;
    }
    for (size_t i = 0; i < rx_count_; ++i) {
        if (rx_[i].ready) {
            frame = rx_[i].frame;
            /* compacta la cola */
            for (size_t j = i + 1; j < rx_count_; ++j) {
                rx_[j - 1] = rx_[j];
            }
            rx_[rx_count_ - 1].ready = false;
            --rx_count_;
            ++diag_.rxCount;
            return CAN_OK;
        }
    }
    return CAN_ERR_NO_DATA;
}

void PCD_MockCAN::setFilter(const CanFilter &filter) {
    filter_ = filter;
}

bool PCD_MockCAN::isBusOff() const {
    return bus_off_;
}

CanStatus PCD_MockCAN::recover() {
    if (!bus_off_) {
        return CAN_OK;
    }
    bus_off_ = false;
    ++diag_.busOffCount;
    return CAN_OK;
}

void PCD_MockCAN::tick(uint32_t now_ms) {
    now_ms_ = now_ms;
    for (size_t i = 0; i < rx_count_; ++i) {
        if (!rx_[i].ready && now_ms_ >= rx_[i].deliver_ms) {
            rx_[i].ready = true;
        }
    }
}

void PCD_MockCAN::injectRx(const CanFrame &frame) {
    enqueue(frame, now_ms_);
}

void PCD_MockCAN::deliverFromNetwork(const CanFrame &frame, uint32_t base_ms) {
    if (!started_ || bus_off_) {
        return;
    }
    if (shouldDrop()) {
        return;
    }
    const uint32_t deliver_ms = base_ms + faults_.delayMs; /* latencia del receptor */
    CanFrame f = frame;
    if (shouldCorrupt() && f.dlc > 0) {
        f.data[0] = static_cast<uint8_t>(f.data[0] ^ 0x80u);
    }
    enqueue(f, deliver_ms);
    if (shouldDuplicate()) {
        enqueue(f, deliver_ms);
    }
}

void PCD_MockCAN::clearRx() {
    for (size_t i = 0; i < rx_count_; ++i) {
        rx_[i].ready = false;
    }
    rx_count_ = 0;
}

uint32_t PCD_MockCAN::nextRandom() {
    uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;
    return x;
}

float PCD_MockCAN::randomFloat() {
    return static_cast<float>(nextRandom() & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

bool PCD_MockCAN::shouldDrop() {
    return faults_.dropProbability > 0.0f && randomFloat() < faults_.dropProbability;
}

bool PCD_MockCAN::shouldDuplicate() {
    return faults_.duplicateProbability > 0.0f && randomFloat() < faults_.duplicateProbability;
}

bool PCD_MockCAN::shouldCorrupt() {
    return faults_.crcErrorProbability > 0.0f && randomFloat() < faults_.crcErrorProbability;
}

void PCD_MockCAN::enqueue(const CanFrame &frame, uint32_t deliver_ms) {
    if (filter_.mask != 0 && !filter_.accepts(frame.id)) {
        return;
    }
    if (rx_count_ >= kRxQueue) {
        ++diag_.droppedFrames;
        return;
    }
    RxEntry &entry = rx_[rx_count_++];
    entry.frame = frame;
    entry.deliver_ms = deliver_ms;
    entry.ready = (now_ms_ >= deliver_ms);
}

}  // namespace pcd
