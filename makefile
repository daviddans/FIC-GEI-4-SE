#Makefile - SE

#Variables implicitas
CC = arm-none-eabi-gcc-14.2.0
CFLAGS = -O2 -Wall -mthumb -mcpu=cortex-m0plus
#Target-specific flags
main.elf: CFLAGS += --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld
#Variables propias
includes:= ./includes
openconf:= ./openocd.cfg

#toolchain completo
all: cleanall build flash	

build: main.elf

#Regra de linkado
main.elf: main.o startup.o 
	$(CC) $(CFLAGS)  main.o startup.o -o main.elf
#Regra de compilacion para arm
%.o: %.c
	$(CC) -I $(includes) $(CFLAGS) -c $< -o $@

.PHONY: clean cleanall flash
#Regra para flashear o programa
flash: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

#Regras de limpeza 
clean: 
	rm -f *.o 

cleanall: clean
	rm -f *.elf