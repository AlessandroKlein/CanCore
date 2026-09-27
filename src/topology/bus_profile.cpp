#include "topology/bus_profile.h"

namespace pcd {

namespace {

const BusProfile kProfiles[] = {
    /* name                              topologia            bitrate  stub totalStub length term hub   rec */
    {"lineal-1m",                       BUS_TOPOLOGY_LINEAR, 1000000, 10,  100,       25,   2, false, true},
    {"lineal-500k",                     BUS_TOPOLOGY_LINEAR, 500000,  30,  300,      100,   2, false, true},
    {"lineal-250k",                     BUS_TOPOLOGY_LINEAR, 250000,  60,  600,      250,   2, false, true},
    {"lineal-125k",                     BUS_TOPOLOGY_LINEAR, 125000, 120, 1200,      500,   2, false, true},
    {"lineal-50k",                      BUS_TOPOLOGY_LINEAR,  50000, 300, 3000,     1000,   2, false, true},
    {"estrella-hub-125k",               BUS_TOPOLOGY_STAR,   125000,  60,  600,      100,   2, true,  true},
    {"estrella-hub-50k",                BUS_TOPOLOGY_STAR,    50000, 150, 1500,      250,   2, true,  true},
    {"arbol-hub-50k",                   BUS_TOPOLOGY_TREE,    50000, 150, 1500,      500,   2, true,  true},
    {"estrella-sin-terminacion-50k",    BUS_TOPOLOGY_STAR,    50000,  50,  500,      100,   0, false, false},
    {"arbol-sin-terminacion-50k",       BUS_TOPOLOGY_TREE,    50000,  50,  500,      100,   0, false, false}
};

const uint8_t kProfileCount = static_cast<uint8_t>(sizeof(kProfiles) / sizeof(kProfiles[0]));

}  // namespace

uint8_t busProfileCount() {
    return kProfileCount;
}

const BusProfile *busProfileAt(uint8_t index) {
    return index < kProfileCount ? &kProfiles[index] : 0;
}

const BusProfile *findBusProfile(BusTopology topology, uint32_t bitrate) {
    const BusProfile *fallback = 0;
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        const BusProfile &profile = kProfiles[i];
        if (profile.topology != topology) {
            continue;
        }
        if (profile.bitrate == bitrate && profile.recommended) {
            return &profile;
        }
        if (fallback == 0 && profile.bitrate == bitrate) {
            fallback = &profile;
        }
    }
    return fallback;
}

const BusProfile *recommendedProfile(BusTopology topology) {
    const BusProfile *best = 0;
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        const BusProfile &profile = kProfiles[i];
        if (profile.topology != topology || !profile.recommended) {
            continue;
        }
        if (best == 0 || profile.bitrate > best->bitrate) {
            best = &profile;
        }
    }
    return best;
}

bool topologySupports(BusTopology topology, uint32_t bitrate) {
    return findBusProfile(topology, bitrate) != 0;
}

uint16_t maxStubCm(BusTopology topology, uint32_t bitrate) {
    const BusProfile *profile = findBusProfile(topology, bitrate);
    return profile == 0 ? 0 : profile->max_stub_cm;
}

bool topologyRequiresHub(BusTopology topology) {
    const BusProfile *profile = findBusProfile(topology, 125000);
    if (profile != 0) {
        return profile->requires_hub;
    }
    profile = findBusProfile(topology, 50000);
    return profile == 0 ? false : profile->requires_hub;
}

const char *topologyName(BusTopology topology) {
    switch (topology) {
        case BUS_TOPOLOGY_LINEAR: return "lineal";
        case BUS_TOPOLOGY_STAR: return "estrella";
        case BUS_TOPOLOGY_TREE: return "arbol";
        default: return "desconocida";
    }
}

BusWiringIssue validateWiring(BusTopology topology, uint32_t bitrate, uint32_t stub_cm,
                              uint32_t total_stub_cm, uint32_t length_m, uint8_t terminations,
                              bool has_hub) {
    const BusProfile *profile = findBusProfile(topology, bitrate);
    if (profile == 0) {
        return BUS_WIRING_UNKNOWN_PROFILE;
    }
    if (profile->requires_hub && !has_hub) {
        return BUS_WIRING_MISSING_HUB;
    }
    if (terminations != profile->terminations) {
        return BUS_WIRING_BAD_TERMINATION;
    }
    if (stub_cm > profile->max_stub_cm) {
        return BUS_WIRING_STUB_TOO_LONG;
    }
    if (total_stub_cm > profile->max_total_stub_cm) {
        return BUS_WIRING_TOTAL_STUB_TOO_LONG;
    }
    if (length_m > profile->max_length_m) {
        return BUS_WIRING_TOO_LONG;
    }
    return BUS_WIRING_OK;
}

const char *wiringIssueName(BusWiringIssue issue) {
    switch (issue) {
        case BUS_WIRING_OK: return "ok";
        case BUS_WIRING_UNKNOWN_PROFILE: return "perfil desconocido";
        case BUS_WIRING_STUB_TOO_LONG: return "derivacion demasiado larga";
        case BUS_WIRING_TOTAL_STUB_TOO_LONG: return "suma de derivaciones excesiva";
        case BUS_WIRING_TOO_LONG: return "tramo demasiado largo";
        case BUS_WIRING_MISSING_HUB: return "falta el hub/star coupler";
        case BUS_WIRING_BAD_TERMINATION: return "terminacion incorrecta";
        default: return "desconocido";
    }
}

}  // namespace pcd
