    .syntax unified
    .cpu cortex-m4
    .thumb

    .extern SVC_Handler
    .extern PendSV_Handler
    .extern SysTick_Handler
    .extern LPTMR0_IRQHandler
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
    /* External IRQ 58 is LPTMR0 on S32K148. */
    .rept 58
    .word Default_Handler
    .endr
    .word LPTMR0_IRQHandler
    .rept 181
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

    /* Disable the reset watchdog before the full SRAM ECC initialization.
     * This must remain in assembly because the initial MSP points into SRAM
     * whose ECC state has not been initialized yet. The sequence mirrors the
     * S32K148 SDK SystemInit watchdog unlock without using the stack. */
    ldr     r0, =0x40052004          /* WDOG->CNT */
    ldr     r1, =0xD928C520          /* FEATURE_WDOG_UNLOCK_VALUE */
    str     r1, [r0]
    ldr     r1, [r0]                  /* complete the unlock write */
    ldr     r0, =0x40052000          /* WDOG->CS */
    /* Use the SDK reset configuration exactly: CMD32EN, LPO clock,
     * watchdog disabled, and UPDATE enabled.  Do not set DBG here; the
     * reset value is the documented configuration used by SystemInit(). */
    ldr     r1, =0x00002520          /* CMD32EN | LPO | UPDATE, EN=0 */
    str     r1, [r0]
    ldr     r0, =0x40052008          /* WDOG->TOVAL */
    ldr     r1, =0x0000ffff
    str     r1, [r0]

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
