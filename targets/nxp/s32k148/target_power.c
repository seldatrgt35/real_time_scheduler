#include "target_power.h"

#include <stdbool.h>
#include <stdint.h>

#include "S32K148.h"
#include "assert_internal.h"
#include "port.h"
#include "target_config.h"
#include "target_diagnostics.h"

static volatile bool rts_s32k148_lptmr_woke;
static bool rts_s32k148_power_ready;

__attribute__((weak)) uint32_t rts_s32k148_lptmr_count_read(void)
{
    LPTMR0->CNR = 0u;
    return LPTMR0->CNR;
}

rts_status_t rts_s32k148_power_initialize(void)
{
    PCC->PCCn[PCC_LPTMR0_INDEX] = PCC_PCCn_CGC_MASK;
    LPTMR0->CSR = 0u;
    LPTMR0->PSR = LPTMR_PSR_PCS(1u) | LPTMR_PSR_PBYP_MASK;
    LPTMR0->CMR = 0u;
    LPTMR0->CSR = LPTMR_CSR_TCF_MASK;
    NVIC_ClearPendingIRQ(LPTMR0_IRQn);
    NVIC_SetPriority(LPTMR0_IRQn, RTS_S32K148_SYSTICK_LOGICAL_PRIORITY);
    NVIC_EnableIRQ(LPTMR0_IRQn);
    __DSB();
    __ISB();

    rts_s32k148_lptmr_woke = false;
    rts_s32k148_power_ready =
        (PCC->PCCn[PCC_LPTMR0_INDEX] & PCC_PCCn_CGC_MASK) != 0u &&
        (LPTMR0->PSR & LPTMR_PSR_PBYP_MASK) != 0u;
    return rts_s32k148_power_ready ? RTS_STATUS_OK : RTS_STATUS_PORT_ERROR;
}

rts_port_sleep_result_t rts_port_power_sleep(rts_tick_t maximum_ticks)
{
    rts_port_sleep_result_t result = {
        .status = RTS_STATUS_PORT_ERROR,
        .elapsed_ticks = 0u,
        .wake_source = RTS_PORT_WAKE_OTHER
    };
    uint32_t systick_control;
    uint32_t elapsed;
    bool valid = rts_s32k148_power_ready && maximum_ticks != 0u &&
                 maximum_ticks <= UINT16_MAX && __get_IPSR() == 0u &&
                 (__get_PRIMASK() & 1u) != 0u;

    RTS_ASSERT(valid);
    if (!valid)
    {
        return result;
    }

    systick_control = SysTick->CTRL;
    SysTick->CTRL = 0u;
    rts_s32k148_lptmr_woke = false;
    LPTMR0->CSR = 0u;
    LPTMR0->CMR = maximum_ticks - 1u;
    LPTMR0->CSR = LPTMR_CSR_TCF_MASK;
    NVIC_ClearPendingIRQ(LPTMR0_IRQn);
    LPTMR0->CSR = LPTMR_CSR_TIE_MASK | LPTMR_CSR_TEN_MASK;
    __DSB();

    /* Close the final race in the target layer: unmask and sleep atomically. */
    rts_s32k148_timing_critical_end();
    __set_PRIMASK(0u);
    __WFI();
    __disable_irq();
    rts_s32k148_timing_critical_begin();

    elapsed = rts_s32k148_lptmr_woke
                  ? maximum_ticks
                  : rts_s32k148_lptmr_count_read();
    if (elapsed > maximum_ticks)
    {
        elapsed = maximum_ticks;
    }
    LPTMR0->CSR = 0u;
    LPTMR0->CSR = LPTMR_CSR_TCF_MASK;
    NVIC_ClearPendingIRQ(LPTMR0_IRQn);
    SysTick->VAL = 0u;
    SysTick->CTRL = systick_control;
    __DSB();
    __ISB();

    result.status = RTS_STATUS_OK;
    result.elapsed_ticks = elapsed;
    result.wake_source = rts_s32k148_lptmr_woke
                             ? RTS_PORT_WAKE_TIMER
                             : RTS_PORT_WAKE_EXTERNAL;
    return result;
}

void LPTMR0_IRQHandler(void)
{
    if ((LPTMR0->CSR & LPTMR_CSR_TCF_MASK) != 0u)
    {
        LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
        rts_s32k148_lptmr_woke = true;
    }
}
