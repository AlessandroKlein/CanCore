/*
 * Hub repetidor / star coupler para topologia en estrella o arbol.
 *
 * Un microcontrolador con un transceptor CAN por ramal. Cada ramal es un bus
 * lineal propio terminado con 120 ohm en sus dos extremos; el hub los une
 * reenviando tramas entre ramales (con anti-bucle por huella de trama) y,
 * en modo LEARNED, enrutando los comandos dirigidos solo hacia la rama que
 * aloja al nodo destino.
 *
 *   [rama 0: backbone/gateway] ---.
 *   [rama 1: dispositivos]     ---+--- HUB ---
 *   [rama 2: dispositivos]     ---'
 *
 * Valida el cableado antes de energizar con `validateWiring(...)` segun el
 * perfil de `bus_profile.h` y luego entra en el bucle de reenvio.
 */

#include <PCD_CAN.h>

namespace {

/* Velocidad conservadora para estrella/arbol (ver recommendedProfile). */
const uint32_t kHubBitrate = 125000UL;

#if defined(ARDUINO_ARCH_ESP32)
/* ESP32 integra un unico controlador TWAI. Para mas de una rama se agregan
 * transceptores MCP2515 por SPI (consultar hal/hal_can_mcp2515.h, hoy AVR). */
const int kBackboneTx = 5;
const int kBackboneRx = 4;
pcd::Esp32TwaiBus g_backbone(kBackboneTx, kBackboneRx);
pcd::CanTreeHub g_hub(kHubBitrate);
#elif defined(__AVR__)
/* AVR: un MCP2515 por ramal, cada uno con su CS propio en el bus SPI. */
pcd::Mcp2515Bus g_backbone(10, pcd::MCP_CLOCK_16MHZ);
pcd::Mcp2515Bus g_branch_a(9, pcd::MCP_CLOCK_16MHZ);
pcd::Mcp2515Bus g_branch_b(8, pcd::MCP_CLOCK_16MHZ);
pcd::CanTreeHub g_hub(kHubBitrate);
#else
/* Entorno de escritorio: sin hardware, solo estructura de referencia. */
pcd::CanTreeHub g_hub(kHubBitrate);
#endif

void reportWiring() {
    /* Estrella: ramas de hasta 60 cm de derivacion y 100 m, con hub. */
    const pcd::BusWiringIssue issue =
        pcd::validateWiring(pcd::BUS_TOPOLOGY_STAR, kHubBitrate, 60, 600, 100, 2, true);
    if (issue != pcd::BUS_WIRING_OK) {
        /* LOG_ERROR(pcd::wiringIssueName(issue)); */
    }
}

}  // namespace

void setup() {
    reportWiring();

#if defined(ARDUINO_ARCH_ESP32)
    g_hub.addPort(g_backbone, pcd::kBackboneSegment);
    /* g_hub.addPort(mcp_branch_a, 1); ... */
#elif defined(__AVR__)
    g_hub.addPort(g_backbone, pcd::kBackboneSegment);
    g_hub.addPort(g_branch_a, 1);
    g_hub.addPort(g_branch_b, 2);
#endif

    g_hub.setRouteMode(pcd::HUB_ROUTE_LEARNED);
    if (!g_hub.begin(kHubBitrate)) {
        /* LOG_ERROR("No se pudo inicializar algun ramal CAN"); */
    }
}

void loop() {
    g_hub.poll(millis());
}
