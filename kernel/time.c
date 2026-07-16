#include "time_internal.h"

#include <stdbool.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "semaphore_internal.h"
#include "mutex_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"

rts_tick_t rts_kernel_tick_now(void)
{
    return rts_kernel_state_get()->current_tick;
}

static bool rts_tick_expired_task_is_valid(const rts_kernel_state_t *kernel,
                                           const rts_tcb_t *task)
{
    return task != NULL && task != kernel->idle_task &&
           (rts_scheduler_task_is_blocked_delay(task) ||
            (rts_scheduler_task_is_blocked_wait(task) &&
             task->wait.timeout_active));
}

static bool rts_tick_wake_task(rts_kernel_state_t *kernel, rts_tcb_t *task)
{
    bool valid = rts_tick_expired_task_is_valid(kernel, task);
    bool still_executing = task == kernel->current_task;

    RTS_FATAL_UNLESS(valid);
    if (!valid)
    {
        return false;
    }

    if (task->wait.reason == RTS_WAIT_SEMAPHORE)
    {
        return rts_semaphore_timeout_task(kernel, task);
    }
    if (task->wait.reason == RTS_WAIT_MUTEX)
    {
        return rts_mutex_timeout_task(kernel, task);
    }

    rts_delay_remove(&kernel->delay_queue, task);
    RTS_FATAL_UNLESS(task->delay_node.owner == NULL);
    task->wait.reason = RTS_WAIT_NONE;
    task->wait.result = RTS_WAIT_RESULT_NONE;
    task->wait.wake_tick = 0u;
    task->wait.object = NULL;
    task->wait.timeout_active = false;
    task->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    task->state = still_executing ? RTS_TASK_STATE_RUNNING
                                  : RTS_TASK_STATE_READY;
    rts_ready_insert(&kernel->ready_set, task);
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.delay_wakeups);
    RTS_DIAG_COUNTER_INC(task->diagnostic_wake_count);
#endif
    RTS_TRACE(RTS_TRACE_TASK_WOKE, RTS_WAIT_DELAY, task->priority);
    RTS_FATAL_UNLESS(rts_scheduler_task_is_runnable(task));
    return rts_scheduler_task_is_runnable(task);
}

static bool rts_tick_account_current_slice(rts_kernel_state_t *kernel,
                                           rts_tick_t elapsed_ticks)
{
#if RTS_ENABLE_TIME_SLICING
    rts_tcb_t *current = kernel->current_task;
    bool coherent;

    if (current == NULL || current->state != RTS_TASK_STATE_RUNNING ||
        rts_scheduler_task_is_idle(current) || kernel->switch_plan.active ||
        !rts_ready_is_front(&kernel->ready_set, current))
    {
        return false;
    }

    coherent = rts_scheduler_current_is_valid() &&
               current->slice_remaining > 0u;
    RTS_FATAL_UNLESS(coherent);
    if (!coherent)
    {
        return false;
    }

    if (elapsed_ticks < current->slice_remaining)
    {
        current->slice_remaining -= elapsed_ticks;
        return false;
    }

    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    if (!rts_ready_has_peer(&kernel->ready_set, current))
    {
        return false;
    }

    rts_ready_rotate(&kernel->ready_set, current->priority);
    return true;
#else
    (void)kernel;
    (void)elapsed_ticks;
    return false;
#endif
}

bool rts_kernel_tick_advance(rts_tick_t elapsed_ticks)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *expired;
    rts_tcb_t *selected;
    rts_tcb_t *current;
    bool woke_task = false;
    bool current_woke = false;
    bool slice_rotated;
    bool valid = kernel->lifecycle == RTS_KERNEL_RUNNING &&
                 rts_port_is_in_isr() && elapsed_ticks != 0u &&
                 elapsed_ticks <= RTS_TICK_MAX_ADVANCE;

    RTS_ASSERT(valid);
    if (!valid)
    {
        return false;
    }

    kernel->current_tick += elapsed_ticks;
#if RTS_ENABLE_RUNTIME_STATS
    kernel->runtime_counters.scheduler_ticks =
        rts_diagnostic_counter_add(kernel->runtime_counters.scheduler_ticks,
                                   elapsed_ticks);
#endif
    RTS_TRACE(RTS_TRACE_TICK, elapsed_ticks, kernel->current_tick);
    for (;;)
    {
        expired = rts_delay_peek_expired(&kernel->delay_queue,
                                         kernel->current_tick);
        if (expired == NULL)
        {
            break;
        }
        if (expired == kernel->current_task)
        {
            current_woke = true;
        }
        if (!rts_tick_wake_task(kernel, expired))
        {
            return false;
        }
        woke_task = true;
    }

    slice_rotated = !current_woke &&
                    rts_tick_account_current_slice(kernel, elapsed_ticks);

    if (!woke_task && !slice_rotated)
    {
        return false;
    }

    current = kernel->current_task;
    RTS_FATAL_UNLESS(current != NULL);
    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL);
    if (current == NULL || selected == NULL)
    {
        return false;
    }

    if (selected == current)
    {
        (void)rts_scheduler_prepare_switch(current);
        return false;
    }

    if (current->state == RTS_TASK_STATE_BLOCKED)
    {
        RTS_FATAL_UNLESS(rts_scheduler_task_is_blocked_delay(current) ||
                         rts_scheduler_task_is_blocked_wait(current));
        return rts_scheduler_prepare_switch(selected);
    }

    RTS_FATAL_UNLESS(current->state == RTS_TASK_STATE_RUNNING);
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
    if (current->state != RTS_TASK_STATE_RUNNING ||
        !rts_scheduler_current_is_valid() ||
        selected->priority < current->priority ||
        (selected->priority == current->priority && !slice_rotated &&
         rts_ready_is_front(&kernel->ready_set, current)))
    {
        return false;
    }

    return rts_scheduler_prepare_switch(selected);
}
