/*
 * Pruebas de capa fisica por topologia:
 *
 *   - perfiles de bus (lineal, estrella, arbol) y validador de instalacion,
 *   - hub / star coupler multi-puerto (CanTreeHub) con anti-bucle y aprendizaje.
 *
 * El hub se prueba con bus virtual: cada ramal es un VirtualBus independiente y
 * el hub es el unico elemento que los une, exactamente como en el hardware real
 * (un transceptor por rama).
 */

#include <unity.h>

#include "hal/hal_can_native.h"
#include "topology/bus_profile.h"
#include "topology/can_tree_hub.h"

using namespace pcd;

void test_bus_profiles_lineal(void) {
    TEST_ASSERT_EQUAL_UINT8(10, busProfileCount());

    const BusProfile *profile = findBusProfile(BUS_TOPOLOGY_LINEAR, 500000);
    TEST_ASSERT_NOT_NULL(profile);
    TEST_ASSERT_EQUAL_UINT16(30, profile->max_stub_cm);
    TEST_ASSERT_EQUAL_UINT32(100, profile->max_length_m);
    TEST_ASSERT_EQUAL_UINT8(2, profile->terminations);
    TEST_ASSERT_FALSE(profile->requires_hub);
    TEST_ASSERT_TRUE(profile->recommended);
    TEST_ASSERT_EQUAL_STRING("lineal", topologyName(BUS_TOPOLOGY_LINEAR));

    TEST_ASSERT_EQUAL_UINT16(60, maxStubCm(BUS_TOPOLOGY_LINEAR, 250000));
    TEST_ASSERT_EQUAL_UINT16(0, maxStubCm(BUS_TOPOLOGY_LINEAR, 333333));
    TEST_ASSERT_FALSE(topologySupports(BUS_TOPOLOGY_LINEAR, 333333));
    TEST_ASSERT_FALSE(topologyRequiresHub(BUS_TOPOLOGY_LINEAR));
}

void test_bus_profiles_estrella_y_arbol(void) {
    const BusProfile *star = recommendedProfile(BUS_TOPOLOGY_STAR);
    TEST_ASSERT_NOT_NULL(star);
    TEST_ASSERT_EQUAL_UINT32(125000, star->bitrate);
    TEST_ASSERT_TRUE(star->requires_hub);
    TEST_ASSERT_EQUAL_UINT8(2, star->terminations);

    const BusProfile *tree = recommendedProfile(BUS_TOPOLOGY_TREE);
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT_EQUAL_UINT32(50000, tree->bitrate);
    TEST_ASSERT_EQUAL_UINT32(500, tree->max_length_m);
    TEST_ASSERT_TRUE(topologyRequiresHub(BUS_TOPOLOGY_TREE));
    TEST_ASSERT_EQUAL_STRING("arbol", topologyName(BUS_TOPOLOGY_TREE));
    TEST_ASSERT_EQUAL_STRING("estrella", topologyName(BUS_TOPOLOGY_STAR));
}

void test_wiring_validator(void) {
    /* Lineal 500 kbps correcto. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_OK,
                            validateWiring(BUS_TOPOLOGY_LINEAR, 500000, 30, 120, 90, 2, false));
    /* Derivacion larga. */
    TEST_ASSERT_EQUAL_UINT8(
        BUS_WIRING_STUB_TOO_LONG,
        validateWiring(BUS_TOPOLOGY_LINEAR, 500000, 200, 300, 90, 2, false));
    /* Suma de derivaciones excesiva. */
    TEST_ASSERT_EQUAL_UINT8(
        BUS_WIRING_TOTAL_STUB_TOO_LONG,
        validateWiring(BUS_TOPOLOGY_LINEAR, 500000, 30, 500, 90, 2, false));
    /* Tramo mas largo que el perfil. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_TOO_LONG,
                            validateWiring(BUS_TOPOLOGY_LINEAR, 500000, 30, 120, 250, 2, false));
    /* Terminacion ausente o de mas. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_BAD_TERMINATION,
                            validateWiring(BUS_TOPOLOGY_LINEAR, 500000, 30, 120, 90, 1, false));
    /* Estrella sin hub. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_MISSING_HUB,
                            validateWiring(BUS_TOPOLOGY_STAR, 125000, 60, 600, 100, 2, false));
    /* Estrella con hub y dos terminaciones por rama. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_OK,
                            validateWiring(BUS_TOPOLOGY_STAR, 125000, 60, 600, 100, 2, true));
    /* Combinacion sin perfil declarado. */
    TEST_ASSERT_EQUAL_UINT8(BUS_WIRING_UNKNOWN_PROFILE,
                            validateWiring(BUS_TOPOLOGY_STAR, 500000, 10, 20, 30, 2, true));
    TEST_ASSERT_EQUAL_STRING("ok", wiringIssueName(BUS_WIRING_OK));
}
void test_hub_forwards_broadcasts_between_branches(void) {
    VirtualBus backbone;
    VirtualBus branch_a;
    VirtualBus branch_b;

    NativeCanBus port_backbone(&backbone);
    NativeCanBus port_a(&branch_a);
    NativeCanBus port_b(&branch_b);
    NativeCanBus node_a(&branch_a);
    NativeCanBus node_b(&branch_b);
    NativeCanBus gateway(&backbone);

    CanTreeHub hub(125000);
    TEST_ASSERT_TRUE(hub.addPort(port_backbone, kBackboneSegment));
    TEST_ASSERT_TRUE(hub.addPort(port_a, 1));
    TEST_ASSERT_TRUE(hub.addPort(port_b, 2));
    TEST_ASSERT_TRUE(hub.begin());
    TEST_ASSERT_EQUAL_UINT8(3, hub.portCount());

    node_a.begin();
    node_b.begin();
    gateway.begin();

    /* El nodo del ramal A difunde su estado. */
    const CanFrame state = makeStateBroadcast(0x0116, RES_RELAY, 1, 1.0f);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_a.send(state));
    hub.poll(1000);

    /* Llega al gateway del tronco y al nodo del ramal B. */
    CanFrame received;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, gateway.receive(received));
    TEST_ASSERT_EQUAL_HEX32(state.id, received.id);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_b.receive(received));
    TEST_ASSERT_EQUAL_HEX32(state.id, received.id);

    /* El nodo de origen no recibe su propia trama de vuelta. */
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, node_a.receive(received));

    TEST_ASSERT_TRUE(hub.forwardedCount() >= 2);
    TEST_ASSERT_EQUAL_UINT32(0, hub.droppedCount());
}

void test_hub_drops_loops_and_duplicates(void) {
    VirtualBus bus_a;
    VirtualBus bus_b;
    NativeCanBus port_a(&bus_a);
    NativeCanBus port_b(&bus_b);

    CanTreeHub hub(125000);
    hub.addPort(port_a, 1);
    hub.addPort(port_b, 2);
    hub.begin();

    const CanFrame frame = makeStateBroadcast(0x0116, RES_RELAY, 1, 1.0f);
    TEST_ASSERT_TRUE(hub.handleFrame(0, frame, 100));
    /* La misma trama vuelve por un lazo de topologia dentro de la ventana. */
    TEST_ASSERT_FALSE(hub.handleFrame(1, frame, 110));
    TEST_ASSERT_EQUAL_UINT32(1, hub.loopSuppressed());
    TEST_ASSERT_EQUAL_UINT32(1, hub.droppedCount());
    TEST_ASSERT_EQUAL_UINT32(1, hub.port(1)->stats.dropped);

    /* Fuera de la ventana la trama vuelve a circular (retransmision legitima). */
    TEST_ASSERT_TRUE(hub.handleFrame(0, frame, 100 + kHubDefaultDedupeMs));
}

void test_hub_learns_and_routes_directed_commands(void) {
    VirtualBus backbone;
    VirtualBus branch_a;
    VirtualBus branch_b;

    NativeCanBus port_backbone(&backbone);
    NativeCanBus port_a(&branch_a);
    NativeCanBus port_b(&branch_b);
    NativeCanBus node_a(&branch_a);
    NativeCanBus node_b(&branch_b);
    NativeCanBus gateway(&backbone);

    CanTreeHub hub(125000);
    hub.addPort(port_backbone, kBackboneSegment);
    hub.addPort(port_a, 1);
    hub.addPort(port_b, 2);
    hub.begin();
    node_a.begin();
    node_b.begin();
    gateway.begin();
    TEST_ASSERT_EQUAL_UINT8(HUB_ROUTE_LEARNED, hub.routeMode());

    /* El nodo del ramal A se anuncia: el hub aprende direccion -> ramal. */
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_a.send(makeHeartbeat(0x0116, 10, HEALTH_OK)));
    hub.poll(2000);
    TEST_ASSERT_EQUAL_INT8(1, hub.learnedPort(0x16));

    /* Drena el broadcast del heartbeat que quedo en los ramales vecinos. */
    CanFrame drain;
    while (node_b.receive(drain) == CAN_OK) {
    }

    const uint32_t before_a = hub.port(1)->stats.forwarded;
    const uint32_t before_b = hub.port(2)->stats.forwarded;

    /* Comando dirigido desde el gateway al nodo del ramal A. */
    const CanFrame command = makeCommand(0x0001, 0x16, RES_RELAY, 1, ACT_ON);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, gateway.send(command));
    hub.poll(2100);

    TEST_ASSERT_EQUAL_UINT32(before_a + 1, hub.port(1)->stats.forwarded);
    TEST_ASSERT_EQUAL_UINT32(before_b, hub.port(2)->stats.forwarded);

    CanFrame received;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_a.receive(received));
    TEST_ASSERT_EQUAL_UINT8(ACT_ON, received.data[2]);
    TEST_ASSERT_EQUAL_UINT8(CAN_ERR_NO_DATA, node_b.receive(received));
}

void test_hub_in_flood_mode_reaches_every_branch(void) {
    VirtualBus backbone;
    VirtualBus branch_a;
    VirtualBus branch_b;
    NativeCanBus port_backbone(&backbone);
    NativeCanBus port_a(&branch_a);
    NativeCanBus port_b(&branch_b);
    NativeCanBus node_a(&branch_a);
    NativeCanBus node_b(&branch_b);
    NativeCanBus gateway(&backbone);

    CanTreeHub hub(50000);
    hub.addPort(port_backbone, kBackboneSegment);
    hub.addPort(port_a, 1);
    hub.addPort(port_b, 2);
    hub.setRouteMode(HUB_ROUTE_FLOOD);
    hub.begin();
    node_a.begin();
    node_b.begin();
    gateway.begin();

    const CanFrame command = makeCommand(0x0001, 0x16, RES_RELAY, 1, ACT_OFF);
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, gateway.send(command));
    hub.poll(3000);

    CanFrame received;
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_a.receive(received));
    TEST_ASSERT_EQUAL_UINT8(CAN_OK, node_b.receive(received));
    TEST_ASSERT_EQUAL_INT8(-1, hub.learnedPort(0x16));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_bus_profiles_lineal);
    RUN_TEST(test_bus_profiles_estrella_y_arbol);
    RUN_TEST(test_wiring_validator);
    RUN_TEST(test_hub_forwards_broadcasts_between_branches);
    RUN_TEST(test_hub_drops_loops_and_duplicates);
    RUN_TEST(test_hub_learns_and_routes_directed_commands);
    RUN_TEST(test_hub_in_flood_mode_reaches_every_branch);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}