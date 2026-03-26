# Práctica 3 — Optimización con ensamblador (FRDM-KL46Z)

## Descrición

Tres implementacións da función `reverse_int()` (inversión bit a bit dun enteiro de 32 bits) no microcontrolador FRDM-KL46Z (Cortex-M0+), medidas co temporizador SysTick.

## Compilación e flasheo

```bash
make build       # Compilar todo → main.elf
make flash       # Flashear na placa via OpenOCD
make clean       # Eliminar .o
make cleanall    # Eliminar .o e .elf
```

Toolchain: `arm-none-eabi-gcc-14.2.0`. Require OpenOCD con interface CMSIS-DAP.

A saída dos resultados imprímese por UART0 a **115200 baud**.

## Arquivos

| Arquivo | Contido |
|---|---|
| `main.c` | Implementación C de referencia (compilada con `-Ofast`), implementación con ASM inline, medición con SysTick, saída por consola serie |
| `reverse_asm.s` | Implementación en ensamblador puro (función `reverse_int_asm`) |
| `makefile` | Sistema de compilación |
| `explicacion.txt` | Xustificación das optimizacións |

## Táboa comparativa de ciclos de execución

Entrada de proba: `0x12345678` → Saída esperada: `0x1E6A2C48`

| Implementación | Ciclos | Speedup vs C |
|---|---|---|
| C referencia (`-Ofast`) | ??? | 1.00x |
| ASM inline | ??? | ?.??x |
| ASM puro (`.s`) | ??? | ?.??x |

> Os ciclos medíronse co temporizador SysTick usando o reloxo do procesador (48 MHz).
> A medición inclúe a sobrecarga de chamada á función (~5-6 ciclos por dúas lecturas de `SysTick->VAL`).
