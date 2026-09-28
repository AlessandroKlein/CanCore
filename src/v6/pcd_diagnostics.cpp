#include "v6/pcd_diagnostics.h"

namespace pcd {

const char *pcdCanStateName(PCD_CAN_State state) {
    switch (state) {
        case PCD_CAN_STATE_BUS_OK: return "BUS_OK";
        case PCD_CAN_STATE_ERROR_WARNING: return "ERROR_WARNING";
        case PCD_CAN_STATE_ERROR_PASSIVE: return "ERROR_PASSIVE";
        case PCD_CAN_STATE_BUS_OFF: return "BUS_OFF";
        case PCD_CAN_STATE_RECOVERING: return "RECOVERING";
        default: return "UNKNOWN";
    }
}

}  // namespace pcd
