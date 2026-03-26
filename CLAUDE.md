# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal embedded C project targeting the **NXP FRDM-KL46Z** development board (ARM Cortex-M0+ / MKL46Z256). University course (SE — Sistemas Electrónicos, FIC GEI 4th year). Each practicum lives on its own branch (`Practica_1`, `practica_2`, `practica3`, etc.).

## Build & Flash Commands

```bash
make build       # Compile and link → main.elf
make flash       # Flash via OpenOCD (CMSIS-DAP) and reset
make clean       # Remove .o files
make cleanall    # Remove .o and .elf files
```

Toolchain: `arm-none-eabi-gcc-14.2.0`. Requires OpenOCD for flashing.

## Architecture

- **main.c** — Application entry point. Includes `MKL46Z4.h` for peripheral register definitions.
- **startup.c** — Cortex-M0+ vector table (`g_pfnVectors` in `.isr_vector` section) and `ResetHandler` (copies `.data` from flash to SRAM, zeroes `.bss`, calls `main()`). Interrupt handlers are `WEAK`-aliased to `DefaultIntHandler` (infinite loop) — override by defining a function with the matching name (e.g., `SysTickIntHandler`).
- **link.ld** — Linker script. Memory layout: interrupts at 0x0, flash config at 0x400, text at 0x410, RAM at 0x1FFFE000 (32 KB). Stack size defaults to 1 KB.
- **includes/** — CMSIS headers and `MKL46Z4.h` (MCU register definitions).
- **drivers/** — Pre-compiled `.o` files from the SDK (clock, GPIO, LPSCI/UART, flash, debug console, etc.). Linked directly — no source files.
- **frdm-kl46z-sdk/** — Full NXP MCUXpresso SDK (gitignored). Reference only for docs and additional driver source.
- **openocd.cfg** — OpenOCD config using CMSIS-DAP interface targeting KL46.

## Key Conventions

- All code runs bare-metal, no RTOS. Direct register manipulation via CMSIS-defined structs (e.g., `SIM->SCGC5`, `GPIOC->PDDR`).
- Compiler flags: `-O2 -Wall -mthumb -mcpu=cortex-m0plus`. Linked with `nano.specs` and `--gc-sections`.
- Makefile comments are in Galician.
- Branch-per-practicum workflow: work on the practicum branch, tag releases.
