#include "target_diagnostics.h"

#include "target_device.h"
#include "rts/rts_types.h"

volatile rts_s32k148_timing_record_t g_rts_s32k148_timing_record;

void rts_s32k148_timing_initialize(void)
{
#if RTS_ENABLE_DIAGNOSTICS
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    g_rts_s32k148_timing_record.cycle_counter_available =
        (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u ? 1u : 0u;
#endif
}

uint32_t rts_s32k148_cycle_now(void)
{
#if RTS_ENABLE_DIAGNOSTICS
    return DWT->CYCCNT;
#else
    return 0u;
#endif
}

void rts_s32k148_timing_critical_begin(void)
{
#if RTS_ENABLE_DIAGNOSTICS
    if (g_rts_s32k148_timing_record.cycle_counter_available != 0u)
    {
        g_rts_s32k148_timing_record.critical_start_cycle = DWT->CYCCNT;
    }
#endif
}

void rts_s32k148_timing_critical_end(void)
{
#if RTS_ENABLE_DIAGNOSTICS
    if (g_rts_s32k148_timing_record.cycle_counter_available != 0u)
    {
        uint32_t elapsed = DWT->CYCCNT -
            g_rts_s32k148_timing_record.critical_start_cycle;

        if (elapsed > g_rts_s32k148_timing_record.maximum_critical_cycles)
        {
            g_rts_s32k148_timing_record.maximum_critical_cycles = elapsed;
        }
    }
#endif
}
