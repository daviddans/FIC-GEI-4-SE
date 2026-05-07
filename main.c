/*
 * main.c — Benchmark CRC-8 sobre FRDM-KL46Z (Cortex-M0+).
 *
 * Compara tres implementacións do mesmo algoritmo CRC-8/CCITT:
 *   - crc8_O0    : C branchless compilado con -O0
 *   - crc8_Ofast : a mesma fonte recompilada con -Ofast
 *   - crc8_asm   : versión propia en ensamblador ARMv6-M
 *
 * Mide o número de ciclos de CPU por chamada usando o TPM0 alimentado
 * por MCGFLLCLK con prescaler 1, polo que **1 tick = 1 ciclo de CPU**.
 * Como o sistema é bare-metal e deterministico, basta unha **única
 * chamada** (single-shot) por implementación. O TPM é de 16 bit, polo
 * que `CRC8_BUF_LEN` debe escollerse para que -O0 non supere 65535
 * ciclos; en caso contrario o flag TOF dispárase e o resultado márcase
 * con `[!]`.
 *
 * A saída ordenada imprímese por UART0 a 115200 baud (PTA2 = TX).
 */

#include "includes/MKL46Z4.h"
#include <stdint.h>
#include <stddef.h>

/* Tamaño do buffer.*/
#define CRC8_BUF_LEN 128u

extern uint8_t crc8_O0   (const uint8_t*, size_t);
extern uint8_t crc8_Ofast(const uint8_t*, size_t);
extern uint8_t crc8_asm  (const uint8_t*, size_t);

/* --- UART0 bare-metal a 115200 baud (FLL ~20.97 MHz, OSR=14, SBR=13) --- */

static void uart0_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    SIM->SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_UART0SRC_MASK) | SIM_SOPT2_UART0SRC(1);
    PORTA->PCR[1] = PORT_PCR_MUX(2);   /* PTA1 = UART0_RX */
    PORTA->PCR[2] = PORT_PCR_MUX(2);   /* PTA2 = UART0_TX */
    UART0->C2  = 0;
    UART0->C4  = 0x0D;                 /* OSR = 14 */
    UART0->BDH = 0x00;
    UART0->BDL = 13;                   /* SBR = 13 → ~115228 baud */
    UART0->C2  = UART0_C2_TE_MASK | UART0_C2_RE_MASK;
}

static void uart0_putchar(char c)
{
    while (!(UART0->S1 & UART0_S1_TDRE_MASK));
    UART0->D = (uint8_t)c;
}

static void uart0_puts(const char *s) { while (*s) uart0_putchar(*s++); }

static void uart0_putuint(uint32_t v)
{
    char buf_dec[11];                  /* dabondo para uint32_t en decimal + '\0' */
    int i = 10;
    buf_dec[i] = '\0';
    if (v == 0) { uart0_putchar('0'); return; }
    while (v) { buf_dec[--i] = '0' + (v % 10); v /= 10; }
    uart0_puts(&buf_dec[i]);
}

/* "0xXX" — 2 díxitos hexadecimais (un byte). */
static void uart0_puthex2(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart0_putchar('0'); uart0_putchar('x');
    uart0_putchar(hex[(v >> 4) & 0xF]);
    uart0_putchar(hex[v & 0xF]);
}

/* "X.YYx" — ratio num/den con dous decimais fixos. */
static void uart0_putfix2x(uint32_t num, uint32_t den)
{
    if (den == 0) { uart0_puts("inf"); return; }
    uint32_t r = (num * 100u) / den;
    uart0_putuint(r / 100);
    uart0_putchar('.');
    uart0_putchar('0' + ((r / 10) % 10));
    uart0_putchar('0' + (r % 10));
    uart0_putchar('x');
}

/* --- TPM0 @ MCGFLLCLK, prescaler 1 ---
 * Configurámolo como contador libre de 16 bit (MOD=0xFFFF). CMOD=01 fai
 * que CNT incremente con cada flanco do reloxo do módulo; PS=000 = presc 1.
 * Resultado: 1 tick = 1 ciclo de CPU, sen necesidade de conversión. */
static void tpm0_init(void)
{
    SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_TPMSRC_MASK)
                | SIM_SOPT2_TPMSRC(1);              /* fonte = MCGFLLCLK */
    TPM0->SC  = 0;                                  /* parar mentres se configura */
    TPM0->MOD = 0xFFFFu;                            /* período máx (16 bit) */
    TPM0->CNT = 0;
    TPM0->SC  = TPM_SC_CMOD(1);                     /* arrancar; CMOD=01, PS=000 */
}

/* Buffer de proba. O contido é indiferente — só importa que as 3 versións
 * operen sobre os mesmos datos. Inicialízase con buf[i]=i en main(). */
static uint8_t buf[CRC8_BUF_LEN];

typedef struct {
    const char *name;
    uint8_t     crc;
    uint32_t    cycles;       /* ciclos CPU (single-shot, 1 tick = 1 ciclo) */
    int         overflow;     /* TPM->TOF detectado: medición non fiable */
} result_t;

/* Realiza UNHA medición single-shot da función fn(buf, CRC8_BUF_LEN). */
static void measure(result_t *r, uint8_t (*fn)(const uint8_t*, size_t),
                    const char *name)
{
    TPM0->SC |= TPM_SC_TOF_MASK;                    /* limpa TOF (w1c) */
    uint16_t t0  = (uint16_t)TPM0->CNT;
    uint8_t  crc = fn(buf, CRC8_BUF_LEN);
    uint16_t t1  = (uint16_t)TPM0->CNT;
    r->name     = name;
    r->crc      = crc;
    r->cycles   = (uint32_t)(uint16_t)(t1 - t0);    /* resta módulo 16-bit */
    r->overflow = (TPM0->SC & TPM_SC_TOF_MASK) != 0;
}

/* Ordena os 3 resultados por ciclos ascendentes (mais rapido primeiro). */
static void sort3_results(result_t *r)
{
    result_t t;
    if (r[0].cycles > r[1].cycles) { t = r[0]; r[0] = r[1]; r[1] = t; }
    if (r[1].cycles > r[2].cycles) { t = r[1]; r[1] = r[2]; r[2] = t; }
    if (r[0].cycles > r[1].cycles) { t = r[0]; r[0] = r[1]; r[1] = t; }
}

int main(void)
{
    /* --- Inicialización --- */
    SIM->COPC = 0;                     /* desactivar watchdog */
    uart0_init();
    tpm0_init();

    for (unsigned i = 0; i < CRC8_BUF_LEN; i++) buf[i] = (uint8_t)i;

    /* --- Mediciones single-shot ---
     * Tomamos crc8_O0 como referencia para a comprobación de igualdade. */
    result_t r[3];
    measure(&r[0], crc8_O0,    "C -O0   ");
    uint8_t expected = r[0].crc;
    measure(&r[1], crc8_Ofast, "C -Ofast");
    measure(&r[2], crc8_asm,   "ASM     ");
    sort3_results(r);

    /* --- Cabeceira --- */
    uart0_puts("\r\n================ CRC-8 Benchmark ================\r\n");
    uart0_puts("Algoritmo   : CRC-8/CCITT (POLY=0x07, INIT=0x00)\r\n");
    uart0_puts("Datos       : buf[i] = i, len = ");
    uart0_putuint(CRC8_BUF_LEN); uart0_puts(" B\r\n");
    uart0_puts("Temporizador: TPM0 @ MCGFLLCLK, presc 1 (1 tick = 1 ciclo CPU)\r\n");
    uart0_puts("Medición    : single-shot\r\n");
    uart0_puts("CRC esperado: "); uart0_puthex2(expected); uart0_puts("\r\n");

    /* --- Táboa ordenada --- */
    uart0_puts("\r\nResultados (mais rapido primeiro):\r\n");
    uart0_puts("  Versión    CRC    OK    ciclos     speedup vs peor\r\n");

    int any_fail = 0, any_ovf = 0;
    uint32_t worst = r[2].cycles;     /* tras sort, r[2] é o mais lento */

    for (int i = 0; i < 3; i++) {
        uart0_puts("  ");
        uart0_puts(r[i].name);
        uart0_puts("   ");
        uart0_puthex2(r[i].crc);
        uart0_puts("   ");
        if      (r[i].overflow)         { uart0_puts("[!]"); any_ovf  = 1; }
        else if (r[i].crc != expected)  { uart0_puts("[x]"); any_fail = 1; }
        else                              uart0_puts("[v]");
        uart0_puts("    ");
        uart0_putuint(r[i].cycles);
        uart0_puts("       ");
        if (r[i].cycles == worst) uart0_puts("1.00x (peor)");
        else                      uart0_putfix2x(worst, r[i].cycles);
        uart0_puts("\r\n");
    }

    uart0_puts("==================================================\r\n");
    if (any_fail) uart0_puts("*** Verificación FALLIDA: algun CRC non coincide co esperado ***\r\n");
    if (any_ovf)  uart0_puts("*** Overflow do TPM: reduce CRC8_BUF_LEN ***\r\n");

    while (1) { __NOP(); }
}
