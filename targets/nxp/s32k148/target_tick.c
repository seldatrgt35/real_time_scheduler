#include "target_tick.h"

#include <stdbool.h>
#include <stdint.h>

#include "S32K148.h"
#include "assert_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "target_config.h"

typedef uint8_t rts_s32k148_tick_state_t;
enum
{
    RTS_S32K148_TICK_UNINITIALIZED = 0,
    RTS_S32K148_TICK_READY,
    RTS_S32K148_TICK_ARMED,
    RTS_S32K148_TICK_RUNNING
};

static rts_s32k148_tick_state_t rts_s32k148_tick_state;
static uint32_t rts_s32k148_tick_reload;

static uint8_t rts_s32k148_tick_priority_encode(void)
{
    return (uint8_t)(RTS_S32K148_SYSTICK_LOGICAL_PRIORITY <<
                     (8u - RTS_S32K148_NVIC_PRIORITY_BITS));
}

rts_status_t rts_s32k148_tick_initialize(uint32_t core_clock_hz,
                                         uint32_t tick_rate_hz)
{
    uint32_t reload;

    SysTick->CTRL = 0u;
    rts_s32k148_tick_state = RTS_S32K148_TICK_UNINITIALIZED;
    rts_s32k148_tick_reload = 0u;
    if (!rts_s32k148_tick_reload_calculate(core_clock_hz, tick_rate_hz,
                                           &reload))
    {
        return RTS_STATUS_PORT_ERROR;
    }

    SysTick->LOAD = reload;
    SysTick->VAL = 0u;
    SCB->SHP[11] = rts_s32k148_tick_priority_encode();
    __DSB();
    __ISB();
    if (SysTick->LOAD != reload ||
        SCB->SHP[11] != rts_s32k148_tick_priority_encode())
    {
        SysTick->CTRL = 0u;
        return RTS_STATUS_PORT_ERROR;
    }

    rts_s32k148_tick_reload = reload;
    rts_s32k148_tick_state = RTS_S32K148_TICK_READY;
    return RTS_STATUS_OK;
}

rts_status_t rts_port_tick_start(void)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    bool valid = rts_s32k148_tick_state == RTS_S32K148_TICK_READY &&
                 kernel->lifecycle == RTS_KERNEL_RUNNING &&
                 rts_scheduler_current_is_valid() &&
                 (__get_PRIMASK() & 1u) != 0u && __get_IPSR() == 0u;

    RTS_ASSERT(valid);
    if (!valid)
    {
        return RTS_STATUS_PORT_ERROR;
    }

    SysTick->VAL = 0u;
    rts_s32k148_tick_state = RTS_S32K148_TICK_ARMED;
    return RTS_STATUS_OK;
}

bool rts_port_tick_commit_start(void)
{
    bool valid = rts_s32k148_tick_state == RTS_S32K148_TICK_ARMED &&
                 (__get_PRIMASK() & 1u) != 0u && __get_IPSR() == 11u;

    RTS_FATAL_UNLESS(valid);
    if (!valid)
    {
        return false;
    }

    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
    __DSB();
    __ISB();
    valid = (SysTick->CTRL & (SysTick_CTRL_CLKSOURCE_Msk |
                              SysTick_CTRL_TICKINT_Msk |
                              SysTick_CTRL_ENABLE_Msk)) ==
            (SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
             SysTick_CTRL_ENABLE_Msk);
    RTS_FATAL_UNLESS(valid);
    if (valid)
    {
        rts_s32k148_tick_state = RTS_S32K148_TICK_RUNNING;
    }
    return valid;
}

void rts_port_tick_stop(void)
{
    SysTick->CTRL = 0u;
    SysTick->VAL = 0u;
    if (rts_s32k148_tick_state != RTS_S32K148_TICK_UNINITIALIZED)
    {
        rts_s32k148_tick_state = RTS_S32K148_TICK_READY;
    }
    __DSB();
    __ISB();
}

bool rts_s32k148_tick_is_ready(void)
{
    return rts_s32k148_tick_state == RTS_S32K148_TICK_READY;
}

bool rts_s32k148_tick_is_running(void)
{
    return rts_s32k148_tick_state == RTS_S32K148_TICK_RUNNING;
}

uint32_t rts_s32k148_tick_reload_get(void)
{
    return rts_s32k148_tick_reload;
}

__attribute__((weak)) bool rts_s32k148_tick_isr_hook(void)
{
    return false;
}

void SysTick_Handler(void)
{
    bool valid = rts_s32k148_tick_state == RTS_S32K148_TICK_RUNNING;
    bool notify_port;

    RTS_FATAL_UNLESS(valid);
    (void)SysTick->CTRL;
    notify_port = rts_kernel_tick_advance(UINT32_C(1));
    notify_port = rts_s32k148_tick_isr_hook() || notify_port;
    if (notify_port)
    {
        rts_port_request_reschedule(rts_cpu_current_id());
    }
}
