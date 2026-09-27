#pragma once

/*
 * Unidades y perfiles de escala.
 *
 * El protocolo PCD transporta siempre el valor de ingenieria (temperatura en
 * grados Celsius, potencia en vatios...). Los buses externos transportan
 * enteros con escalas distintas:
 *
 *   CANopen     temperatura 0,1 C     presion 1 Pa      tension 0,01 V
 *   NMEA2000    temperatura 0,01 K    presion 100 Pa    tension 0,01 V
 *   KNX DPT 9   float 0,01            DPT 5  0..100 %  (raw 0..255)
 *
 * Este modulo concentra esas tablas para que cada puente no invente su propia
 * constante y para poder validar los perfiles con pruebas automaticas. Los
 * perfiles de fabricante que no sigan una escala publica se agregan aqui con su
 * nombre y su documentacion (ver wiki 12).
 */

#include <stdint.h>

namespace pcd {

enum UnitId : uint8_t {
    UNIT_NONE = 0,
    UNIT_CELSIUS,
    UNIT_KELVIN,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_WATT_HOUR,
    UNIT_PASCAL,
    UNIT_BAR,
    UNIT_LUX,
    UNIT_PPM,
    UNIT_HERTZ,
    UNIT_METER,
    UNIT_METER_PER_SECOND,
    UNIT_DEGREE,
    UNIT_SECOND,
    UNIT_KILOGRAM,
    UNIT_LITER_PER_MINUTE
};

/* Factor y desplazamiento hacia la unidad base del SI. */
struct UnitInfo {
    const char *symbol;
    const char *name;
    float to_si;     /* si = value * to_si + si_offset */
    float si_offset;
};

const UnitInfo *unitInfo(UnitId unit);
const char *unitSymbol(UnitId unit);
const char *unitNameEs(UnitId unit);

/* value (unidad) -> SI y SI -> value. */
bool unitToSi(UnitId unit, float value, float &si_out);
bool unitFromSi(UnitId unit, float si_value, float &value_out);

/* ------------------------------------------------------------------ */
/* Perfiles de escala de bus externo                                    */
/* ------------------------------------------------------------------ */

enum ScaleProfileId : uint8_t {
    SCALE_RAW = 0,               /* 1:1                              */
    SCALE_CANOPEN_TEMPERATURE,   /* 0,1 C        (0x2300)            */
    SCALE_CANOPEN_VOLTAGE,       /* 0,01 V       (0x2000)            */
    SCALE_CANOPEN_CURRENT,       /* 0,001 A      (0x2200)            */
    SCALE_CANOPEN_PRESSURE,      /* 1 Pa                             */
    SCALE_CANOPEN_PERCENT,       /* 0,1 %                            */
    SCALE_NMEA2000_TEMPERATURE,  /* 0,01 K                           */
    SCALE_NMEA2000_VOLTAGE,      /* 0,01 V                           */
    SCALE_NMEA2000_CURRENT,      /* 0,1 A                            */
    SCALE_NMEA2000_PRESSURE,     /* 100 Pa                           */
    SCALE_KNX_DPT9,              /* 0,01                             */
    SCALE_KNX_DPT5,              /* (100 / 255) %                    */
    SCALE_COUNT
};

struct ScaleProfile {
    const char *name;
    UnitId unit;
    float scale;   /* valor = raw * scale + offset */
    float offset;
    uint8_t decimals;
};

const ScaleProfile *scaleProfile(ScaleProfileId id);
const ScaleProfile *scaleProfileByName(const char *name);

/* Entero del bus externo -> valor de ingenieria. */
float rawToEngineering(const ScaleProfile &profile, int32_t raw);

/* Valor de ingenieria -> entero del bus externo (redondeo al mas cercano). */
int32_t engineeringToRaw(const ScaleProfile &profile, float value);

/* Variante compacta de 16 bits con signo (CAN clasico / PDO de 2 bytes). */
bool engineeringToRaw16(const ScaleProfile &profile, float value, int16_t &raw_out);
float raw16ToEngineering(const ScaleProfile &profile, int16_t raw);

}  // namespace pcd
