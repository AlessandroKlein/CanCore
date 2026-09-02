#include "system/system_api.h"

namespace pcd {

/* Puntero global a la estructura de servicios inyectada por la aplicacion.
 * Se mantiene como puntero (no copia) para que el usuario pueda actualizarla
 * en caliente si lo desea; en la practica se configura una vez en setup(). */

static const SystemApi *g_api = 0;

void setSystemApi(const SystemApi *api) {
    g_api = api;
}

const SystemApi *systemApi() {
    return g_api;
}

time_ms_t systemMillis() {
    const SystemApi *api = systemApi();
    return (api && api->millis) ? api->millis() : 0;
}

void systemDelay(uint32_t ms) {
    const SystemApi *api = systemApi();
    if (api && api->delay_ms) {
        api->delay_ms(ms);
    }
}

}  // namespace pcd
