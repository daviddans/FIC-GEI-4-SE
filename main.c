/* main.c — Práctica 4: Productor-Consumidor con FreeRTOS (FRDM-KL46Z) */

#include "MKL46Z4.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ============================================================
 * Configuración
 * ============================================================ */
#define QUEUE_SIZE      10
#define MAX_TASKS        5
#define PROD_DELAY_MS  300
#define CONS_DELAY_MS  500
#define TASK_STACK_MED  (configMINIMAL_STACK_SIZE * 2)
#define TASK_PRIORITY    1

/* ============================================================
 * Variables globales
 *
 * IPC utilizado (según tema 7):
 *   g_queue       — cola de mensajes para transferencia prod→cons (thread-safe internamente)
 *   g_sem_sw1/sw3 — semáforos binarios para señalización ISR→tarea de botón
 *   g_count_mutex — mutex para lectura atómica consistente de g_n_prod y g_n_cons
 * ============================================================ */
static QueueHandle_t     g_queue;
static SemaphoreHandle_t g_sem_sw1;
static SemaphoreHandle_t g_sem_sw3;
static SemaphoreHandle_t g_count_mutex;

static TaskHandle_t      prod_handles[MAX_TASKS];
static TaskHandle_t      cons_handles[MAX_TASKS];
static uint8_t           g_n_prod;
static uint8_t           g_n_cons;

/* ============================================================
 * Incremento 0: Tareas productora y consumidora
 * ============================================================ */

static void producer_task(void *pvParam) {
    uint32_t id = (uint32_t)pvParam;
    for (;;) {
        vTaskDelay(PROD_DELAY_MS / portTICK_RATE_MS);
        xQueueSend(g_queue, &id, 0); /* timeout 0: descarta si cola llena */
    }
}

static void consumer_task(void *pvParam) {
    uint32_t data;
    (void)pvParam;
    for (;;) {
        xQueueReceive(g_queue, &data, portMAX_DELAY); /* bloquea esperando dato */
        vTaskDelay(CONS_DELAY_MS / portTICK_RATE_MS);
    }
}

/* Gestión dinámica de tareas.
 * Sólo llamado desde button_sw1_task (único escritor de g_n_prod).
 * El mutex protege los incrementos/decrementos del contador para
 * que display_task lea un valor consistente. */
static void set_producer_count(uint8_t new_n) {
    while (g_n_prod < new_n) {
        if (xTaskCreate(producer_task, "Prod",
                        configMINIMAL_STACK_SIZE,
                        (void *)(uint32_t)g_n_prod, TASK_PRIORITY,
                        &prod_handles[g_n_prod]) == pdPASS) {
            xSemaphoreTake(g_count_mutex, portMAX_DELAY);
            g_n_prod++;
            xSemaphoreGive(g_count_mutex);
        } else {
            break; /* sin heap suficiente */
        }
    }
    while (g_n_prod > new_n) {
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        g_n_prod--;
        uint8_t idx = g_n_prod;
        xSemaphoreGive(g_count_mutex);
        vTaskDelete(prod_handles[idx]); /* fuera del mutex: puede activar idle */
        prod_handles[idx] = NULL;
    }
}

static void set_consumer_count(uint8_t new_n) {
    while (g_n_cons < new_n) {
        if (xTaskCreate(consumer_task, "Cons",
                        configMINIMAL_STACK_SIZE,
                        NULL, TASK_PRIORITY,
                        &cons_handles[g_n_cons]) == pdPASS) {
            xSemaphoreTake(g_count_mutex, portMAX_DELAY);
            g_n_cons++;
            xSemaphoreGive(g_count_mutex);
        } else {
            break;
        }
    }
    while (g_n_cons > new_n) {
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        g_n_cons--;
        uint8_t idx = g_n_cons;
        xSemaphoreGive(g_count_mutex);
        vTaskDelete(cons_handles[idx]);
        cons_handles[idx] = NULL;
    }
}

/* ============================================================
 * Incremento 1: Botones SW1 (PORTC pin 3) / SW3 (PORTC pin 12)
 *
 * ISR → semáforo binario → tarea de botón (debounce + acción)
 * El handler en startup.c se llama PORTDIntHandler y cubre PORTC+PORTD.
 * ============================================================ */

static void buttons_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    /* SW1 pin 3: MUX GPIO, IRQ flanco bajada (IRQC=0xA), pull-up */
    PORTC->PCR[3]  = PORT_PCR_MUX(1) | PORT_PCR_IRQC(0xA)
                   | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* SW3 pin 12: ídem */
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_IRQC(0xA)
                   | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    GPIOC->PDDR &= ~((1u << 3) | (1u << 12));
    /* Prioridad máxima que permite llamadas a API FreeRTOS desde ISR */
    NVIC_SetPriority(PORTC_PORTD_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(PORTC_PORTD_IRQn);
}

/* ISR compartida PORTC/PORTD. Señaliza con semáforo (no crea/destruye tareas). */
void PORTDIntHandler(void) {
    uint32_t flags_c = PORTC->ISFR;
    uint32_t flags_d = PORTD->ISFR;
    PORTC->ISFR = flags_c; /* escribir 1 limpia el flag */
    PORTD->ISFR = flags_d;
    BaseType_t woken = pdFALSE;
    if (flags_c & (1u << 3))  xSemaphoreGiveFromISR(g_sem_sw1, &woken);
    if (flags_c & (1u << 12)) xSemaphoreGiveFromISR(g_sem_sw3, &woken);
    portYIELD_FROM_ISR(woken);
}

/* Cicla n_prod entre 1 y MAX_TASKS */
static void button_sw1_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_sem_sw1, portMAX_DELAY); /* bloquea hasta ISR */
        vTaskDelay(50 / portTICK_RATE_MS);         /* debounce */
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t next = (g_n_prod % MAX_TASKS) + 1; /* 1→2→3→4→5→1 */
        xSemaphoreGive(g_count_mutex);
        set_producer_count(next);
    }
}

/* Cicla n_cons entre 1 y MAX_TASKS */
static void button_sw3_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_sem_sw3, portMAX_DELAY);
        vTaskDelay(50 / portTICK_RATE_MS);
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t next = (g_n_cons % MAX_TASKS) + 1;
        xSemaphoreGive(g_count_mutex);
        set_consumer_count(next);
    }
}

/* ============================================================
 * Incremento 2: LCD (SLCD bare-metal, MKL46Z4.h)
 *
 * Back planes (COM):
 *   LCD_P40/PTD0  → COM1 / PhaseA (bit 0 de WF8B)
 *   LCD_P52/PTE4  → COM2 / PhaseB (bit 1)
 *   LCD_P19/PTB23 → COM3 / PhaseC (bit 2)
 *   LCD_P18/PTB22 → COM4 / PhaseD (bit 3)
 *
 * Dígitos (izquierda→derecha):
 *   Pos 0: LCD_P37(PTC17)=low,  LCD_P17(PTB21)=high
 *   Pos 1: LCD_P7(PTB7)=low,    LCD_P8(PTB8)=high
 *   Pos 2: LCD_P53(PTE5)=low,   LCD_P38(PTC18)=high
 *   Pos 3: LCD_P10(PTB10)=low,  LCD_P11(PTB11)=high
 *
 * Segmentos por fase (convención FRDM-KL46Z):
 *   pin 'low'  → PhaseA=E, PhaseB=D, PhaseC=DP, PhaseD=G
 *   pin 'high' → PhaseA=A, PhaseB=B, PhaseC=C,  PhaseD=F
 * ============================================================ */

/* Waveform para cada dígito 0-9 (sin punto decimal) */
static const uint8_t SEG_LOW[10]  = {0x03,0x00,0x0B,0x0A,0x08,0x0A,0x0B,0x00,0x0B,0x0A};
static const uint8_t SEG_HIGH[10] = {0x0F,0x06,0x03,0x07,0x0E,0x0D,0x0D,0x07,0x0F,0x0F};

static const uint8_t PIN_LOW[4]   = {37,  7, 53, 10};
static const uint8_t PIN_HIGH[4]  = {17,  8, 38, 11};

static void lcd_init(void) {
    MCG->C1 |= MCG_C1_IRCLKEN_MASK; /* habilitar MCGIRCLK (~32 kHz slow IRC) para SOURCE=1 */
    SIM->SCGC5 |= SIM_SCGC5_SLCD_MASK | SIM_SCGC5_PORTB_MASK
               | SIM_SCGC5_PORTC_MASK  | SIM_SCGC5_PORTD_MASK
               | SIM_SCGC5_PORTE_MASK;

    /* MUX=0 (función LCD analógica) en todos los pines del display */
    PORTB->PCR[7] = PORTB->PCR[8] = PORTB->PCR[10] = PORTB->PCR[11] = 0;
    PORTB->PCR[21] = PORTB->PCR[22] = PORTB->PCR[23] = 0;
    PORTC->PCR[17] = PORTC->PCR[18] = 0;
    PORTD->PCR[0]  = 0;
    PORTE->PCR[4]  = PORTE->PCR[5]  = 0;

    LCD->GCR &= ~LCD_GCR_LCDEN_MASK; /* deshabilitar antes de reconfigurar */

    /* GCR: duty 1/4 (3), prescaler 1, reloj alternativo, carga alta, charge pump */
    LCD->GCR = LCD_GCR_DUTY(3) | LCD_GCR_LCLK(1) | LCD_GCR_SOURCE(1)
             | LCD_GCR_LADJ(1) | LCD_GCR_CPSEL(1) | LCD_GCR_RVTRIM(8);

    /* Habilitar todos los pines LCD (front + back planes) */
    LCD->PEN[0]  = 0x000E0D80U; /* P7,P8,P10,P11,P17,P18,P19 */
    LCD->PEN[1]  = 0x00300160U; /* P37,P38,P40,P52,P53 */

    /* Habilitar back planes */
    LCD->BPEN[0] = 0x000C0000U; /* P18(COM4), P19(COM3) */
    LCD->BPEN[1] = 0x00100100U; /* P40(COM1), P52(COM2) */

    /* Inicializar todos los waveforms a 0 (display apagado) */
    for (int i = 0; i < 64; i++) LCD->WF8B[i] = 0;

    /* Asignar fases a los back planes */
    LCD->WF8B[40] = 0x01; /* COM1 PhaseA */
    LCD->WF8B[52] = 0x02; /* COM2 PhaseB */
    LCD->WF8B[19] = 0x04; /* COM3 PhaseC */
    LCD->WF8B[18] = 0x08; /* COM4 PhaseD */

    LCD->GCR |= LCD_GCR_LCDEN_MASK;
}

static void lcd_set_digit(uint8_t pos, uint8_t val) {
    if (pos >= 4 || val > 9) return;
    LCD->WF8B[PIN_LOW[pos]]  = SEG_LOW[val];
    LCD->WF8B[PIN_HIGH[pos]] = SEG_HIGH[val];
}

/* Muestra: [d0][d1][d2][d3] de izquierda a derecha */
static void lcd_show(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3) {
    lcd_set_digit(0, d0);
    lcd_set_digit(1, d1);
    lcd_set_digit(2, d2);
    lcd_set_digit(3, d3);
}

/* Tarea display: refresca LCD cada 200 ms.
 * Lee contadores bajo mutex para snapshot consistente. */
static void display_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t np = g_n_prod;
        uint8_t nc = g_n_cons;
        xSemaphoreGive(g_count_mutex);

        uint8_t q = (uint8_t)uxQueueMessagesWaiting(g_queue); /* thread-safe sin mutex */
        lcd_show(q / 10, q % 10, np, nc);

        vTaskDelay(200 / portTICK_RATE_MS);
    }
}

/* ============================================================
 * main
 * ============================================================ */
int main(void) {
    SIM->COPC = 0; /* deshabilitar watchdog */

    g_queue       = xQueueCreate(QUEUE_SIZE, sizeof(uint32_t));
    g_sem_sw1     = xSemaphoreCreateBinary();
    g_sem_sw3     = xSemaphoreCreateBinary();
    g_count_mutex = xSemaphoreCreateMutex();

    buttons_init();
    lcd_init();

    /* 1 productor y 1 consumidor iniciales (creados directamente sin mutex
     * porque el scheduler aún no ha arrancado) */
    xTaskCreate(producer_task, "Prod", configMINIMAL_STACK_SIZE,
                (void *)0u, TASK_PRIORITY, &prod_handles[0]);
    g_n_prod = 1;
    xTaskCreate(consumer_task, "Cons", configMINIMAL_STACK_SIZE,
                NULL, TASK_PRIORITY, &cons_handles[0]);
    g_n_cons = 1;

    xTaskCreate(button_sw1_task, "BtnSW1", TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(button_sw3_task, "BtnSW3", TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(display_task,    "Display", TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);

    vTaskStartScheduler();
    for (;;);
    return 0;
}
