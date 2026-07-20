#include "target.h"

#include <stddef.h>
#include <stdint.h>

#include "target_device.h"
#include "assert_internal.h"
#include "fatal_internal.h"
#include "target_led.h"

extern int main(void);

rts_s32k148_fault_record_t g_rts_s32k148_fault_record;

void rts_s32k148_record_handler_probe(uint32_t msp, uint32_t ipsr)
{
    g_rts_s32k148_fault_record.handler_probe_msp = msp;
    g_rts_s32k148_fault_record.handler_probe_ipsr = ipsr;
}

void rts_s32k148_request_handler_probe(void)
{
    g_rts_s32k148_fault_record.handler_probe_msp = 0u;
    g_rts_s32k148_fault_record.handler_probe_ipsr = 0u;
    SCB->ICSR = SCB_ICSR_NMIPENDSET_Msk;
    __DSB();
    __ISB();
}

bool rts_s32k148_handler_probe_passed(void)
{
    return g_rts_s32k148_fault_record.handler_probe_msp != 0u &&
           (g_rts_s32k148_fault_record.handler_probe_msp & 7u) == 0u &&
           g_rts_s32k148_fault_record.handler_probe_ipsr == 2u;
}

void rts_target_fatal_hook(const rts_fatal_record_t *record)
{
    __disable_irq();
    g_rts_s32k148_fault_record.assertion_failed =
        record->reason == RTS_FATAL_ASSERTION ? 1u : 0u;
    g_rts_s32k148_fault_record.fatal_reason = record->reason;
    g_rts_s32k148_fault_record.fatal_tick = record->tick;
    g_rts_s32k148_fault_record.fatal_current_task = record->current_task;
}

_Noreturn void rts_s32k148_hardfault_capture(const uint32_t *stacked_frame,
                                             uint32_t exc_return,
                                             uint32_t active_msp,
                                             uint32_t active_psp)
{
    __disable_irq();
    g_rts_s32k148_fault_record.hardfault_seen = 1u;
    g_rts_s32k148_fault_record.exc_return = exc_return;
    g_rts_s32k148_fault_record.active_msp = active_msp;
    g_rts_s32k148_fault_record.active_psp = active_psp;
    if (stacked_frame != NULL)
    {
        g_rts_s32k148_fault_record.stacked_r0 = stacked_frame[0];
        g_rts_s32k148_fault_record.stacked_r1 = stacked_frame[1];
        g_rts_s32k148_fault_record.stacked_r2 = stacked_frame[2];
        g_rts_s32k148_fault_record.stacked_r3 = stacked_frame[3];
        g_rts_s32k148_fault_record.stacked_r12 = stacked_frame[4];
        g_rts_s32k148_fault_record.stacked_lr = stacked_frame[5];
        g_rts_s32k148_fault_record.stacked_pc = stacked_frame[6];
        g_rts_s32k148_fault_record.stacked_xpsr = stacked_frame[7];
    }
    g_rts_s32k148_fault_record.cfsr = SCB->CFSR;
    g_rts_s32k148_fault_record.hfsr = SCB->HFSR;
    g_rts_s32k148_fault_record.dfsr = SCB->DFSR;
    g_rts_s32k148_fault_record.shcsr = SCB->SHCSR;
    g_rts_s32k148_fault_record.mmfar = SCB->MMFAR;
    g_rts_s32k148_fault_record.bfar = SCB->BFAR;
    RTS_KERNEL_FATAL(RTS_FATAL_HARDFAULT, stacked_frame);
}

_Noreturn void rts_s32k148_reset_entry(void)
{
    /* The smoke image deliberately retains the S32K148 reset clock source. */
    rts_s32k148_led_initialize();
    __enable_irq();
    __DSB();
    __ISB();
    (void)main();
    RTS_KERNEL_FATAL(RTS_FATAL_TASK_RETURNED, NULL);
}
