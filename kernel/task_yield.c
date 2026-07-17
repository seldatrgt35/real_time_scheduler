#include "rts/rts_task.h"

#include <stdbool.h>

#include "assert_internal.h"
#include "port.h"
#include "kernel_lock.h"
#include "scheduler_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"
#include "scheduler_policy.h"

static bool rts_yield_current_is_coherent(const rts_kernel_state_t *kernel)
{
    const rts_tcb_t *current = rts_scheduler_current_get();

    return current != NULL && current->state == RTS_TASK_STATE_RUNNING &&
           rts_scheduler_current_is_valid() &&
           rts_policy_validate(current, true) &&
           rts_policy_pick_next() == current &&
           !kernel->switch_plan.active;
}

rts_status_t rts_task_yield(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_kernel_lock_token_t critical_token;
    rts_tcb_t *current;
    rts_tcb_t *selected;
    bool coherent;
    bool notify_port = false;

    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    critical_token = rts_kernel_lock_enter();
    if (kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    coherent = rts_yield_current_is_coherent(kernel);
    RTS_ASSERT(coherent);
    if (!coherent)
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    current = rts_scheduler_current_get();
    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.yields);
#endif
    RTS_TRACE(RTS_TRACE_YIELD, current->priority, 0u);
    if (!rts_policy_yield(current))
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_OK;
    }

    selected = rts_scheduler_select_highest_ready();
    RTS_ASSERT(selected != NULL);
    RTS_ASSERT(selected != current);
    RTS_ASSERT(selected == NULL || selected->state == RTS_TASK_STATE_READY);
    if (selected == NULL || selected == current ||
        selected->state != RTS_TASK_STATE_READY)
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    notify_port = rts_scheduler_prepare_switch(selected);
    rts_kernel_lock_exit(critical_token);

    if (notify_port)
    {
        rts_port_request_reschedule(rts_cpu_current_id());
    }
    return RTS_STATUS_OK;
}
