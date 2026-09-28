#include "v6/pcd_network_stats.h"

namespace pcd {

uint8_t PCD_BusStatistics::utilizationPercent(uint32_t bitrate) const {
    /* Estimacion: 1 frame CAN ~ 130 bits con stuffing/overhead. */
    if (bitrate == 0 || framesPerSec == 0) {
        return 0;
    }
    const uint64_t bitsPerSec = static_cast<uint64_t>(framesPerSec) * 130u;
    const uint64_t util = (bitsPerSec * 100u) / bitrate;
    return util > 255u ? 255u : static_cast<uint8_t>(util);
}

PCD_RateLimiter::PCD_RateLimiter()
    : maxPerSecond_(0), burst_(0), tokens_(0), lastRefillMs_(0) {}

void PCD_RateLimiter::configure(uint16_t maxFramesPerSecond, uint16_t burstLimit) {
    maxPerSecond_ = maxFramesPerSecond;
    burst_ = burstLimit;
    tokens_ = burstLimit;
    lastRefillMs_ = 0;
}

bool PCD_RateLimiter::allow(uint32_t nowMs) {
    if (maxPerSecond_ == 0) {
        return true; /* sin limite */
    }
    const uint32_t elapsed = nowMs - lastRefillMs_;
    const uint16_t refill = static_cast<uint16_t>((elapsed * maxPerSecond_) / 1000u);
    if (refill > 0) {
        tokens_ = (tokens_ + refill > burst_) ? burst_ : static_cast<uint16_t>(tokens_ + refill);
        lastRefillMs_ = nowMs;
    }
    if (tokens_ > 0) {
        --tokens_;
        return true;
    }
    return false;
}

void PCD_RateLimiter::reset(uint32_t nowMs) {
    tokens_ = burst_;
    lastRefillMs_ = nowMs;
}

const char *pcdNetworkHealthName(PCD_NetworkHealth health) {
    switch (health) {
        case PCD_HEALTH_HEALTHY: return "HEALTHY";
        case PCD_HEALTH_DEGRADED: return "DEGRADED";
        case PCD_HEALTH_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

PCD_NetworkHealth pcdComputeNetworkHealth(uint8_t utilizationPercent, uint8_t offlineNodes,
                                          uint32_t errorRatePer1000) {
    if (utilizationPercent > 80 || offlineNodes > 10 || errorRatePer1000 > 50) {
        return PCD_HEALTH_CRITICAL;
    }
    if (utilizationPercent > 50 || offlineNodes > 0 || errorRatePer1000 > 10) {
        return PCD_HEALTH_DEGRADED;
    }
    return PCD_HEALTH_HEALTHY;
}

}  // namespace pcd
