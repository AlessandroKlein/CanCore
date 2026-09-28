#include "v6/pcd_gateway.h"

namespace pcd {

PCD_Gateway::PCD_Gateway(PCD_CanRouter &router) : router_(router), extension_count_(0) {
    for (uint8_t i = 0; i < kMaxExtensions; ++i) {
        extensions_[i] = 0;
    }
}

bool PCD_Gateway::registerExtension(PCD_Extension *extension) {
    if (extension == 0 || extension_count_ >= kMaxExtensions) {
        return false;
    }
    extensions_[extension_count_++] = extension;
    return true;
}

PCD_Error PCD_Gateway::registerRoute(const PCD_RoutingRule &rule) {
    return router_.addRule(rule);
}

void PCD_Gateway::begin() {
    for (uint8_t i = 0; i < extension_count_; ++i) {
        if (extensions_[i] != 0) {
            extensions_[i]->begin();
        }
    }
}

void PCD_Gateway::poll() {
    for (uint8_t i = 0; i < extension_count_; ++i) {
        if (extensions_[i] != 0) {
            extensions_[i]->update();
        }
    }
}

}  // namespace pcd
