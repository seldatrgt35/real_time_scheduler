    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .text.rts_smoke_verify_registers, "ax", %progbits
    .align 2
    .global rts_smoke_verify_registers
    .type rts_smoke_verify_registers, %function
    .thumb_func
rts_smoke_verify_registers:
    /* Preserve the caller's callee-saved state and keep PSP 8-byte aligned. */
    stmdb   sp!, {r4-r11}
    push    {r0, lr}
    ldmia   r0, {r4-r11}
    bl      rts_task_yield
    cmp     r0, #0
    bne     .Lyield_failure

    ldr     r0, [sp, #0]
    ldr     r1, [r0, #0]
    cmp     r4, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #4]
    cmp     r5, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #8]
    cmp     r6, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #12]
    cmp     r7, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #16]
    cmp     r8, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #20]
    cmp     r9, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #24]
    cmp     r10, r1
    bne     .Lregister_failure
    ldr     r1, [r0, #28]
    cmp     r11, r1
    bne     .Lregister_failure
    movs    r0, #0
    b       .Lregister_done

.Lregister_failure:
    movs    r0, #1
    b       .Lregister_done
.Lyield_failure:
    movs    r0, #2
.Lregister_done:
    pop     {r1, lr}
    ldmia   sp!, {r4-r11}
    bx      lr
    .size rts_smoke_verify_registers, .-rts_smoke_verify_registers

    .section .note.GNU-stack, "", %progbits
