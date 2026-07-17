#include "policy_plugin_internal.h"

#include "ready_queue.h"
#include "scheduler_internal.h"

void rts_policy_fp_initialize(void)
{
    rts_ready_initialize(&rts_kernel_state_get()->ready_set);
}

bool rts_policy_fp_insert(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (task == NULL || rts_ready_contains(&kernel->ready_set, task))
    {
        return false;
    }
    rts_ready_insert(&kernel->ready_set, task);
    return rts_ready_contains(&kernel->ready_set, task);
}

bool rts_policy_fp_remove(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (task == NULL || !rts_ready_contains(&kernel->ready_set, task))
    {
        return false;
    }
    rts_ready_remove(&kernel->ready_set, task);
    return !rts_ready_contains(&kernel->ready_set, task);
}

rts_tcb_t *rts_policy_fp_pick_next(void)
{
    return rts_ready_peek_highest(&rts_kernel_state_get()->ready_set);
}

bool rts_policy_fp_yield(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (task == NULL || !rts_ready_contains(&kernel->ready_set, task) ||
        !rts_ready_has_peer(&kernel->ready_set, task))
    {
        return false;
    }
    rts_ready_rotate(&kernel->ready_set, task->priority);
    return true;
}

bool rts_policy_fp_priority_changed(rts_tcb_t *task,
                                    rts_priority_t effective_priority)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    bool linked;

    if (task == NULL || effective_priority == RTS_IDLE_PRIORITY ||
        (size_t)effective_priority >= (size_t)RTS_PRIORITY_COUNT)
    {
        return false;
    }
    if (task->priority == effective_priority)
    {
        return true;
    }
    linked = rts_ready_contains(&kernel->ready_set, task);
    if (linked)
    {
        rts_ready_remove(&kernel->ready_set, task);
    }
    task->priority = effective_priority;
    if (linked)
    {
        rts_ready_insert(&kernel->ready_set, task);
    }
    return !linked || rts_ready_contains(&kernel->ready_set, task);
}

bool rts_policy_fp_tick(rts_tick_t elapsed_ticks)
{
#if RTS_ENABLE_TIME_SLICING
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *current = rts_scheduler_current_get();

    if (current == NULL || current->state != RTS_TASK_STATE_RUNNING ||
        current == kernel->idle_task || kernel->switch_plan.active ||
        !rts_ready_is_front(&kernel->ready_set, current) ||
        current->slice_remaining == 0u)
    {
        return false;
    }
    if (elapsed_ticks < current->slice_remaining)
    {
        current->slice_remaining -= elapsed_ticks;
        return false;
    }
    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    return rts_policy_fp_yield(current);
#else
    (void)elapsed_ticks;
    return false;
#endif
}

bool rts_policy_fp_validate(const rts_tcb_t *task, bool expected_ready)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (task == NULL)
    {
        return rts_ready_validate(&kernel->ready_set);
    }
    return rts_ready_contains(&kernel->ready_set, task) == expected_ready;
}
