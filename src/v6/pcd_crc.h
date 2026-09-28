#pragma once

/*
 * PCD v6 - CRC de aplicacion (independiente del CRC de la trama CAN).
 *
 * El controlador CAN ya garantiza integridad de la trama a nivel hardware
 * (CRC de 15 bits + ACK + stuffing). El CRC de APLICACION tiene otra funcion:
 * detectar corrupcion de datos que cruzan fronteras (firmware, configuracion
 * persistente, manifiestos, bloques OTA, registros Modbus). No sustituye al
 * mecanismo de error CAN; lo complementa donde el dato viaja mas alla del bus.
 *
 * Implementacion bit a bit (sin tablas) para minimizar Flash en AVR. Polinomios
 * estandar y vectores de prueba publicados (ver test/test_v6).
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

/* CRC-8/SMBUS: polinomio 0x07, init 0x00. Vector "123456789" -> 0xF4. */
uint8_t pcd_crc8(const uint8_t *data, size_t len, uint8_t crc = 0x00);

/* CRC-16/CCITT-FALSE: polinomio 0x1021, init 0xFFFF. "123456789" -> 0x29B1. */
uint16_t pcd_crc16(const uint8_t *data, size_t len, uint16_t crc = 0xFFFF);

/* CRC-32 (zlib): polinomio reflejado 0xEDB88320, init 0xFFFFFFFF, xorout
 * 0xFFFFFFFF. Vector "123456789" -> 0xCBF43926. */
uint32_t pcd_crc32(const uint8_t *data, size_t len, uint32_t crc = 0xFFFFFFFF);

}  // namespace pcd
