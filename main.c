#include "includes/MKL46Z4.h"

#define OPEN 0
#define CLOSE 1

// LED (RG)
// LED_GREEN = PTD5
// LED_RED = PTE29

void delay(void)
{
  volatile int i;

  for (i = 0; i < 1000000; i++);
}

void disable_watchdog(void){
    SIM->COPC = 0x00;
}

// LED_GREEN = PTD5
void led_green_init()
{
  SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
  PORTD->PCR[5] = (PORTD->PCR[5] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
  GPIOD->PDDR |= (1U << 5);
  GPIOD->PSOR = (1U << 5);
}

void led_green_toggle()
{
  GPIOD->PTOR = (1U << 5);
}

void led_green_off(void)
{
  GPIOD->PSOR = (1U << 5);
}

void led_green_on(void)
{
  GPIOD->PCOR = (1U << 5);
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

void led_red_off(void)
{
  GPIOE->PSOR = (1U << 29);
}

void led_red_on(void)
{
  GPIOE->PCOR = (1U << 29);
}

//Door status
volatile int door1 = OPEN;
volatile int door2 = OPEN;


void toggle_door(int* door){
  if (*door == OPEN) { *door = CLOSE; }
  else { *door = OPEN; }
}

void irq_init(void)
{
    /* 1. Turn on clock to PORT C (not A) */
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    /* 2. Configure PTC3 (SW1) and PTC12 (SW3) */
    // Using 0x0A for falling edge (button press)
    PORTC->PCR[3]  = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_IRQC(0x0A);
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_IRQC(0x0A);

    /* 3. Clear any existing flags */
    PORTC->ISFR = 0xFFFFFFFF;

    /* 4. Enable PORTC Interrupts in NVIC */
    NVIC_ClearPendingIRQ(PORTC_PORTD_IRQn); // Note: On KL46Z, C and D often share an IRQ
    NVIC_EnableIRQ(PORTC_PORTD_IRQn);

    __enable_irq();
}

/* 5. Rename your handler to match the Port C/D vector */
void PORTC_PORTD_IRQHandler(void)
{
    uint32_t flags = PORTC->ISFR;
    
    // Clear flags immediately
    PORTC->ISFR = 0xFFFFFFFF; 

    if (flags & (1U << 3)) {   // SW1
        toggle_door(&door1);
    }
    if (flags & (1U << 12)) {  // SW3
        toggle_door(&door2);
    }
}

int main(void)
{
  //init
  disable_watchdog();
  irq_init();
  led_green_init();
  led_red_init();


  //Main loop
  while(1) {
      if (door1 == CLOSE || door2 == CLOSE) {
        //Secure status
        led_green_off();
        led_red_on();
      } else {
        //Unsecure status
        led_green_on();
        led_red_off();
      }
  }
  return 0;
}