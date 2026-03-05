# Makefile - SE (FRDM-KL46Z)

# --- Configuración del Compilador ---
CC = arm-none-eabi-gcc-14.2.0
CPU_MODEL := CPU_MKL46Z256VLL4

# Directorios
INCLUDES := ./includes
DRIVERS  := ./drivers
OPENCONF := ./openocd.cfg

# --- MAGIA DE AUTODETECCIÓN ---
# Busca todos los archivos .c dentro de la carpeta /drivers
SRCS_DRV := $(wildcard $(DRIVERS)/*.c)
# Genera la lista de objetos .o a partir de esos .c
DEPS_DRV := $(SRCS_DRV:.c=.o)

# CFLAGS: Compilación (Incluye la macro de CPU y los cabeceras)
CFLAGS = -O2 -Wall -mthumb -mcpu=cortex-m0plus -D$(CPU_MODEL) -I$(INCLUDES)

# LDFLAGS: Linkado (Añadimos nosys.specs para Newlib)
LDFLAGS = --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Tlink.ld

.PHONY: flash_led flash_hello flash_main clean cleanall

# --- Reglas de Linkado (.elf) ---

# Compilamos el .o principal (ej. led_blinky.o), el startup y TODOS los drivers detectados
%.elf: %.o startup.o $(DEPS_DRV)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map,$(@:.elf=.map) startup.o $< $(DEPS_DRV) -o $@

# --- Reglas de Compilación (.o) ---

# Regla para archivos en el directorio raíz (led_blinky.c, startup.c, etc.)
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para archivos en la carpeta /drivers
$(DRIVERS)/%.o: $(DRIVERS)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Regras de Flasheo ---

flash_led: led_blinky.elf 
	openocd -f $(OPENCONF) -c "program led_blinky.elf verify reset exit"

flash_hello: hello_world.elf
	openocd -f $(OPENCONF) -c "program hello_world.elf verify reset exit"

flash_main: main.elf
	openocd -f $(OPENCONF) -c "program main.elf verify reset exit"

# --- Limpieza ---

clean: 
	rm -f *.o $(DRIVERS)/*.o *.map

cleanall: clean
	rm -f *.elf
