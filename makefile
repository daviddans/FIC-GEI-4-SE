#Makefile - SE

#Variables implicitas
CC = arm-none-eabi-gcc-14.2.0
CFLAGS = -O2 -Wall -mthumb -mcpu=cortex-m0plus
#Target-specific flags
%.elf: CFLAGS += --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld
#Variables propias
includes:= ./includes
drivers:= ./drivers
openconf:= ./openocd.cfg

.PHONY = flash_led flash_hello flash_main
#Varibles da segunda practica

#Regras para a segunda practica

flash_led: led_blinky.elf 
	openocd -f $(openconf) -c "program led_blinky.elf verify reset exit"

flash_hello: hello_world.elf
	openocd -f $(openconf) -c "program hello_world.elf verify reset exit"


#Regra de linkado
%.elf: %.o startup.o 
	$(CC) $(CFLAGS) $< startup.o -o $@

#Regra de compilacion para arm
%.o: %.c
	$(CC) -I $(includes) $(CFLAGS) -c $< -o $@

#Regra para flashear o programa main.c
flash_main: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

#Regras de limpeza 
clean: 
	rm -f *.o 

cleanall: clean
	rm -f *.elf
