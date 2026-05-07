# Makefile - SE - Trabajo Tutelado 2 (CRC-8 benchmark)

CC      = arm-none-eabi-gcc-14.2.0
CFLAGS  = -O2 -Wall -mthumb -mcpu=cortex-m0plus

# Rutas
includes := ./includes
openconf := ./openocd.cfg

# Targets de alto nivel
build: main.elf

# Linkado: main + startup + dúas versións C de crc8 + crc8 ASM
main.elf: main.o startup.o crc8_O0.o crc8_Ofast.o crc8_asm.o
	$(CC) $(CFLAGS) --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld \
	      main.o startup.o crc8_O0.o crc8_Ofast.o crc8_asm.o -o main.elf

# Regra xenérica para .c (usada por main.o e startup.o)
%.o: %.c
	$(CC) -I$(includes) $(CFLAGS) -c $< -o $@

# crc8.c compílase DÚAS VECES con flags distintos para que o benchmark poida
# comparar -O0 fronte a -Ofast. -DCRC8_FUNC renomea o símbolo en cada obxecto
# para evitar colisión no linkado.
crc8_O0.o: crc8.c
	$(CC) -I$(includes) -O0 -Wall -mthumb -mcpu=cortex-m0plus \
	      -DCRC8_FUNC=crc8_O0 -c $< -o $@

crc8_Ofast.o: crc8.c
	$(CC) -I$(includes) -Ofast -Wall -mthumb -mcpu=cortex-m0plus \
	      -DCRC8_FUNC=crc8_Ofast -c $< -o $@

# Regra para ensamblador (.s)
%.o: %.s
	$(CC) -mthumb -mcpu=cortex-m0plus -c $< -o $@

.PHONY: clean cleanall flash

# Flashear o programa via OpenOCD (CMSIS-DAP)
flash: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

# Limpeza
clean:
	rm -f *.o

cleanall: clean
	rm -f *.elf *.map
