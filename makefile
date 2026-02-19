#Makefile

includes:= ./includes
oocdconf:= ./openocd.cfg

flash: main.elf
	openocd -f $(oocdconf) -c "program main.elf verify reset exit"

build: main.elf

main.elf: main.o startup.o
	arm-none-eabi-gcc -O2 -Wall -mthumb -mcpu=cortex-m0plus --specs=nano.specs -Wl,--gc-sections,-Map,main.map,-Tlink.ld main.o startup.o -o main.elf

main.o: main.c
	arm-none-eabi-gcc -I $(includes) -O2 -Wall -mthumb -mcpu=cortex-m0plus -c -o main.o main.c

startup.o: startup.c 
	arm-none-eabi-gcc -I $(includes) -O2 -Wall -mthumb -mcpu=cortex-m0plus -c -o startup.o startup.c

clean: 
	rm -f *.o *.elf 
