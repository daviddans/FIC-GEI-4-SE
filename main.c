#include "MKL46Z4.h"
#include "FreeRTOS.h"
#include "task.h"

void led_green_init()
{
	SIM->COPC = 0;
	SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
	PORTD->PCR[5] = PORT_PCR_MUX(1);
	GPIOD->PDDR |= (1 << 5);
	GPIOD->PSOR = (1 << 5);
}

void led_green_toggle()
{
	GPIOD->PTOR = (1 << 5);
}

void led_red_init()
{
	SIM->COPC = 0;
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
	PORTE->PCR[29] = PORT_PCR_MUX(1);
	GPIOE->PDDR |= (1 << 29);
	GPIOE->PSOR = (1 << 29);
}

void led_red_toggle(void)
{
	GPIOE->PTOR = (1 << 29);
}

void taskLedGreen(void *pvParameters)
{
    for (;;) {
        led_green_toggle();
        vTaskDelay(200/portTICK_RATE_MS);
    }
}

void taskLedRed(void *pvParameters)
{
    for (;;) {
        led_red_toggle();
        vTaskDelay(500/portTICK_RATE_MS);
    }
}

int main(void)
{
	led_green_init();
	led_red_init();

	/* create green led task */
	xTaskCreate(taskLedGreen, "TaskLedGreen",
		configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	/* create red led task */
	xTaskCreate(taskLedRed, "TaskLedRed",
		configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	
	/* start the scheduler */
	vTaskStartScheduler();

	/* should never reach here! */
	for (;;);

	return 0;
}

