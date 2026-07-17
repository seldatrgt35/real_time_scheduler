#include <stdbool.h>
#include <stdint.h>

#include "S32K148.h"
#include "port.h"
#include "scheduler_internal.h"
#include "target_config.h"
#include "target_diagnostics.h"
#include "target_tick.h"

static int test_failures;
static unsigned int assertion_count;
static unsigned int tick_call_count;
static rts_tick_t last_elapsed;
static bool tick_notification_required;
static unsigned int switch_request_count;
static bool current_valid;
static rts_kernel_state_t test_kernel;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

rts_kernel_state_t *rts_kernel_state_get(void)
{
    return &test_kernel;
}

bool rts_scheduler_current_is_valid(void)
{
    return current_valid;
}

bool rts_kernel_tick_advance(rts_tick_t elapsed_ticks)
{
    ++tick_call_count;
    last_elapsed = elapsed_ticks;
    return tick_notification_required;
}

void rts_port_request_reschedule(rts_cpu_id_t cpu)
{
    CHECK(rts_cpu_id_is_valid(cpu));
    ++switch_request_count;
}

static void reset_environment(void)
{
    rts_test_systick = (SysTick_Type){0};
    rts_test_scb = (SCB_Type){0};
    rts_test_dwt = (DWT_Type){0};
    rts_test_core_debug = (CoreDebug_Type){0};
    g_rts_s32k148_timing_record = (rts_s32k148_timing_record_t){0};
    rts_test_primask = 1u;
    rts_test_ipsr = 0u;
    rts_test_dsb_count = 0u;
    rts_test_isb_count = 0u;
    test_kernel = (rts_kernel_state_t){0};
    current_valid = true;
    tick_call_count = 0u;
    last_elapsed = 0u;
    tick_notification_required = false;
    switch_request_count = 0u;
}

static void test_dwt_critical_window_measurement(void)
{
    reset_environment();
    rts_s32k148_timing_initialize();
    CHECK(g_rts_s32k148_timing_record.cycle_counter_available == 1u);
    CHECK((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0u);
    DWT->CYCCNT = 100u;
    rts_s32k148_timing_critical_begin();
    DWT->CYCCNT = 137u;
    rts_s32k148_timing_critical_end();
    CHECK(g_rts_s32k148_timing_record.maximum_critical_cycles == 37u);
    DWT->CYCCNT = 200u;
    rts_s32k148_timing_critical_begin();
    DWT->CYCCNT = 205u;
    rts_s32k148_timing_critical_end();
    CHECK(g_rts_s32k148_timing_record.maximum_critical_cycles == 37u);
}

static void test_initialize_and_start_ordering(void)
{
    reset_environment();
    CHECK(rts_s32k148_tick_initialize(UINT32_C(48000000), 1000u) ==
          RTS_STATUS_OK);
    CHECK(rts_s32k148_tick_is_ready());
    CHECK(!rts_s32k148_tick_is_running());
    CHECK(SysTick->CTRL == 0u);
    CHECK(SysTick->LOAD == UINT32_C(47999));
    CHECK(rts_s32k148_tick_reload_get() == UINT32_C(47999));
    CHECK(SCB->SHP[11] == UINT8_C(0xe0));

    test_kernel.lifecycle = RTS_KERNEL_RUNNING;
    CHECK(rts_port_tick_start() == RTS_STATUS_OK);
    CHECK(!rts_s32k148_tick_is_running());
    CHECK(SysTick->CTRL == 0u);

    rts_test_ipsr = 11u;
    CHECK(rts_port_tick_commit_start());
    CHECK(rts_s32k148_tick_is_running());
    CHECK((SysTick->CTRL & (SysTick_CTRL_ENABLE_Msk |
                            SysTick_CTRL_TICKINT_Msk |
                            SysTick_CTRL_CLKSOURCE_Msk)) ==
          (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk |
           SysTick_CTRL_CLKSOURCE_Msk));

    rts_test_ipsr = 15u;
    tick_notification_required = true;
    SysTick_Handler();
    CHECK(tick_call_count == 1u);
    CHECK(last_elapsed == 1u);
    CHECK(switch_request_count == 1u);

    rts_port_tick_stop();
    CHECK(rts_s32k148_tick_is_ready());
    CHECK(!rts_s32k148_tick_is_running());
    CHECK(SysTick->CTRL == 0u);
}

static void test_invalid_start_context(void)
{
    unsigned int before;

    reset_environment();
    CHECK(rts_s32k148_tick_initialize(UINT32_C(48000000), 1000u) ==
          RTS_STATUS_OK);
    before = assertion_count;
    CHECK(rts_port_tick_start() == RTS_STATUS_PORT_ERROR);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    CHECK(assertion_count == before);
#endif
    CHECK(rts_s32k148_tick_is_ready());
    CHECK(SysTick->CTRL == 0u);
}

static void test_initialization_failure_is_disabled(void)
{
    reset_environment();
    SysTick->CTRL = UINT32_MAX;
    CHECK(rts_s32k148_tick_initialize(UINT32_C(48000000), 1001u) ==
          RTS_STATUS_PORT_ERROR);
    CHECK(!rts_s32k148_tick_is_ready());
    CHECK(!rts_s32k148_tick_is_running());
    CHECK(rts_s32k148_tick_reload_get() == 0u);
    CHECK(SysTick->CTRL == 0u);
}

int main(void)
{
    test_initialize_and_start_ordering();
    test_dwt_critical_window_measurement();
    test_invalid_start_context();
    test_initialization_failure_is_disabled();
    return test_failures;
}
