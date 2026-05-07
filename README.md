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
CRC esperado: 0xXX

Resultados (mais rapido primeiro):
  Versión    CRC    OK    ciclos     speedup vs peor
  ASM        0xXX   [v]    ~1300       ~6.3x
  C -Ofast   0xXX   [v]    ~1500       ~5.5x
  C -O0      0xXX   [v]    ~8200       1.00x (peor)
==================================================
```

## Bibliografía

Recursos consultados durante o desenvolvemento:

- **NXP/Freescale**, *KL46 Sub-Family Reference Manual* (`KL46P121M48SF4RM.pdf`,
  Rev. 3, 2013) — secciones **3.8 (Timer modules configuration)**,
  **5.7 (Clock distribution: PIT, TPM, LPTMR clocking)** e capítulos individuais
  para os layouts dos rexistros do PIT e UART0.
- **ARM Limited**, *ARMv6-M Architecture Reference Manual* (DDI 0419E) —
  comportamento exacto de `LSLS`/`BCC`/`EORS` e a especificación do carry flag.
  <https://developer.arm.com/documentation/ddi0419/latest/>
- **ARM Limited**, *Cortex-M0+ Devices Generic User Guide* (DUI 0662) —
  confirmación do conxunto de instrucións dispoñibles na variante M0+
  (ausencia de `RBIT`, `CLZ`, `UBFX`, `BFI`).
  <https://developer.arm.com/documentation/dui0662/latest/>
- **Bauer, Jens**, *"Arm Cortex-M0 assembly programming tips and tricks"*,
  ARM Community Blog (22 agosto 2016) — documenta o uso do carry flag con
  `LSLS`+`ADCS`/`SBCS` como técnica branchless.
- **Yiu, Joseph**, *The Definitive Guide to ARM Cortex-M0 and Cortex-M0+
  Processors*, 2ª ed., Newnes/Elsevier (2015), ISBN 978-0-12-803277-0 —
  diferenzas ARMv6-M vs ARMv7-M e detalle do pipeline para estimación de ciclos.
- **CMSIS** (`MKL46Z4.h`, `core_cm0plus.h`) — definicións de rexistros e
  bit-fields (`PIT_TCTRL_TEN_MASK`, `SIM_SCGC6_PIT_MASK`, `SIM_SOPT2_TPMSRC`, …)
  utilizadas no código.
