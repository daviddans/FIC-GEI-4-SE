# Makefile — SE — Traballo Tutelado 2 (PWM + Sensor de luz)

CC      = arm-none-eabi-gcc-14.2.0
CFLAGS  = -O2 -Wall -mthumb -mcpu=cortex-m0plus

includes := ./includes
openconf := ./openocd.cfg

build: main.elf

main.elf: main.o startup.o
	$(CC) $(CFLAGS) --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld \
	      main.o startup.o -o main.elf

%.o: %.c
	$(CC) -I$(includes) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) -mthumb -mcpu=cortex-m0plus -c $< -o $@

.PHONY: clean cleanall flash

flash: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

clean:
	rm -f *.o

cleanall: clean
	rm -f *.elf *.map
