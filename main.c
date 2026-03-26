#include "includes/MKL46Z4.h"

extern unsigned int reverse_int_asm(unsigned int in);

/* --- Implementacions de reverse_int --- */

/* 1. Version C de referencia (compilada con -Ofast) */
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

/* 2. Version con ASM inline.
 * Optimizacion: LSRS mete o LSB no carry flag, ADCS Rd,Rd fai (Rd<<1)|carry.
 * O bucle queda en 4 instruccions en vez de 7. */
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

/* --- UART0 bare-metal a 115200 baud (reloxo FLL ~20.97 MHz) --- */

static void uart0_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    SIM->SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_UART0SRC_MASK) | SIM_SOPT2_UART0SRC(1);
    PORTA->PCR[1] = PORT_PCR_MUX(2);   /* PTA1 = UART0_RX */
    PORTA->PCR[2] = PORT_PCR_MUX(2);   /* PTA2 = UART0_TX */
    UART0->C2  = 0;
    UART0->C4  = 0x0D;   /* OSR = 14 */
    UART0->BDH = 0x00;
    UART0->BDL = 13;     /* SBR = 13  ->  20971520/(14*13) = 115228 baud */
    UART0->C2  = UART0_C2_TE_MASK | UART0_C2_RE_MASK;
}

static void uart0_putchar(char c)
{
    while (!(UART0->S1 & UART0_S1_TDRE_MASK));
    UART0->D = (uint8_t)c;
}

static void uart0_puts(const char *s)
{
    while (*s) uart0_putchar(*s++);
}

static void uart0_puthex(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart0_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart0_putchar(hex[(v >> i) & 0xF]);
}

static void uart0_putuint(uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (v == 0) { uart0_putchar('0'); return; }
    while (v > 0) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart0_puts(&buf[i]);
}

/* --- SysTick: contador libre descendente (sen interrupcion) --- */

static void systick_init(void)
{
    SysTick->LOAD = 0x00FFFFFF;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* --- Main --- */

int main(void)
{
    SIM->COPC = 0;  /* Desactivar watchdog */

    uart0_init();
    systick_init();

    /* volatile: impide que -Ofast calcule o resultado en tempo de compilacion */
    volatile uint32_t test_input = 0x12345678;
    volatile uint32_t result;
    uint32_t start, end, cycles;

    uart0_puts("=== Reverse Int Benchmark ===\r\n");
    uart0_puts("Input: "); uart0_puthex(test_input); uart0_puts("\r\n");

    start = SysTick->VAL;
    result = reverse_int_c(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;
    uart0_puts("C -Ofast:   result="); uart0_puthex(result);
    uart0_puts("  ciclos="); uart0_putuint(cycles); uart0_puts("\r\n");

    start = SysTick->VAL;
    result = reverse_int_inline(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;
    uart0_puts("Inline ASM: result="); uart0_puthex(result);
    uart0_puts("  ciclos="); uart0_putuint(cycles); uart0_puts("\r\n");

    start = SysTick->VAL;
    result = reverse_int_asm(test_input);
    end = SysTick->VAL;
    cycles = (start - end) & 0x00FFFFFF;
    uart0_puts("ASM puro:   result="); uart0_puthex(result);
    uart0_puts("  ciclos="); uart0_putuint(cycles); uart0_puts("\r\n");

    while (1) { __NOP(); }
}
