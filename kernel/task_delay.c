#include "rts/rts_task.h"

#include <stdbool.h>

#include "assert_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "time_internal.h"

static bool rts_delay_current_is_coherent(const rts_kernel_state_t *kernel)
{
    const rts_tcb_t *current = kernel->current_task;

    return current != NULL && current->state == RTS_TASK_STATE_RUNNING &&
           !rts_scheduler_task_is_idle(current) &&
           rts_scheduler_current_is_valid() &&
           rts_ready_is_front(&kernel->ready_set, current) &&
           current->wait.reason == RTS_WAIT_NONE &&
           current->wait.result == RTS_WAIT_RESULT_NONE &&
           current->wait.object == NULL && !current->wait.timeout_active &&
           current->wait_node.owner == NULL &&
           current->delay_node.owner == NULL &&
           !kernel->switch_plan.active;
}

rts_status_t rts_task_delay(rts_tick_t delay)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t critical_token;
    rts_tcb_t *current;
    rts_tcb_t *selected;
    bool coherent;
    bool notify_port;

    if (delay == 0u)
    {
        return rts_task_yield();
    }
    if (!rts_tick_relative_is_valid(delay))
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    critical_token = rts_port_critical_enter();
    if (kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    coherent = rts_delay_current_is_coherent(kernel);
    RTS_ASSERT(coherent);
    if (!coherent)
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    current = kernel->current_task;
    current->wait.wake_tick = kernel->current_tick + delay;
    rts_ready_remove(&kernel->ready_set, current);
    RTS_FATAL_UNLESS(!rts_ready_contains(&kernel->ready_set, current));

    current->wait.reason = RTS_WAIT_DELAY;
    current->wait.result = RTS_WAIT_RESULT_NONE;
    current->wait.object = NULL;
    current->wait.timeout_active = false;
    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    current->state = RTS_TASK_STATE_BLOCKED;
    rts_delay_insert(&kernel->delay_queue, current);
    RTS_FATAL_UNLESS(rts_scheduler_task_is_blocked_delay(current));

    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL);
    RTS_FATAL_UNLESS(selected == NULL || selected != current);
    if (selected == NULL || selected == current)
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    notify_port = rts_scheduler_prepare_switch(selected);
    RTS_FATAL_UNLESS(kernel->switch_plan.pending || kernel->switch_plan.active);
    rts_port_critical_exit(critical_token);

    if (notify_port)
    {
        rts_port_request_context_switch();
    }
    return RTS_STATUS_OK;
}
