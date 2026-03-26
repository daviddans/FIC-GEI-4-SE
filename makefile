#Makefile - SE

#Variables implicitas
CC = arm-none-eabi-gcc-14.2.0
CFLAGS = -O2 -Wall -mthumb -mcpu=cortex-m0plus
#Target-specific flags
main.elf: CFLAGS += --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld
#Compilacion de main.c con -Ofast (requisito da practica)
main.o: CFLAGS := -Ofast -Wall -mthumb -mcpu=cortex-m0plus
#Variables propias
includes:= ./includes
openconf:= ./openocd.cfg
DRIVERS := $(wildcard drivers/*.o)

#toolchain completo
build: main.elf

#Regra de linkado
main.elf: main.o startup.o reverse_asm.o
	$(CC) $(CFLAGS)  main.o startup.o reverse_asm.o $(DRIVERS) -o main.elf
#Regra de compilacion para arm
%.o: %.c
	$(CC) -I $(includes) $(CFLAGS) -c $< -o $@

#Regra de compilacion para ensamblador
%.o: %.s
	$(CC) -mthumb -mcpu=cortex-m0plus -c $< -o $@

.PHONY: clean cleanall flash
#Regra para flashear o programa
flash: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

#Regras de limpeza
clean:
	rm -f *.o

cleanall: clean
	rm -f *.elf
