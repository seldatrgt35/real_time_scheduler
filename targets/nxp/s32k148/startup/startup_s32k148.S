    .syntax unified
    .cpu cortex-m4
    .thumb

    .extern SVC_Handler
    .extern PendSV_Handler
    .extern SysTick_Handler
    .extern rts_s32k148_reset_entry

    .section .isr_vector, "a", %progbits
    .align 10
    .global g_pfnVectors
    .type g_pfnVectors, %object
g_pfnVectors:
    .word __StackTop
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word Default_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .rept 240
    .word Default_Handler
    .endr
    .size g_pfnVectors, .-g_pfnVectors

    .section .flash_config, "a", %progbits
    .align 4
    .global g_s32k148_flash_config
g_s32k148_flash_config:
    .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    .byte 0xff, 0xff, 0xff, 0xff
    .byte 0xfe, 0xff, 0xff, 0xff

    .section .text.Reset_Handler, "ax", %progbits
    .align 2
    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    cpsid   i

    /* S32K148 SRAM ECC requires initialization with aligned word writes. */
    ldr     r0, =__ram_start
    ldr     r1, =__ram_end
    movs    r2, #0
.Lram_clear:
    cmp     r0, r1
    bhs     .Lram_clear_done
    str     r2, [r0], #4
    b       .Lram_clear
.Lram_clear_done:

    ldr     r0, =__data_load
    ldr     r1, =__data_start
    ldr     r2, =__data_end
.Ldata_copy:
    cmp     r1, r2
    bhs     .Ldata_done
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    b       .Ldata_copy
.Ldata_done:

    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    movs    r2, #0
.Lbss_clear:
    cmp     r0, r1
    bhs     .Lbss_done
    str     r2, [r0], #4
    b       .Lbss_clear
.Lbss_done:

    ldr     r0, =0xe000ed08
    ldr     r1, =g_pfnVectors
    str     r1, [r0]
    dsb
    isb
    b       rts_s32k148_reset_entry
    .size Reset_Handler, .-Reset_Handler

    .section .text.Default_Handler, "ax", %progbits
    .align 2
    .global Default_Handler
    .type Default_Handler, %function
    .thumb_func
Default_Handler:
    cpsid   i
    b       Default_Handler
    .size Default_Handler, .-Default_Handler

    .section .note.GNU-stack, "", %progbits
