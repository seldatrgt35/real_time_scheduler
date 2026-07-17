#include "port_offsets.h"

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .text.rts_cm4f_start_trigger, "ax", %progbits
    .align 2
    .global rts_cm4f_start_trigger
    .type rts_cm4f_start_trigger, %function
    .thumb_func
rts_cm4f_start_trigger:
    cpsie   i
    svc     #RTS_CM4F_SVC_START_NUMBER
    b       rts_cm4f_start_fatal
    .size rts_cm4f_start_trigger, .-rts_cm4f_start_trigger

    .section .text.rts_cm4f_pend_context_switch, "ax", %progbits
    .align 2
    .global rts_cm4f_pend_context_switch
    .type rts_cm4f_pend_context_switch, %function
    .thumb_func
rts_cm4f_pend_context_switch:
    ldr     r0, =RTS_CM4F_SCB_ICSR_ADDRESS
    ldr     r1, =RTS_CM4F_SCB_ICSR_PENDSVSET
    str     r1, [r0]
    dsb
    isb
    bx      lr
    .size rts_cm4f_pend_context_switch, .-rts_cm4f_pend_context_switch

    .section .text.SVC_Handler, "ax", %progbits
    .align 2
    .global SVC_Handler
    .type SVC_Handler, %function
    .thumb_func
SVC_Handler:
    cpsid   i

    /* Preserve 8-byte MSP alignment and exception LR across the C bridge. */
    push    {r0, lr}
    bl      rts_cm4f_start_handoff_consume
    pop     {r1, lr}
    cbz     r0, .Lsvc_fatal

    /* saved_sp addresses R4; the incremented value addresses hardware R0. */
    ldmia   r0!, {r4-r11}
    msr     psp, r0

    /* Privileged Thread mode, PSP selected, no FP-active context. */
    mrs     r1, control
    bic     r1, r1, #(RTS_CM4F_CONTROL_NPRIV_BIT | RTS_CM4F_CONTROL_FPCA_BIT)
    orr     r1, r1, #RTS_CM4F_CONTROL_SPSEL_BIT
    msr     control, r1
    isb

    /* Scheduler-aware sources remain disabled; first task enters unmasked. */
    cpsie   i
    ldr     lr, =RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC
    bx      lr

.Lsvc_fatal:
    b       rts_cm4f_start_fatal
    .size SVC_Handler, .-SVC_Handler

    .section .text.PendSV_Handler, "ax", %progbits
    .align 2
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .thumb_func
PendSV_Handler:
    cpsid   i
    ldr     r0, =RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC
    cmp     lr, r0
    bne     .Lpendsv_fatal

    /* Preserve outgoing task context before the first C bridge call. */
    mrs     r0, psp
    stmdb   r0!, {r4-r11}
    push    {r0, lr}

    /* R0 is outgoing saved SP; return R0 is immutable handoff or null. */
    bl      rts_cm4f_switch_bridge_acquire
    cbz     r0, .Lpendsv_no_handoff

    /* Publish outgoing SP and fetch incoming SP through verified TCB offset. */
    ldr     r1, [sp, #0]
    ldr     r2, [r0, #RTS_CM4F_HANDOFF_FROM_OFFSET]
    str     r1, [r2, #RTS_CM4F_TCB_SAVED_SP_OFFSET]
    ldr     r2, [r0, #RTS_CM4F_HANDOFF_TO_OFFSET]
    ldr     r2, [r2, #RTS_CM4F_TCB_SAVED_SP_OFFSET]
    str     r0, [sp, #0]

    /* Restore incoming callee-saved context and publish hardware-frame PSP. */
    ldmia   r2!, {r4-r11}
    msr     psp, r2

    /* AAPCS preserves restored R4-R11 across this ordinary-C completion call. */
    ldr     r0, [sp, #0]
    bl      rts_cm4f_switch_bridge_complete
    cbz     r0, .Lpendsv_fatal
    add     sp, sp, #8

    dsb
    isb
    cpsie   i
    ldr     lr, =RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC
    bx      lr

.Lpendsv_no_handoff:
    /* Distinguish an approved stale request from failed active acquisition. */
    bl      rts_cm4f_switch_bridge_no_plan
    cbz     r0, .Lpendsv_fatal
    pop     {r0, lr}
    cpsie   i
    bx      lr

.Lpendsv_fatal:
    b       rts_cm4f_start_fatal
    .size PendSV_Handler, .-PendSV_Handler

    .section .note.GNU-stack, "", %progbits
