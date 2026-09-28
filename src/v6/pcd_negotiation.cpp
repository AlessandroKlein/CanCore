#include "v6/pcd_negotiation.h"

namespace pcd {

bool pcdNegotiateProtocol(uint8_t aMajor, uint8_t aMinor, uint8_t bMajor, uint8_t bMinor,
                          uint8_t &outMajor, uint8_t &outMinor) {
    if (aMajor != bMajor) {
        return false; /* incompatibles: distinta major */
    }
    outMajor = aMajor;
    outMinor = (aMinor < bMinor) ? aMinor : bMinor;
    return true;
}

}  // namespace pcd
