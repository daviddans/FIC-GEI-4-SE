/*
 * main.c — PWM + Sensor de luz (FRDM-KL46Z, Cortex-M0+)
 *
 * Le o sensor de luz analóxico (PTE22 / ADC0_SE3) e controla a intensidade
 * dos dous LEDs mediante PWM (TPM0):
 *   - LED verde (PTD5 / TPM0_CH5): máximo en escuridade, apagado con luz.
 *   - LED vermello (PTE29 / TPM0_CH2): apagado en escuridade, máximo con luz.
 *
 * UART0 a 115200 baud (PTA2=TX) imprime o valor ADC e os duty cycles.
 */

#include "includes/MKL46Z4.h"
#include <stdint.h>

/* PWM: TPM0, prescaler /8, MCGFLLCLK ~20.97 MHz → tick ~2.62 MHz.
 * MOD=999 → f_PWM ≈ 2.62 kHz.
 * Configuración high-true + LEDs activo-baixo:
 *   CnV=0     → LED ao MÁXIMO brillo.
 *   CnV=MOD+1 → LED APAGADO de verdade (100% duty pin alto). */
#define PWM_MOD  999u

/* Rango útil do sensor de luz (observado na placa):
 *   ADC ≲ 200  cunha lanterna directa → saturación a luz.
 *   ADC ≳ 3800 co sensor tapado co dedo → saturación a escuridade.
 * Reescalamos [ADC_LO, ADC_HI] → [0, PWM_MOD] e facemos clamp fora dese rango,
 * así o duty cycle cobre todo o rango visual útil. */
#define ADC_LO   200u
#define ADC_HI   3800u
#define ADC_SPAN (ADC_HI - ADC_LO)

/* ---- UART0 ---------------------------------------------------------------- */

static void uart0_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    SIM->SCGC4 |= SIM_SCGC4_UART0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_UART0SRC_MASK) | SIM_SOPT2_UART0SRC(1);
    PORTA->PCR[1] = PORT_PCR_MUX(2);
    PORTA->PCR[2] = PORT_PCR_MUX(2);
    UART0->C2  = 0;
    UART0->C4  = 0x0D;          /* OSR = 14 */
    UART0->BDH = 0x00;
    UART0->BDL = 13;            /* SBR = 13 → ~115228 baud */
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
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (v == 0) { uart0_putchar('0'); return; }
    while (v) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart0_puts(&buf[i]);
}

/* ---- TPM0 como PWM de dous canais ---------------------------------------- */

static void tpm0_pwm_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK;
    SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
    SIM->SOPT2  = (SIM->SOPT2 & ~SIM_SOPT2_TPMSRC_MASK) | SIM_SOPT2_TPMSRC(1);

    PORTD->PCR[5]  = PORT_PCR_MUX(4);  /* PTD5  = TPM0_CH5 (LED verde) */
    PORTE->PCR[29] = PORT_PCR_MUX(3);  /* PTE29 = TPM0_CH2 (LED vermello) */

    TPM0->SC  = 0;
    TPM0->MOD = PWM_MOD;

    /* PWM edge-aligned HIGH-TRUE: MSB=1, ELSB=1, ELSA=0 (ELSB:ELSA=10).
     * Reload pon o pin alto, o match con CnV pono baixo. LEDs activo-baixo:
     *   CnV=0      → match inmediato → pin baixo todo o periodo → LED MÁXIMO.
     *   CnV=MOD    → un só tick baixo por periodo → LED case apagado (1/MOD).
     *   CnV=MOD+1  → nunca hai match → pin alto sempre → LED APAGADO real. */
    TPM0->CONTROLS[5].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    TPM0->CONTROLS[5].CnV  = PWM_MOD + 1;
    TPM0->CONTROLS[2].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    TPM0->CONTROLS[2].CnV  = PWM_MOD + 1;

    /* Arrancar: CMOD=01 (reloxo do módulo), PS=011 (÷8) */
    TPM0->SC = TPM_SC_CMOD(1) | TPM_SC_PS(3);
}

/* ---- ADC0 — sensor de luz PTE22 / ADC0_SE3 ------------------------------- */

static void adc0_init(void)
{
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
    PORTE->PCR[22] = 0;                         /* PTE22 como entrada analóxica */

    ADC0->CFG1 = ADC_CFG1_MODE(1)               /* 12 bit single-ended */
               | ADC_CFG1_ADIV(1)               /* reloxo ÷2 */
               | ADC_CFG1_ADLSMP_MASK;          /* sample longo */
    ADC0->CFG2 = ADC_CFG2_ADLSTS(0);            /* +24 ciclos ADCK de sample */
    ADC0->SC2  = 0;                             /* disparo por software */

    /* Auto-calibración — patrón de fsl_adc16.c::ADC16_DoAutoCalibration.
     * 1) Limpa CALF e arranca CAL.
     * 2) Espera COCO en SC1[0] e descarta R[0].
     * 3) Suma CLP0..CLP4 + CLPS, divide por 2 e escribe en PG (gain plus). */
    ADC0->SC3 = ADC_SC3_CAL_MASK | ADC_SC3_CALF_MASK;
    while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK)) { }
    (void)ADC0->R[0];
    uint32_t cal = ADC0->CLP0 + ADC0->CLP1 + ADC0->CLP2 + ADC0->CLP3
                 + ADC0->CLP4 + ADC0->CLPS;
    ADC0->PG = 0x8000u | (cal >> 1);

    /* Media por hardware: 32 mostras (AVGS=3). Reduce o ruído nas zonas
     * saturadas, evitando o parpadeo do LED cando o sensor está tapado. */
    ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);
}

static uint16_t adc0_read(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(3);            /* canal SE3, inicia conversión */
    while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK));
    return (uint16_t)ADC0->R[0];
}

/* ---- Control de LEDs ------------------------------------------------------ */

/* Últimos valores escritos en CnV — visibles para o printf de depuración. */
static uint16_t g_red_cnv;
static uint16_t g_green_cnv;

/* Mapea CnV "ideal" a un valor seguro para o rexistro:
 * cando o duty pedido é 100%, escribimos MOD+1 (como fai o SDK) para garantir
 * un nivel constante e evitar o tick de transición no límite do contador. */
static inline uint16_t cnv_clamp(uint32_t v)
{
    return (v >= PWM_MOD) ? (uint16_t)(PWM_MOD + 1) : (uint16_t)v;
}

static void set_leds(uint16_t adc)
{
    uint32_t v;
    if (adc <= ADC_LO)      v = 0;
    else if (adc >= ADC_HI) v = ADC_SPAN;
    else                    v = adc - ADC_LO;

    uint32_t red   = (v * PWM_MOD) / ADC_SPAN;
    uint32_t green = PWM_MOD - red;

    g_red_cnv   = cnv_clamp(red);
    g_green_cnv = cnv_clamp(green);
    TPM0->CONTROLS[2].CnV = g_red_cnv;
    TPM0->CONTROLS[5].CnV = g_green_cnv;
}

/* ---- Retardo --------------------------------------------------------------- */

/* ~1 ms por unidade a 20.97 MHz con -O2 (estimación). */
static void delay_ms(uint32_t ms)
{
    while (ms--) {
        for (volatile uint32_t i = 0; i < 6990u; i++) { __NOP(); }
    }
}

/* ---- main ----------------------------------------------------------------- */

int main(void)
{
    SIM->COPC = 0;
    uart0_init();
    tpm0_pwm_init();
    adc0_init();

    uart0_puts("\r\n==== PWM + Sensor de luz (FRDM-KL46Z) ====\r\n");
    uart0_puts("  ADC [0-4095]  |  GREEN_CnV  |  RED_CnV  (CnV=MOD+1 => OFF)\r\n");
    uart0_puts("---------------------------------------------------------\r\n");

    while (1) {
        uint16_t v = adc0_read();
        set_leds(v);

        uart0_puts("  ADC=");
        uart0_putuint(v);
        uart0_puts("  GREEN_CnV=");
        uart0_putuint(g_green_cnv);
        uart0_puts("  RED_CnV=");
        uart0_putuint(g_red_cnv);
        uart0_puts("\r\n");

        delay_ms(200);
    }
}
