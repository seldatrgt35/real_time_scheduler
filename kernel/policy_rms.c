#include "policy_plugin_internal.h"

#include "scheduler_internal.h"

static void rts_policy_rms_assign_priorities(rts_tcb_t *new_task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t index;

    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        rts_tcb_t *task = &kernel->application_task_pool.slots[index];
        rts_priority_t rank = 1u;
        size_t other_index;
        bool included = task == new_task ||
                        task->slot_state == RTS_TASK_SLOT_ALLOCATED;

        if (!included)
        {
            continue;
        }
        for (other_index = 0u; other_index < (size_t)RTS_MAX_TASKS;
             ++other_index)
        {
            const rts_tcb_t *other =
                &kernel->application_task_pool.slots[other_index];
            bool other_included = other == new_task ||
                                  other->slot_state == RTS_TASK_SLOT_ALLOCATED;

            if (other_included && other->period > task->period)
            {
                ++rank;
            }
        }
        task->base_priority = rank;
        (void)rts_policy_fp_priority_changed(task, rank);
    }
}

void rts_policy_rms_initialize(void)
{
    rts_policy_fp_initialize();
}

bool rts_policy_rms_insert(rts_tcb_t *task)
{
    if (task != NULL && task->state == RTS_TASK_STATE_DORMANT &&
        task->period != 0u)
    {
        rts_policy_rms_assign_priorities(task);
    }
    return rts_policy_fp_insert(task);
}

bool rts_policy_rms_remove(rts_tcb_t *task)
{
    return rts_policy_fp_remove(task);
}

rts_tcb_t *rts_policy_rms_pick_next(void)
{
    return rts_policy_fp_pick_next();
}

bool rts_policy_rms_yield(rts_tcb_t *task)
{
    return rts_policy_fp_yield(task);
}

bool rts_policy_rms_priority_changed(rts_tcb_t *task,
                                     rts_priority_t effective_priority)
{
    return rts_policy_fp_priority_changed(task, effective_priority);
}

bool rts_policy_rms_tick(rts_tick_t elapsed_ticks)
{
    return rts_policy_fp_tick(elapsed_ticks);
}

bool rts_policy_rms_validate(const rts_tcb_t *task, bool expected_ready)
{
    size_t index;
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (!rts_policy_fp_validate(task, expected_ready))
    {
        return false;
    }
    if (task != NULL)
    {
        return task == kernel->idle_task || task == kernel->timer_service_task ||
               task->period != 0u;
    }
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        const rts_tcb_t *slot = &kernel->application_task_pool.slots[index];
        if (slot->slot_state == RTS_TASK_SLOT_ALLOCATED && slot->period == 0u)
        {
            return false;
        }
    }
    return true;
}
