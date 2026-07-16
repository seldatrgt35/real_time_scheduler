#include "target.h"

#include <stddef.h>
#include <stdint.h>

#include "S32K148.h"
#include "assert_internal.h"

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

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    __disable_irq();
    g_rts_s32k148_fault_record.assertion_failed = 1u;
    for (;;)
    {
    }
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
    g_rts_s32k148_fault_record.mmfar = SCB->MMFAR;
    g_rts_s32k148_fault_record.bfar = SCB->BFAR;
    for (;;)
    {
    }
}

_Noreturn void rts_s32k148_reset_entry(void)
{
    /* The smoke image deliberately retains the S32K148 reset clock source. */
    __enable_irq();
    __DSB();
    __ISB();
    (void)main();
    rts_assert_fail("main returned", __FILE__, __LINE__);
    for (;;)
    {
    }
}
