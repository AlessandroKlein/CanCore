#include "topology/can_tree_hub.h"

#include <string.h>

namespace pcd {

CanTreeHub::CanTreeHub(uint32_t bitrate)
    : port_count_(0),
      learn_next_(0),
      dedupe_next_(0),
      bitrate_(bitrate),
      dedupe_ms_(kHubDefaultDedupeMs),
      route_mode_(HUB_ROUTE_LEARNED),
      forwarded_(0),
      dropped_(0),
      loops_(0),
      started_(false) {
    clear();
}

CanTreeHub::~CanTreeHub() {}

void CanTreeHub::clear() {
    port_count_ = 0;
    forwarded_ = 0;
    dropped_ = 0;
    loops_ = 0;
    learn_next_ = 0;
    dedupe_next_ = 0;
    memset(buses_, 0, sizeof(buses_));
    memset(ports_, 0, sizeof(ports_));
    memset(learned_, 0, sizeof(learned_));
    memset(dedupe_, 0, sizeof(dedupe_));
}

bool CanTreeHub::addPort(ICanBus &bus, uint8_t segment) {
    if (port_count_ >= CAN_HUB_MAX_PORTS) {
        return false;
    }
    CanHubPort &entry = ports_[port_count_];
    entry.bus = &bus;
    entry.segment = segment;
    entry.stats.received = 0;
    entry.stats.forwarded = 0;
    entry.stats.dropped = 0;
    entry.stats.errors = 0;
    buses_[port_count_] = &bus;
    ++port_count_;
    return true;
}

bool CanTreeHub::begin(uint32_t bitrate) {
    const uint32_t rate = bitrate == 0 ? bitrate_ : bitrate;
    for (uint8_t i = 0; i < port_count_; ++i) {
        if (ports_[i].bus->begin(rate) != CAN_OK) {
            return false;
        }
        ports_[i].bus->setFilter(CanFilter::acceptAll());
    }
    started_ = port_count_ > 0;
    return started_;
}

void CanTreeHub::end() {
    for (uint8_t i = 0; i < port_count_; ++i) {
        ports_[i].bus->end();
    }
    started_ = false;
}

const CanHubPort *CanTreeHub::port(uint8_t index) const {
    return index < port_count_ ? &ports_[index] : 0;
}

ICanBus *CanTreeHub::portBus(uint8_t segment) const {
    for (uint8_t i = 0; i < port_count_; ++i) {
        if (ports_[i].segment == segment) {
            return ports_[i].bus;
        }
    }
    return 0;
}

uint32_t CanTreeHub::fingerprint(const CanFrame &frame) const {
    uint32_t hash = 2166136261UL;
    hash ^= frame.id;
    hash *= 16777619UL;
    hash ^= frame.dlc;
    hash *= 16777619UL;
    for (uint8_t i = 0; i < frame.dlc && i < kPayloadSize; ++i) {
        hash ^= frame.data[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool CanTreeHub::recentlySeen(uint32_t fingerprint_value, uint32_t now_ms) {
    for (uint8_t i = 0; i < CAN_HUB_DEDUPE_SLOTS; ++i) {
        DedupeEntry &entry = dedupe_[i];
        if (!entry.valid) {
            continue;
        }
        if ((now_ms - entry.seen_ms) >= dedupe_ms_) {
            entry.valid = false;
            continue;
        }
        if (entry.fingerprint == fingerprint_value) {
            return true;
        }
    }
    DedupeEntry &slot = dedupe_[dedupe_next_];
    slot.fingerprint = fingerprint_value;
    slot.seen_ms = now_ms;
    slot.valid = true;
    dedupe_next_ = static_cast<uint8_t>((dedupe_next_ + 1) % CAN_HUB_DEDUPE_SLOTS);
    return false;
}

void CanTreeHub::learn(uint16_t node_id, uint8_t port) {
    const uint8_t address = nodeIdAddress(node_id);
    if (address == 0) {
        return;
    }
    for (uint8_t i = 0; i < CAN_HUB_LEARN_SLOTS; ++i) {
        LearnEntry &entry = learned_[i];
        if (entry.valid && entry.address == address) {
            entry.port = port;
            return;
        }
    }
    LearnEntry &slot = learned_[learn_next_];
    slot.address = address;
    slot.port = port;
    slot.seen_ms = 0;
    slot.valid = true;
    learn_next_ = static_cast<uint8_t>((learn_next_ + 1) % CAN_HUB_LEARN_SLOTS);
}

void CanTreeHub::expireLearning(uint32_t now_ms) {
    for (uint8_t i = 0; i < CAN_HUB_LEARN_SLOTS; ++i) {
        LearnEntry &entry = learned_[i];
        if (entry.valid && entry.seen_ms != 0 && (now_ms - entry.seen_ms) >= kHubLearnHoldMs) {
            entry.valid = false;
        }
    }
}

int8_t CanTreeHub::learnedPort(uint8_t address) const {
    for (uint8_t i = 0; i < CAN_HUB_LEARN_SLOTS; ++i) {
        const LearnEntry &entry = learned_[i];
        if (entry.valid && entry.address == address) {
            return static_cast<int8_t>(entry.port);
        }
    }
    return kUnknownPort;
}

void CanTreeHub::forward(uint8_t origin_port, const CanFrame &frame, const CanId &id) {
    const bool directed = id.target != kBroadcastTarget;
    int8_t destination = kUnknownPort;
    if (route_mode_ == HUB_ROUTE_LEARNED && directed) {
        destination = learnedPort(id.target);
    }
    for (uint8_t p = 0; p < port_count_; ++p) {
        if (p == origin_port || ports_[p].bus == 0) {
            continue;
        }
        if (destination != kUnknownPort && destination != static_cast<int8_t>(p)) {
            continue;
        }
        if (ports_[p].bus->send(frame) == CAN_OK) {
            ++forwarded_;
            ++ports_[p].stats.forwarded;
        } else {
            ++ports_[p].stats.errors;
        }
    }
}

bool CanTreeHub::handleFrame(uint8_t origin_port, const CanFrame &frame, uint32_t now_ms) {
    if (origin_port >= port_count_) {
        return false;
    }
    const uint32_t fingerprint_value = fingerprint(frame);
    if (recentlySeen(fingerprint_value, now_ms)) {
        /* La misma trama ya circulo por el hub dentro de la ventana: lazo de
         * topologia o retransmision identica. No se reenvia. */
        ++dropped_;
        ++loops_;
        ++ports_[origin_port].stats.dropped;
        return false;
    }

    const CanId id = frame.fields();
    if (isValidSource(id.source)) {
        learn(id.source, origin_port);
        const uint8_t address = nodeIdAddress(id.source);
        for (uint8_t i = 0; i < CAN_HUB_LEARN_SLOTS; ++i) {
            if (learned_[i].valid && learned_[i].address == address) {
                learned_[i].seen_ms = now_ms;
            }
        }
    }
    forward(origin_port, frame, id);
    return true;
}

void CanTreeHub::poll(uint32_t now_ms) {
    if (!started_) {
        return;
    }
    for (uint8_t p = 0; p < port_count_; ++p) {
        ICanBus *bus = ports_[p].bus;
        if (bus == 0) {
            continue;
        }
        if (bus->isBusOff()) {
            bus->recover();
            ++ports_[p].stats.errors;
        }
        CanFrame frame;
        while (bus->receive(frame) == CAN_OK) {
            ++ports_[p].stats.received;
            handleFrame(p, frame, now_ms);
        }
    }
    expireLearning(now_ms);
}

}  // namespace pcd
