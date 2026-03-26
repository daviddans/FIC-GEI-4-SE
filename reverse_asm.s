    .syntax unified
    .cpu    cortex-m0plus
    .thumb

    .section .text

@ ============================================================
@ reverse_int_asm: inverte todos os bits dun enteiro de 32 bits
@
@ Entrada:  r0 = valor a invertir
@ Saida:    r0 = valor invertido
@
@ Convención AAPCS: usamos r0-r2 (todos caller-saved),
@ non precisamos PUSH/POP.
@
@ Optimizacion: LSRS mete o LSB no carry flag, e ADCS Rd,Rd
@ calcula (Rd << 1) | carry. 2 instruccions por bit.
@ ============================================================

    .global reverse_int_asm
    .type   reverse_int_asm, %function
    .thumb_func

reverse_int_asm:
    movs    r1, #0              @ r1 = out = 0
    movs    r2, #32             @ r2 = contador de bits
.Lloop:
    lsrs    r0, r0, #1          @ in >>= 1, LSB -> carry flag
    adcs    r1, r1              @ out = (out << 1) | carry
    subs    r2, r2, #1          @ contador--
    bne     .Lloop              @ repetir se non e cero
    movs    r0, r1              @ mover resultado a r0 (valor de retorno)
    bx      lr                  @ retornar
    .size   reverse_int_asm, .-reverse_int_asm
