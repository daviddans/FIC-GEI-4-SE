#include "includes/MKL46Z4.h"

/* Funcions dos drivers pre-compilados (resolvidas polo linker) */
extern void BOARD_InitPins(void);
extern void BOARD_BootClockRUN(void);
extern void BOARD_InitDebugConsole(void);
extern int DbgConsole_Printf(const char *fmt_s, ...);

/* Funcion en ensamblador puro (reverse_asm.s) */
extern unsigned int reverse_int_asm(unsigned int in);

/* Implementacion C de referencia (compilada con -Ofast) */
__attribute__((noinline))
unsigned int reverse_int_c(unsigned int in)
{
    unsigned int out = 0;

    for (unsigned int i = 0; i < 32; i++) {
        out = out << 1;
        out |= in & 1;
        in = in >> 1;
    }

    return out;
}

/*
 * Optimizacion con ensamblador inline.
 *
 * O truco: LSRS bota o bit menos significativo ao carry flag.
 * Logo ADCS Rd,Rd calcula Rd = Rd + Rd + C = (Rd << 1) | carry.
 * Dous instruccions fan o traballo de catro no bucle C.
 *
 * Corpo do bucle (4 instruccions por iteracion):
 *   lsrs  in, in, #1    -> LSB de in vai ao carry
 *   adcs  out, out       -> out = (out << 1) | carry
 *   subs  cnt, cnt, #1   -> decrementa (machaca carry, pero xa o consumiu adcs)
 *   bne   loop           -> bne mira o zero flag, non o carry
 */
__attribute__((noinline))
unsigned int reverse_int_inline(unsigned int in)
{
    unsigned int out = 0;
    unsigned int count = 32;
    __asm volatile (
        ".syntax unified          \n\t"
        "1:                       \n\t"
        "lsrs %[in],  %[in],  #1 \n\t"
        "adcs %[out], %[out]      \n\t"
        "subs %[cnt], %[cnt], #1  \n\t"
        "bne  1b                  \n\t"
        ".syntax divided          \n\t"
        : [out] "+l" (out), [in] "+l" (in), [cnt] "+l" (count)
        :
        : "cc"
    );
    return out;
}

/* Configura SysTick como contador libre descendente (sen interrupcion) */
static void systick_init(void)
{
    SysTick->LOAD = 0x00FFFFFF;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_ENABLE_Msk;
}

int main(void)
{
    /* Desactivar o watchdog (COP) — esta activado por defecto e reinicia o MCU */
    SIM->COPC = 0;

    /* Inicializacion da placa e consola serie */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    systick_init();

    uint32_t test_input = 0x12345678;
    volatile uint32_t result;
    uint32_t start, end, cycles;

    /* Medicion da implementacion C (-Ofast) */
    start = SysTick->VAL;
    result = reverse_int_c(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;

    DbgConsole_Printf("=== Reverse Int Benchmark ===\r\n");
    DbgConsole_Printf("Input:  0x%08X\r\n", test_input);
    DbgConsole_Printf("C -Ofast:  resultado=0x%08X  ciclos=%u\r\n", (unsigned)result, cycles);

    /* Medicion da implementacion con ASM inline */
    start = SysTick->VAL;
    result = reverse_int_inline(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;

    DbgConsole_Printf("Inline ASM: resultado=0x%08X  ciclos=%u\r\n", (unsigned)result, cycles);

    /* Medicion da implementacion en ensamblador puro (.s) */
    start = SysTick->VAL;
    result = reverse_int_asm(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;

    DbgConsole_Printf("ASM puro:   resultado=0x%08X  ciclos=%u\r\n", (unsigned)result, cycles);

    while (1) {
        __NOP();
    }
}
