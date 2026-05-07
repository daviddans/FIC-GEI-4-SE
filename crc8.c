/*
 * crc8.c — CRC-8/CCITT (POLY=0x07, INIT=0x00) "naive".
 *
 * Cumpre o requisito do enunciado: o bucle interno usa **só desprazamentos
 * e operadores lóxicos** (sen `if`). A condición sobre o MSB exprésase como
 * unha máscara aritmética de bits, idéntica conceptualmente ao truco que
 * aplica a versión ASM en hardware co carry flag.
 *
 * Esta mesma fonte compílase dúas veces polo makefile (con -O0 e con -Ofast)
 * usando -DCRC8_FUNC=crc8_O0 e -DCRC8_FUNC=crc8_Ofast respectivamente, para
 * obter dous obxectos con nomes distintos no mesmo executable.
 */

#include <stdint.h>
#include <stddef.h>

#define POLY 0x07u

#ifndef CRC8_FUNC
#define CRC8_FUNC crc8        /* nome por defecto se non se define o macro */
#endif

uint8_t CRC8_FUNC(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (unsigned b = 0; b < 8; b++) {
            /* mask = 0xFF se o MSB era 1, 0x00 se era 0 (sen ramas). */
            uint8_t mask = (uint8_t)(-(crc >> 7));
            crc = (uint8_t)((crc << 1) ^ (POLY & mask));
        }
    }
    return crc;
}
