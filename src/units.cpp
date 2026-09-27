#include "units.h"

#include <string.h>

namespace pcd {

namespace {

const UnitInfo kUnits[] = {
    {"", "sin unidad", 1.0f, 0.0f},
    {"C", "grados Celsius", 1.0f, 273.15f},
    {"K", "kelvin", 1.0f, 0.0f},
    {"%", "porcentaje", 1.0f, 0.0f},
    {"V", "voltios", 1.0f, 0.0f},
    {"A", "amperios", 1.0f, 0.0f},
    {"W", "vatios", 1.0f, 0.0f},
    {"Wh", "vatios hora", 3600.0f, 0.0f},
    {"Pa", "pascales", 1.0f, 0.0f},
    {"bar", "bares", 100000.0f, 0.0f},
    {"lx", "lux", 1.0f, 0.0f},
    {"ppm", "partes por millon", 1.0f, 0.0f},
    {"Hz", "hercios", 1.0f, 0.0f},
    {"m", "metros", 1.0f, 0.0f},
    {"m/s", "metros por segundo", 1.0f, 0.0f},
    {"deg", "grados sexagesimales", 1.0f, 0.0f},
    {"s", "segundos", 1.0f, 0.0f},
    {"kg", "kilogramos", 1.0f, 0.0f},
    {"l/min", "litros por minuto", 0.0000166667f, 0.0f}
};

const uint8_t kUnitCount = static_cast<uint8_t>(sizeof(kUnits) / sizeof(kUnits[0]));

const ScaleProfile kProfiles[SCALE_COUNT] = {
    /* name                    unit              scale            offset  dec */
    {"raw",                    UNIT_NONE,        1.0f,            0.0f,   0},
    {"canopen-temperature",    UNIT_CELSIUS,     0.1f,            0.0f,   1},
    {"canopen-voltage",        UNIT_VOLT,        0.01f,           0.0f,   2},
    {"canopen-current",        UNIT_AMPERE,      0.001f,          0.0f,   3},
    {"canopen-pressure",       UNIT_PASCAL,      1.0f,            0.0f,   0},
    {"canopen-percent",        UNIT_PERCENT,     0.1f,            0.0f,   1},
    {"nmea2000-temperature",   UNIT_KELVIN,      0.01f,           0.0f,   2},
    {"nmea2000-voltage",       UNIT_VOLT,        0.01f,           0.0f,   2},
    {"nmea2000-current",       UNIT_AMPERE,      0.1f,            0.0f,   1},
    {"nmea2000-pressure",      UNIT_PASCAL,      100.0f,          0.0f,   0},
    {"knx-dpt9",               UNIT_NONE,        0.01f,           0.0f,   2},
    {"knx-dpt5",               UNIT_PERCENT,     100.0f / 255.0f, 0.0f,   1}
};

}  // namespace

const UnitInfo *unitInfo(UnitId unit) {
    /* La tabla sigue el orden del enum UnitId, de modo que el indice es la
     * propia unidad y la busqueda es O(1). */
    if (static_cast<uint8_t>(unit) >= kUnitCount) {
        return 0;
    }
    return &kUnits[static_cast<uint8_t>(unit)];
}

const char *unitSymbol(UnitId unit) {
    const UnitInfo *info = unitInfo(unit);
    return info == 0 ? "" : info->symbol;
}

const char *unitNameEs(UnitId unit) {
    const UnitInfo *info = unitInfo(unit);
    return info == 0 ? "desconocida" : info->name;
}

bool unitToSi(UnitId unit, float value, float &si_out) {
    const UnitInfo *info = unitInfo(unit);
    if (info == 0) {
        return false;
    }
    si_out = value * info->to_si + info->si_offset;
    return true;
}

bool unitFromSi(UnitId unit, float si_value, float &value_out) {
    const UnitInfo *info = unitInfo(unit);
    if (info == 0 || info->to_si == 0.0f) {
        return false;
    }
    value_out = (si_value - info->si_offset) / info->to_si;
    return true;
}

const ScaleProfile *scaleProfile(ScaleProfileId id) {
    return id < SCALE_COUNT ? &kProfiles[id] : 0;
}

const ScaleProfile *scaleProfileByName(const char *name) {
    if (name == 0) {
        return 0;
    }
    for (uint8_t i = 0; i < SCALE_COUNT; ++i) {
        if (strcmp(kProfiles[i].name, name) == 0) {
            return &kProfiles[i];
        }
    }
    return 0;
}

float rawToEngineering(const ScaleProfile &profile, int32_t raw) {
    return static_cast<float>(raw) * profile.scale + profile.offset;
}

int32_t engineeringToRaw(const ScaleProfile &profile, float value) {
    if (profile.scale == 0.0f) {
        return 0;
    }
    const float scaled = (value - profile.offset) / profile.scale;
    return static_cast<int32_t>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

bool engineeringToRaw16(const ScaleProfile &profile, float value, int16_t &raw_out) {
    const int32_t raw = engineeringToRaw(profile, value);
    if (raw > 32767 || raw < -32768) {
        return false;
    }
    raw_out = static_cast<int16_t>(raw);
    return true;
}

float raw16ToEngineering(const ScaleProfile &profile, int16_t raw) {
    return rawToEngineering(profile, static_cast<int32_t>(raw));
}

}  // namespace pcd
