    .syntax unified
    .cpu    cortex-m0plus
    .thumb

    .section .text
    .global crc8_asm
    .type   crc8_asm, %function
    .thumb_func

@ crc8_asm(const uint8_t *data r0, size_t len r1) -> uint8_t en r0
@ CRC-8/CCITT POLY=0x07, INIT=0x00
@
@ Optimización: o CRC mantense nos bits 24..31 do rexistro durante todo
@ o procesamento. Así, LSLS r,r,#1 deixa o MSB do CRC no carry flag de
@ forma natural, sen precisar UXTB no bucle interno (os bits inferiores
@ permanecen sempre a cero porque LSLS recheea con 0).
@
@ AAPCS: r0-r3 caller-saved; r4,r5 callee-saved → push/pop.

crc8_asm:
    push    {r4, r5, lr}
    movs    r2, #0              @ crc inicializado a 0; vivirá en bits 24..31
    movs    r3, #0x07           @ POLY = 0x07
    lsls    r3, r3, #24         @ r3 = POLY << 24 = 0x07000000 (pre-deslocado)
    cmp     r1, #0              @ tratamento de buffer baleiro
    beq     .Ldone

.Lbyte:
    ldrb    r4, [r0]            @ r4 = *data (bits 0..7)
    adds    r0, r0, #1          @ data++
    subs    r1, r1, #1          @ len--
    lsls    r4, r4, #24         @ byte << 24 → posición do top byte
    eors    r2, r4              @ crc ^= byte (no top byte)
    movs    r5, #8              @ contador: 8 bits por byte

.Lbit:
    lsls    r2, r2, #1          @ desprazar; bit 31 (== MSB do CRC) → carry
    bcc     .Lno_xor            @ se carry=0 (MSB era 0), saltar o XOR
    eors    r2, r3              @ crc ^= POLY (ambos en bits 24..31)
.Lno_xor:
    subs    r5, r5, #1
    bne     .Lbit               @ saída cando se procesaron os 8 bits

    cmp     r1, #0
    bne     .Lbyte

.Ldone:
    lsrs    r0, r2, #24         @ recuperar o crc desde bits 24..31 → 0..7
    pop     {r4, r5, pc}

    .size   crc8_asm, .-crc8_asm
