    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .text.NMI_Handler, "ax", %progbits
    .align 2
    .global NMI_Handler
    .type NMI_Handler, %function
    .thumb_func
NMI_Handler:
    mrs     r0, msp
    mrs     r1, ipsr
    push    {r2, lr}
    bl      rts_s32k148_record_handler_probe
    pop     {r2, lr}
    bx      lr
    .size NMI_Handler, .-NMI_Handler

    .section .text.HardFault_Handler, "ax", %progbits
    .align 2
    .global HardFault_Handler
    .type HardFault_Handler, %function
    .thumb_func
HardFault_Handler:
    mov     r1, lr
    mrs     r2, msp
    mrs     r3, psp
    tst     lr, #4
    ite     eq
    moveq   r0, r2
    movne   r0, r3
    b       rts_s32k148_hardfault_capture
    .size HardFault_Handler, .-HardFault_Handler

    .section .note.GNU-stack, "", %progbits
