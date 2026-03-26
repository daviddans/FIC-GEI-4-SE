    .syntax unified
    .cpu    cortex-m0plus
    .thumb

    .section .text
    .global reverse_int_asm
    .type   reverse_int_asm, %function
    .thumb_func

@ reverse_int_asm: inverte os 32 bits de r0, devolve resultado en r0.
@ Usa o truco LSRS+ADCS (carry flag) — mesma optimizacion que a version inline.
@ Convencion AAPCS: entrada r0, saida r0, usa r0-r2 (caller-saved, sen push/pop).

reverse_int_asm:
    movs    r1, #0              @ out = 0
    movs    r2, #32             @ contador de bits
.Lloop:
    lsrs    r0, r0, #1          @ in >>= 1, LSB -> carry flag
    adcs    r1, r1              @ out = (out << 1) | carry
    subs    r2, r2, #1          @ contador--
    bne     .Lloop
    movs    r0, r1              @ retornar out en r0
    bx      lr
    .size   reverse_int_asm, .-reverse_int_asm
