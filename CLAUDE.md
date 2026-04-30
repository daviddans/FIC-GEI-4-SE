# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal embedded C project targeting the **NXP FRDM-KL46Z** development board (ARM Cortex-M0+ / MKL46Z256). University course (SE — Sistemas Electrónicos, FIC GEI 4th year). Each practicum / trabajo lives on its own branch (`Practica_1`, `practica_2`, `practica_3`, `practica_4`, `trabajo_tutelado_1`, …).

Current branch: **`trabajo_tutelado_1`** — was used for the *reverse_int* benchmark and is now the starting point for **`trabajo_tutelado_2`** (CRC8 — see `.claude/trabajo_tutelado_2_crc8.md`).

## Build & Flash Commands

```bash
make build       # Compile and link → main.elf
make flash       # Flash via OpenOCD (CMSIS-DAP) and reset
make clean       # Remove .o files
make cleanall    # Remove .o and .elf files
```

Toolchain: `arm-none-eabi-gcc-14.2.0`. Requires OpenOCD for flashing.

The makefile already supports per-target `CFLAGS` overrides (e.g. `main.o` is built with `-Ofast` on the previous trabajo). When mixing optimisation levels per file in a new practicum, add a target-specific rule next to the existing one — do not change the global `CFLAGS`.

## Architecture

- **main.c** — Application entry point. Includes `MKL46Z4.h` for peripheral register definitions.
- **startup.c** — Cortex-M0+ vector table (`g_pfnVectors` in `.isr_vector` section) and `ResetHandler` (copies `.data` from flash to SRAM, zeroes `.bss`, calls `main()`). Interrupt handlers are `WEAK`-aliased to `DefaultIntHandler` (infinite loop) — override by defining a function with the matching name (e.g., `SysTickIntHandler`, `PIT_IRQHandler`).
- **link.ld** — Linker script. Memory layout: interrupts at 0x0, flash config at 0x400, text at 0x410, RAM at 0x1FFFE000 (32 KB). Stack size defaults to 1 KB.
- **includes/** — CMSIS headers and `MKL46Z4.h` (MCU register definitions). All peripheral structs (`PIT`, `TPM0/1/2`, `LPTMR0`, `UART0`, `SIM`, `PORTx`, `GPIOx`, …) come from here.
- **freertos/** — Pre-compiled FreeRTOS objects (`tasks.o`, `queue.o`, `list.o`, `portable/`). Linked only when a practicum needs FreeRTOS.
- **frdm-kl46z-sdk/** — Full NXP MCUXpresso SDK (gitignored). Reference only for docs and additional driver source.
- **sistemasembebidos/** — Course seminar repo (gitignored).
- **openocd.cfg** — OpenOCD config using CMSIS-DAP interface targeting KL46.

UART0 bare-metal init at 115200 baud (PTA1=RX, PTA2=TX) is already implemented in the previous trabajo's `main.c` and is the standard console for benchmark output — copy it forward.

## Key Conventions

- All code runs bare-metal, no RTOS unless a practicum explicitly links `freertos/`. Direct register manipulation via CMSIS-defined structs (e.g., `SIM->SCGC5`, `GPIOC->PDDR`).
- Default compiler flags: `-O2 -Wall -mthumb -mcpu=cortex-m0plus`. Linked with `nano.specs` and `--gc-sections`.
- Assembly source files (`.s`) are built by the same makefile rule (`%.o: %.s`) and must use `.syntax unified`, `.cpu cortex-m0plus`, `.thumb`, `.thumb_func`. AAPCS calling convention: arg in `r0`, return in `r0`, `r0–r3` and `r12` are caller-saved.
- The Cortex-M0+ is **ARMv6-M** — instructions like `RBIT`, `CLZ`, `UBFX`, `BFI` are **not** available. Stick to the ARMv6-M Thumb subset.
- Always disable the watchdog at the top of `main()`: `SIM->COPC = 0;`.
- Makefile comments and most code comments are in **Galician**.
- Branch-per-practicum workflow: work on the practicum branch, tag releases. Previous trabajo is on `trabajo_tutelado_1`; new trabajo continues from there (see `.claude/trabajo_tutelado_2_crc8.md`).
