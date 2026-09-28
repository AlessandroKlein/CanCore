#include "v6/pcd_errors.h"

namespace pcd {

const char *pcdErrorName(PCD_Error error) {
    switch (error) {
        case PCD_OK: return "OK";
        case PCD_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case PCD_ERR_INVALID_ID: return "INVALID_ID";
        case PCD_ERR_INVALID_PAYLOAD: return "INVALID_PAYLOAD";
        case PCD_ERR_TIMEOUT: return "TIMEOUT";
        case PCD_ERR_BUSY: return "BUSY";
        case PCD_ERR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case PCD_ERR_NO_MEMORY: return "NO_MEMORY";
        case PCD_ERR_CRC: return "CRC";
        case PCD_ERR_BUS_OFF: return "BUS_OFF";
        case PCD_ERR_OFFLINE: return "OFFLINE";
        case PCD_ERR_AUTH: return "AUTH";
        case PCD_ERR_VERSION: return "VERSION";
        case PCD_ERR_HARDWARE: return "HARDWARE";
        default: return "UNKNOWN";
    }
}

}  // namespace pcd
