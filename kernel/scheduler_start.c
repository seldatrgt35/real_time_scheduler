#include "rts/rts.h"

#include <stdbool.h>

#include "assert_internal.h"
#include "port.h"
#include "scheduler_internal.h"

static bool rts_start_preflight_is_coherent(const rts_kernel_state_t *kernel)
{
    return kernel->lifecycle == RTS_KERNEL_INITIALIZED &&
           kernel->current_task == NULL && kernel->idle_task != NULL &&
           kernel->idle_task == &kernel->idle_task_storage &&
           kernel->idle_task->state == RTS_TASK_STATE_READY &&
           rts_ready_contains(&kernel->ready_set, kernel->idle_task) &&
           rts_ready_peek_highest(&kernel->ready_set) != NULL &&
           kernel->delay_queue.ordered_tasks.count == 0u &&
           !kernel->switch_plan.pending && !kernel->switch_plan.active &&
           kernel->switch_plan.from == NULL &&
           kernel->switch_plan.to == NULL;
}

static void rts_start_rollback(rts_kernel_state_t *kernel)
{
    kernel->lifecycle = RTS_KERNEL_INITIALIZED;
    RTS_FATAL_UNLESS(rts_scheduler_current_release_initial());
    RTS_FATAL_UNLESS(kernel->current_task == NULL);
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
}

rts_status_t rts_start(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t critical_token;
    rts_tcb_t *selected;
    rts_status_t port_status;
    bool coherent;

    if (rts_port_is_in_isr())
    {
        RTS_ASSERT(!rts_port_is_in_isr());
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (kernel->lifecycle == RTS_KERNEL_RESET)
    {
        return RTS_STATUS_INVALID_STATE;
    }
    if (kernel->lifecycle == RTS_KERNEL_RUNNING)
    {
        return RTS_STATUS_ALREADY_STARTED;
    }
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
        return RTS_STATUS_INVALID_STATE;
    }

    critical_token = rts_port_critical_enter();
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        rts_status_t status = kernel->lifecycle == RTS_KERNEL_RUNNING
                                  ? RTS_STATUS_ALREADY_STARTED
                                  : RTS_STATUS_INVALID_STATE;
        rts_port_critical_exit(critical_token);
        return status;
    }

    coherent = rts_start_preflight_is_coherent(kernel);
    RTS_ASSERT(coherent);
    if (!coherent)
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL);
    RTS_FATAL_UNLESS(selected == NULL || selected->state == RTS_TASK_STATE_READY);
    RTS_FATAL_UNLESS(selected == NULL ||
                     rts_scheduler_task_is_runnable(selected));
    if (selected == NULL || selected->state != RTS_TASK_STATE_READY ||
        !rts_scheduler_task_is_runnable(selected) ||
        !rts_scheduler_current_establish(selected))
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    kernel->lifecycle = RTS_KERNEL_RUNNING;
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());

    port_status = rts_port_tick_start();
    if (port_status != RTS_STATUS_OK)
    {
        rts_port_tick_stop();
        rts_start_rollback(kernel);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    port_status = rts_port_start_first_task();
    if (port_status != RTS_STATUS_OK)
    {
        rts_port_tick_stop();
        rts_start_rollback(kernel);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    /* Only the deterministic host port returns after simulated transfer. */
    rts_port_critical_exit(critical_token);
    return RTS_STATUS_OK;
}
