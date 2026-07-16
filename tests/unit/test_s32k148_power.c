#include <stdint.h>

#include "S32K148.h"
#include "port.h"
#include "target_diagnostics.h"
#include "target_power.h"

static int failures;

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++failures;
}

static void timer_wake(void)
{
    LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
    LPTMR0_IRQHandler();
}

static void external_wake(void)
{
    LPTMR0->CNR = 7u;
}

uint32_t rts_s32k148_lptmr_count_read(void)
{
    return LPTMR0->CNR;
}

int main(void)
{
    rts_port_sleep_result_t result;

    rts_test_primask = 1u;
    CHECK(rts_s32k148_power_initialize() == RTS_STATUS_OK);
    CHECK((PCC->PCCn[PCC_LPTMR0_INDEX] & PCC_PCCn_CGC_MASK) != 0u);
    CHECK(LPTMR0->PSR ==
          (LPTMR_PSR_PCS(1u) | LPTMR_PSR_PBYP_MASK));
    CHECK((rts_test_nvic_enabled[(uint32_t)LPTMR0_IRQn / 32u] &
           (UINT32_C(1) << ((uint32_t)LPTMR0_IRQn % 32u))) != 0u);

    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    rts_test_wfi_hook = timer_wake;
    result = rts_port_power_sleep(25u);
    CHECK(result.status == RTS_STATUS_OK);
    CHECK(result.elapsed_ticks == 25u);
    CHECK(result.wake_source == RTS_PORT_WAKE_TIMER);
    CHECK(rts_test_wfi_count == 1u);
    CHECK(SysTick->CTRL ==
          (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk));
    CHECK(rts_test_primask == 1u);

    rts_test_wfi_hook = external_wake;
    result = rts_port_power_sleep(25u);
    CHECK(result.status == RTS_STATUS_OK);
    CHECK(result.elapsed_ticks == 7u);
    CHECK(result.wake_source == RTS_PORT_WAKE_EXTERNAL);
    return failures;
}
