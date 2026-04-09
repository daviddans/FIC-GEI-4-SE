#Makefile - SE Practica 3

#Variables implicitas
CC = arm-none-eabi-gcc-14.2.0
CFLAGS = -O2 -Wall -mthumb -mcpu=cortex-m0plus
#Target-specific flags
main.elf: CFLAGS += --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld
#Variables propias
includes := $(addprefix -I, ./ ./includes ./freertos/ ./freertos/portable/ \
                           ./freertos/include/ ./freertos/portable/GCC/ARM_CM0/)
openconf := ./openocd.cfg
DRIVERS := $(wildcard drivers/*.o)

#Fontes do kernel FreeRTOS
FREERTOS_SRC := freertos/tasks.c freertos/list.c freertos/queue.c \
                freertos/portable/GCC/ARM_CM0/port.c \
                freertos/portable/MemMang/heap_4.c
FREERTOS_OBJ := $(FREERTOS_SRC:.c=.o)

#toolchain completo
build: main.elf

#Regra de linkado
main.elf: main.o startup.o $(FREERTOS_OBJ)
	$(CC) $(CFLAGS) main.o startup.o $(FREERTOS_OBJ) $(DRIVERS) -o main.elf

#Regra de compilacion para arm
%.o: %.c
	$(CC) $(includes) $(CFLAGS) -c $< -o $@

#Regra de compilacion para ensamblador
%.o: %.s
	$(CC) -mthumb -mcpu=cortex-m0plus -c $< -o $@

.PHONY: clean cleanall flash
#Regra para flashear o programa
flash: main.elf
	openocd -f $(openconf) -c "program main.elf verify reset exit"

#Regras de limpeza
clean:
	rm -f *.o $(FREERTOS_OBJ)

cleanall: clean
	rm -f *.elf
