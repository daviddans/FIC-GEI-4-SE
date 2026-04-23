/* main.c — Práctica 4: Productor-Consumidor con FreeRTOS (FRDM-KL46Z) */

#include "MKL46Z4.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ============================================================
 * Configuración
 * ============================================================ */
#define QUEUE_SIZE      99
#define MAX_TASKS        5
#define PROD_DELAY_MS  300
#define CONS_DELAY_MS  500
#define TASK_STACK_MED  (configMINIMAL_STACK_SIZE * 2)
#define TASK_PRIORITY    1

/* ============================================================
 * IPC:
 *   g_queue       — cola prod→cons (thread-safe internamente)
 *   g_sem_sw1/sw3 — semáforos binarios ISR→tarea de botón
 *   g_count_mutex — mutex para snapshot atómico de g_n_prod/g_n_cons
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
 * Tareas productora y consumidora
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
        xQueueReceive(g_queue, &data, portMAX_DELAY);
        vTaskDelay(CONS_DELAY_MS / portTICK_RATE_MS);
    }
}

/* Gestión dinámica de tareas.
 * Sólo llamado desde button_sw1_task (único escritor de g_n_prod).
 * El mutex protege el contador para que display_task lea un valor consistente.
 * vTaskDelete va fuera del mutex para no retenerlo durante la limpieza por idle. */
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
            break;
        }
    }
    while (g_n_prod > new_n) {
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        g_n_prod--;
        uint8_t idx = g_n_prod;
        xSemaphoreGive(g_count_mutex);
        vTaskDelete(prod_handles[idx]);
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
 * Botones y display LCD
 * ============================================================ */

static void buttons_init(void) {
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    /* SW1 — PTC3: GPIO, IRQ flanco bajada (IRQC=0xA), pull-up interno */
    PORTC->PCR[3]  = PORT_PCR_MUX(1) | PORT_PCR_IRQC(0xA)
                   | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* SW3 — PTC12: ídem */
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_IRQC(0xA)
                   | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    GPIOC->PDDR &= ~((1u << 3) | (1u << 12));
    NVIC_SetPriority(PORTC_PORTD_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(PORTC_PORTD_IRQn);
}

/* ISR compartida PORTC/PORTD — señaliza semáforo, no crea ni destruye tareas */
void PORTDIntHandler(void) {
    uint32_t flags_c = PORTC->ISFR;
    uint32_t flags_d = PORTD->ISFR;
    PORTC->ISFR = flags_c;
    PORTD->ISFR = flags_d;
    BaseType_t woken = pdFALSE;
    if (flags_c & (1u << 3))  xSemaphoreGiveFromISR(g_sem_sw3, &woken);
    if (flags_c & (1u << 12)) xSemaphoreGiveFromISR(g_sem_sw1, &woken);
    portYIELD_FROM_ISR(woken);
}

/* SW1 cicla n_prod: 0→1→2→3→4→5→0 */
static void button_sw1_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_sem_sw1, portMAX_DELAY);
        vTaskDelay(50 / portTICK_RATE_MS); /* debounce */
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t next = (g_n_prod + 1) % (MAX_TASKS + 1);
        xSemaphoreGive(g_count_mutex);
        set_producer_count(next);
    }
}

/* SW3 cicla n_cons: 0→1→2→3→4→5→0 */
static void button_sw3_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_sem_sw3, portMAX_DELAY);
        vTaskDelay(50 / portTICK_RATE_MS);
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t next = (g_n_cons + 1) % (MAX_TASKS + 1);
        xSemaphoreGive(g_count_mutex);
        set_consumer_count(next);
    }
}

/* ============================================================
 * LCD (SLCD KL46Z — modo 8 fases, 1/4 duty, fuente MCGIRCLK)
 *
 * Cada dígito usa dos pines frontplane:
 *   Pin A (P37/P7/P53/P10): segmentos D=0x11, E=0x22, G=0x44, F=0x88
 *   Pin B (P17/P8/P38/P11): segmentos DP=0x11, C=0x22, B=0x44, A=0x88
 * ============================================================ */
static const uint8_t LCD_PIN_A[4] = {37,  7, 53, 10};
static const uint8_t LCD_PIN_B[4] = {17,  8, 38, 11};

/* Waveform pin A por dígito 0-9 (segmentos D, E, G, F) */
static const uint8_t CHAR_A[10] = {0xBB, 0x00, 0x77, 0x55, 0xCC, 0xDD, 0xFF, 0x00, 0xFF, 0xCC};
/* Waveform pin B por dígito 0-9 (segmentos A, B, C) */
static const uint8_t CHAR_B[10] = {0xEE, 0x66, 0xCC, 0xEE, 0x66, 0xAA, 0xAA, 0xEE, 0xEE, 0xEE};

static void lcd_init(void) {
    /* 1. Relojes */
    MCG->C1 |= MCG_C1_IRCLKEN_MASK;
    SIM->SCGC5 |= SIM_SCGC5_SLCD_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK
               | SIM_SCGC5_PORTD_MASK  | SIM_SCGC5_PORTE_MASK;

    /* 2. Pines en MUX 0 (función LCD) */
    PORTB->PCR[7]  = PORTB->PCR[8]  = PORTB->PCR[10] = PORTB->PCR[11] = 0;
    PORTB->PCR[21] = PORTB->PCR[22] = PORTB->PCR[23] = 0;
    PORTC->PCR[17] = PORTC->PCR[18] = 0;
    PORTD->PCR[0]  = 0;
    PORTE->PCR[4]  = PORTE->PCR[5]  = 0;

    /* 3. Controlador: PADSAFE durante configuración */
    LCD->GCR |= LCD_GCR_PADSAFE_MASK;
    LCD->GCR &= ~LCD_GCR_LCDEN_MASK;

    LCD->GCR = LCD_GCR_SOURCE(1)    | LCD_GCR_ALTSOURCE(0) | LCD_GCR_ALTDIV(0) |
               LCD_GCR_RVTRIM(0)    | LCD_GCR_CPSEL_MASK   | LCD_GCR_LADJ(3)   |
               LCD_GCR_PADSAFE_MASK | LCD_GCR_FFR_MASK      | LCD_GCR_LCLK(4)   |
               LCD_GCR_DUTY(3);
    LCD->AR   = 0;
    LCD->FDCR = 0;

    /* 4. Pines habilitados (PEN) y backplanes (BPEN) */
    LCD->PEN[0]  = 0x000E0D80U; /* P7, P8, P10, P11, P17, P18, P19 */
    LCD->PEN[1]  = 0x00300160U; /* P37, P38, P40, P52, P53 */
    LCD->BPEN[0] = 0x000C0000U; /* COM2=P19, COM3=P18 */
    LCD->BPEN[1] = 0x00100100U; /* COM0=P40, COM1=P52 */

    /* 5. Backplanes: waveform dual-phase (modo 8 fases) */
    LCD->WF8B[40] = 0x11; /* COM0 — fases A+E */
    LCD->WF8B[52] = 0x22; /* COM1 — fases B+F */
    LCD->WF8B[19] = 0x44; /* COM2 — fases C+G */
    LCD->WF8B[18] = 0x88; /* COM3 — fases D+H */

    /* 6. Limpiar frontplanes y arrancar */
    for (int i = 0; i < 64; i++) {
        if (i != 40 && i != 52 && i != 19 && i != 18) LCD->WF8B[i] = 0;
    }
    LCD->GCR &= ~LCD_GCR_PADSAFE_MASK;
    LCD->GCR |= LCD_GCR_LCDEN_MASK;
}

static void lcd_set_digit(uint8_t pos, uint8_t val) {
    if (pos >= 4 || val > 9) return;
    LCD->WF8B[LCD_PIN_A[pos]] = CHAR_A[val];
    LCD->WF8B[LCD_PIN_B[pos]] = CHAR_B[val];
}

/* Muestra [d0][d1][d2][d3] de izquierda a derecha */
static void lcd_show(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3) {
    lcd_set_digit(0, d0);
    lcd_set_digit(1, d1);
    lcd_set_digit(2, d2);
    lcd_set_digit(3, d3);
}

/* Refresca LCD cada 200 ms: [cola/10][cola%10][n_prod][n_cons] */
static void display_task(void *pvParam) {
    (void)pvParam;
    for (;;) {
        xSemaphoreTake(g_count_mutex, portMAX_DELAY);
        uint8_t np = g_n_prod;
        uint8_t nc = g_n_cons;
        xSemaphoreGive(g_count_mutex);

        uint8_t q = (uint8_t)uxQueueMessagesWaiting(g_queue);
        lcd_show(q / 10, q % 10, np, nc);

        vTaskDelay(200 / portTICK_RATE_MS);
    }
}

/* ============================================================
 * main
 * ============================================================ */
int main(void) {
    SIM->COPC = 0;

    g_queue       = xQueueCreate(QUEUE_SIZE, sizeof(uint32_t));
    g_sem_sw1     = xSemaphoreCreateBinary();
    g_sem_sw3     = xSemaphoreCreateBinary();
    g_count_mutex = xSemaphoreCreateMutex();

    buttons_init();
    lcd_init();

    /* 1 productor y 1 consumidor iniciales — sin mutex porque el scheduler
     * aún no ha arrancado y no hay concurrencia */
    xTaskCreate(producer_task, "Prod", configMINIMAL_STACK_SIZE,
                (void *)0u, TASK_PRIORITY, &prod_handles[0]);
    g_n_prod = 1;
    xTaskCreate(consumer_task, "Cons", configMINIMAL_STACK_SIZE,
                NULL, TASK_PRIORITY, &cons_handles[0]);
    g_n_cons = 1;

    xTaskCreate(button_sw1_task, "BtnSW1",  TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(button_sw3_task, "BtnSW3",  TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(display_task,    "Display", TASK_STACK_MED, NULL, TASK_PRIORITY, NULL);

    vTaskStartScheduler();
    for (;;);
    return 0;
}
