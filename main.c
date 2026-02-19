#include "includes/MKL46Z4.h"


// LED (RG)
// LED_GREEN = PTD5
// LED_RED = PTE29

void delay(void)
{
  volatile int i;

  for (i = 0; i < 1000000; i++);
}

// LED_GREEN = PTD5
void led_green_init()
{
  SIM->COPC = 0x00;
  SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
  PORTD->PCR[5] = (PORTD->PCR[5] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
  GPIOD->PDDR |= (1U << 5);
  GPIOD->PSOR = (1U << 5);
}

void led_green_toggle()
{
  GPIOD->PTOR = (1U << 5);
}

// LED_RED = PTE29
void led_red_init()
{
  SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
  PORTE->PCR[29] = (PORTE->PCR[29] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
  GPIOE->PDDR |= (1U << 29);
  GPIOE->PSOR = (1U << 29);
}

void led_red_toggle(void)
{
  GPIOE->PTOR = (1U << 29);
}

int main(void)
{
  led_green_init();
  led_red_init();

  while (1) {
    led_green_toggle();
    delay();
    led_red_toggle();
  }

  return 0;
}
