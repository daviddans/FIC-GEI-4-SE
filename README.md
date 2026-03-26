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

A saída dos resultados imprímese por UART0 a **115200 baud** (PTA2 = TX).

## Arquivos

| Arquivo | Contido |
|---|---|
| `main.c` | Implementación C de referencia (compilada con `-Ofast`), implementación con ASM inline, inicialización bare-metal UART0, medición con SysTick |
| `reverse_asm.s` | Implementación en ensamblador puro (función `reverse_int_asm`) |
| `makefile` | Sistema de compilación |
| `explicacion.txt` | Xustificación das optimizacións |

## Táboa comparativa de ciclos de execución

Entrada de proba: `0x12345678` → Saída: `0x1E6A2C48`

Reloxo: FLL interno por defecto (~20.97 MHz). Medición con SysTick (reloxo do procesador).

| Implementación | Ciclos | Speedup vs C |
|---|---|---|
| C referencia (`-Ofast`) | 312 | 1.00x |
| ASM inline | 175 | 1.78x |
| ASM puro (`.s`) | 175 | 1.78x |

> A medición inclúe a sobrecarga de chamada á función e dúas lecturas de `SysTick->VAL`
> (~4-5 ciclos de overhead constante en todas as medicións).
