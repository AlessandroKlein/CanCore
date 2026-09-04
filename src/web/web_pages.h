#pragma once

/* Recursos HTML agnosticos al servidor para un gateway PCD. */

#include <stdint.h>

namespace pcd {

enum WebView : uint8_t {
    WEB_VIEW_DASHBOARD = 0,
    WEB_VIEW_NODES,
    WEB_VIEW_ROUTES,
    WEB_VIEW_OTA,
    WEB_VIEW_TUNNEL,
    WEB_VIEW_DIAGNOSTICS,
    WEB_VIEW_SETTINGS,
    WEB_VIEW_COUNT
};

/* Aplicacion web completa: servirla en / y usar el hash para las vistas. */
const char *webAppHtml();

const char *webViewName(WebView view);
const char *webViewPath(WebView view);

}  // namespace pcd
