#include "v6/pcd_event.h"

namespace pcd {

PCD_EventStore::PCD_EventStore() : head_(0), count_(0) {}

void PCD_EventStore::push(const PCD_Event &event) {
    events_[head_] = event;
    head_ = static_cast<uint16_t>((head_ + 1) % kCapacity);
    if (count_ < kCapacity) {
        ++count_;
    }
}

const PCD_Event &PCD_EventStore::at(uint16_t index) const {
    /* index 0 = mas antiguo. */
    const uint16_t oldest = (count_ < kCapacity) ? 0 : head_;
    return events_[(oldest + index) % kCapacity];
}

void PCD_EventStore::clear() {
    head_ = 0;
    count_ = 0;
}

}  // namespace pcd
