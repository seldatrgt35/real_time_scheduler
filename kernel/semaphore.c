#include "rts/rts_semaphore.h"

#include <stdbool.h>

#include "assert_internal.h"
#include "port.h"
#include "semaphore_internal.h"
#include "time_internal.h"
#include "wait_object_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"
#include "scheduler_policy.h"

bool rts_semaphore_is_valid(const rts_semaphore_t *semaphore)
{
    return semaphore != NULL && semaphore->identity == semaphore &&
           semaphore->signature == RTS_SEMAPHORE_SIGNATURE &&
           semaphore->maximum_count > 0u &&
           semaphore->count <= semaphore->maximum_count &&
           rts_wait_object_validate(&semaphore->waiters);
}

rts_status_t rts_semaphore_init(rts_semaphore_t *semaphore,
                                rts_count_t initial_count,
                                rts_count_t maximum_count)
{
    if (semaphore == NULL || maximum_count == 0u ||
        initial_count > maximum_count)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (rts_semaphore_is_valid(semaphore))
    {
        return RTS_STATUS_ALREADY_INITIALIZED;
    }

    semaphore->count = initial_count;
    semaphore->maximum_count = maximum_count;
    rts_wait_object_initialize(&semaphore->waiters);
    semaphore->identity = semaphore;
    semaphore->signature = RTS_SEMAPHORE_SIGNATURE;
    RTS_FATAL_UNLESS(rts_semaphore_is_valid(semaphore));
    return RTS_STATUS_OK;
}

static bool rts_semaphore_current_can_block(const rts_kernel_state_t *kernel)
{
    const rts_tcb_t *current = kernel->current_task;

    return current != NULL && current->state == RTS_TASK_STATE_RUNNING &&
           !rts_scheduler_task_is_idle(current) &&
           rts_scheduler_current_is_valid() &&
           rts_policy_validate(current, true) &&
           current->wait.reason == RTS_WAIT_NONE &&
           current->wait.result == RTS_WAIT_RESULT_NONE &&
           current->wait.object == NULL && !current->wait.timeout_active &&
           current->delay_node.owner == NULL &&
           current->wait_node.owner == NULL && !kernel->switch_plan.active;
}

static void rts_semaphore_make_ready(rts_kernel_state_t *kernel,
                                     rts_tcb_t *task,
                                     rts_wait_result_t result)
{
    bool still_executing = task == kernel->current_task;

    RTS_FATAL_UNLESS(task->state == RTS_TASK_STATE_BLOCKED);
    RTS_FATAL_UNLESS(task->wait.reason == RTS_WAIT_SEMAPHORE);
    RTS_FATAL_UNLESS(task->wait_node.owner ==
                     &((rts_semaphore_t *)task->wait.object)->waiters);
    rts_wait_object_remove(
        &((rts_semaphore_t *)task->wait.object)->waiters, task);
    if (task->wait.timeout_active)
    {
        RTS_FATAL_UNLESS(rts_delay_contains(&kernel->delay_queue, task));
        rts_delay_remove(&kernel->delay_queue, task);
    }
    task->wait.reason = RTS_WAIT_NONE;
    task->wait.result = result;
    task->wait.wake_tick = 0u;
    task->wait.object = NULL;
    task->wait.timeout_active = false;
    task->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    task->state = still_executing ? RTS_TASK_STATE_RUNNING
                                  : RTS_TASK_STATE_READY;
    RTS_FATAL_UNLESS(rts_policy_task_unblock(task));
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(task->diagnostic_wake_count);
    if (result == RTS_WAIT_RESULT_ACQUIRED)
    {
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.semaphore_acquisitions);
    }
    else
    {
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.semaphore_timeouts);
    }
#endif
    RTS_TRACE(RTS_TRACE_SEMAPHORE, result, task->priority);
    RTS_FATAL_UNLESS(rts_scheduler_task_is_runnable(task));
}

static bool rts_semaphore_wake_one(rts_kernel_state_t *kernel,
                                   rts_semaphore_t *semaphore)
{
    rts_tcb_t *task = semaphore->waiters.head;

    if (task == NULL)
    {
        return false;
    }
    rts_semaphore_make_ready(kernel, task, RTS_WAIT_RESULT_ACQUIRED);
    return true;
}

static bool rts_semaphore_plan_after_wake(rts_kernel_state_t *kernel)
{
    rts_tcb_t *selected = rts_scheduler_select_highest_ready();
    rts_tcb_t *current = kernel->current_task;

    RTS_FATAL_UNLESS(selected != NULL);
    RTS_FATAL_UNLESS(current != NULL);
    if (selected == NULL || current == NULL)
    {
        return false;
    }
    if (selected == current)
    {
        (void)rts_scheduler_prepare_switch(current);
        return false;
    }
    if (current->state == RTS_TASK_STATE_BLOCKED || selected != current)
    {
        return rts_scheduler_prepare_switch(selected);
    }
    return false;
}

rts_status_t rts_semaphore_take(rts_semaphore_t *semaphore,
                                rts_tick_t timeout)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t token;
    rts_tcb_t *current;
    rts_tcb_t *selected;
    bool notify_port;

    if (semaphore == NULL ||
        (timeout > RTS_DELAY_MAX && timeout != RTS_WAIT_FOREVER))
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

    token = rts_port_critical_enter();
    if (!rts_semaphore_is_valid(semaphore))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (semaphore->count > 0u)
    {
        --semaphore->count;
#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(
            kernel->runtime_counters.semaphore_acquisitions);
#endif
        RTS_TRACE(RTS_TRACE_SEMAPHORE,
                  RTS_WAIT_RESULT_ACQUIRED, semaphore->count);
        rts_port_critical_exit(token);
        return RTS_STATUS_OK;
    }
    if (timeout == 0u)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_TIMEOUT;
    }
    if (rts_scheduler_task_is_timer_service(kernel->current_task))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (kernel->lifecycle != RTS_KERNEL_RUNNING ||
        !rts_semaphore_current_can_block(kernel))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }

    current = kernel->current_task;
    RTS_FATAL_UNLESS(rts_policy_task_block(current));
    current->wait.reason = RTS_WAIT_SEMAPHORE;
    current->wait.result = RTS_WAIT_RESULT_NONE;
    current->wait.object = semaphore;
    current->wait.timeout_active = timeout != RTS_WAIT_FOREVER;
    current->wait.wake_tick = current->wait.timeout_active
                                  ? kernel->current_tick + timeout
                                  : 0u;
    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    current->state = RTS_TASK_STATE_BLOCKED;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.semaphore_blocks);
    RTS_DIAG_COUNTER_INC(current->diagnostic_block_count);
#endif
    rts_wait_object_insert(&semaphore->waiters, current);
    if (current->wait.timeout_active)
    {
        rts_delay_insert(&kernel->delay_queue, current);
    }
    RTS_FATAL_UNLESS(rts_scheduler_task_is_blocked_wait(current));

    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL && selected != current);
    notify_port = selected != NULL && selected != current &&
                  rts_scheduler_prepare_switch(selected);
    rts_port_critical_exit(token);
    if (notify_port)
    {
        rts_port_request_context_switch();
    }

    if (current->wait.result != RTS_WAIT_RESULT_NONE)
    {
        return rts_semaphore_wait_result_consume(current);
    }
    /* The non-executing host port returns here while the task remains blocked. */
    return RTS_STATUS_OK;
}

static rts_status_t rts_semaphore_give_common(rts_semaphore_t *semaphore,
                                              bool from_isr,
                                              bool *notify_port)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t token;
    bool woke;

    *notify_port = false;
    if (kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        return RTS_STATUS_INVALID_STATE;
    }
    token = rts_port_critical_enter();
    if (!rts_semaphore_is_valid(semaphore))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    woke = rts_semaphore_wake_one(kernel, semaphore);
    if (!woke)
    {
        if (semaphore->count == semaphore->maximum_count)
        {
            rts_port_critical_exit(token);
            return RTS_STATUS_FULL;
        }
        ++semaphore->count;
    }
    else
    {
        *notify_port = rts_semaphore_plan_after_wake(kernel);
    }
    rts_port_critical_exit(token);
    (void)from_isr;
    return RTS_STATUS_OK;
}

rts_status_t rts_semaphore_give(rts_semaphore_t *semaphore)
{
    bool notify_port;
    rts_status_t status;

    if (semaphore == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    status = rts_semaphore_give_common(semaphore, false, &notify_port);
    if (status == RTS_STATUS_OK && notify_port)
    {
        rts_port_request_context_switch();
    }
    return status;
}

rts_status_t rts_semaphore_give_from_isr(
    rts_semaphore_t *semaphore,
    bool *higher_priority_task_woken)
{
    bool notify_port;
    rts_status_t status;

    if (semaphore == NULL || higher_priority_task_woken == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    *higher_priority_task_woken = false;
    if (!rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    status = rts_semaphore_give_common(semaphore, true, &notify_port);
    if (status == RTS_STATUS_OK)
    {
        *higher_priority_task_woken = notify_port;
    }
    return status;
}

bool rts_semaphore_timeout_task(rts_kernel_state_t *kernel,
                                rts_tcb_t *task)
{
    if (kernel == NULL || task == NULL ||
        !rts_scheduler_task_is_blocked_wait(task) ||
        !task->wait.timeout_active)
    {
        return false;
    }
    rts_semaphore_make_ready(kernel, task, RTS_WAIT_RESULT_TIMEOUT);
    return true;
}

rts_status_t rts_semaphore_wait_result_consume(rts_tcb_t *task)
{
    rts_status_t status;

    if (task == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (task->wait.result == RTS_WAIT_RESULT_ACQUIRED)
    {
        status = RTS_STATUS_OK;
    }
    else if (task->wait.result == RTS_WAIT_RESULT_TIMEOUT)
    {
        status = RTS_STATUS_TIMEOUT;
    }
    else
    {
        return RTS_STATUS_INVALID_STATE;
    }
    task->wait.result = RTS_WAIT_RESULT_NONE;
    return status;
}
