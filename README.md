# Trabajo Tutelado 2 — Benchmark CRC-8 (FRDM-KL46Z)

## Enunciado

Implementar un algoritmo CRC-8 de tres formas distintas, medir os ciclos
de execución cun temporizador **distinto de SysTick** e comparar:

1. **C "naive"** (só desprazamentos e operadores lóxicos), compilado con `-O0`.
2. A **mesma fonte** recompilada con `-Ofast`.
3. **Versión propia en ensamblador** ARMv6-M optimizada a man.

Polinomio: CRC-8/CCITT (`0x07`, INIT=0). Datos: buffer en RAM (tamaño parametrizable).

## Compilación e flasheo

```bash
make build    # compilar todo → main.elf
make flash    # flashear na placa via OpenOCD/CMSIS-DAP
```

Saída por **UART0 a 115200 baud, 8N1** (PTA2 = TX).

## Temporizador

**TPM0 alimentado por MCGFLLCLK con prescaler 1** → cada tick é exactamente
**1 ciclo de CPU**, sen conversións. Como o sistema é bare-metal e
deterministico (sen IRQs, sen caché), unha **única chamada** (single-shot)
dá un resultado estable; non fai falta promediar.

O TPM é de **16 bit** (MOD=0xFFFF), polo que `CRC8_BUF_LEN` debe escollerse
para que a versión máis lenta (`-O0`, ~321 ciclos/byte) non supere 65535
ciclos — na práctica, `BUF ≤ ~200`. Se algunha medición desborda, o flag
`TOF` do TPM dispárase e o programa marca esa fila con `[!]` na táboa.

## A optimización en ensamblador

A versión C calcula o CRC bit a bit usando unha máscara aritmética:

```c
mask = -(crc >> 7);              // 0xFF se MSB=1, 0x00 se MSB=0
crc  = (crc << 1) ^ (POLY & mask);
```

O compilador, mesmo con `-Ofast`, xera 6-7 instrucións por bit. A versión
ASM aproveita unha característica do procesador: a instrución `LSLS`
(desprazar á esquerda) **garda o bit que sae nun rexistro especial chamado
carry flag**.

O truco clave é manter o CRC nos **8 bits altos** do rexistro durante todo
o procesamento. Así, ao facer `LSLS r, r, #1`, o bit que cae ao carry é
exactamente o MSB do CRC:

```asm
.Lbit:
    lsls r2, r2, #1     @ despraza; MSB do CRC → carry
    bcc  .Lno_xor       @ se carry=0, salta o XOR
    eors r2, r3         @ crc ^= POLY (xa pre-deslocado a bits 24..31)
.Lno_xor:
    subs r5, r5, #1
    bne  .Lbit
```

Co CRC nos bits altos, os bits inferiores manteñense sempre a 0 (porque
`LSLS` recheea con cero), polo que **non fai falta `UXTB`** para truncar
a 8 bits. O bucle interno queda en **5 instrucións por bit** en vez das
6-7 que produce o compilador.

## Estrutura

| Arquivo       | Contido                                                         |
|---------------|-----------------------------------------------------------------|
| `crc8.c`      | Algoritmo branchless en C (compílase 2× con `-O0` e `-Ofast`)   |
| `crc8_asm.s`  | Versión ASM optimizada                                          |
| `main.c`      | Init UART/TPM, buffer, benchmark, impresión por UART            |
| `makefile`    | Regras de compilación dual (`crc8_O0.o` / `crc8_Ofast.o`)       |

## Resultados

Saída exemplo por UART tras flashear:

```text
================ CRC-8 Benchmark ================
Algoritmo   : CRC-8/CCITT (POLY=0x07, INIT=0x00)
Datos       : buf[i] = i, len = 128 B
Temporizador: TPM0 @ MCGFLLCLK, presc 1 (1 tick = 1 ciclo CPU)
Medición    : single-shot
CRC esperado: 0xED

Resultados (mais rapido primeiro):
  Versión    CRC    OK    ciclos     speedup vs peor
  ASM        0xED   [v]    7320       5.62x
  C -Ofast   0xED   [v]    7446       5.52x
  C -O0      0xED   [v]    41140       1.00x (peor)
==================================================
```

## Bibliografía

Recursos consultados durante o desenvolvemento:

- **NXP/Freescale**, *KL46 Sub-Family Reference Manual* (`KL46P121M48SF4RM.pdf`,
  Rev. 3, 2013) — secciones **3.8 (Timer modules configuration)**,
  **5.7 (Clock distribution:  TPM, LPTMR clocking)** e capítulos individuais
  para os layouts dos rexistros do TPM e UART0.

- **Implementacion del algoritmo crc** https://gist.github.com/David256/f10105e43b45ef8ac292d6f5a11f0ca2

- **ARM Limited**, *Application Note AN179: CRC computation using ARM cores*
  (ARM DAI 0179B) — describe a técnica de manter o CRC no byte alto dun
  rexistro de 32 bits para que `LSL #1` deposite o MSB directamente no carry
  flag, reducindo o bucle interno a 5 instrucións; é a base da optimización
  implementada en `crc8_asm.s`.