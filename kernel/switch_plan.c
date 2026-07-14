#include "scheduler_internal.h"

#include "assert_internal.h"
#include "port.h"

static uint32_t rts_switch_next_generation(uint32_t generation)
{
    return generation + UINT32_C(1);
}

static void rts_switch_plan_clear(rts_switch_plan_t *plan)
{
    plan->from = NULL;
    plan->to = NULL;
    plan->pending = false;
    plan->active = false;
}

bool rts_scheduler_prepare_switch(rts_tcb_t *next_task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_plan_t *plan = &kernel->switch_plan;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_RUNNING);
    RTS_ASSERT(kernel->current_task != NULL);
    RTS_ASSERT(rts_scheduler_current_is_valid());
    RTS_ASSERT(next_task != NULL);
    RTS_ASSERT(next_task == NULL || rts_scheduler_task_is_runnable(next_task));
    if (kernel->lifecycle != RTS_KERNEL_RUNNING ||
        kernel->current_task == NULL || !rts_scheduler_current_is_valid() ||
        next_task == NULL || !rts_scheduler_task_is_runnable(next_task))
    {
        return false;
    }

    if (plan->active)
    {
        plan->reselection_required = true;
        return false;
    }

    plan->reselection_required = false;
    if (next_task == kernel->current_task)
    {
        if (plan->pending)
        {
            RTS_ASSERT(plan->from == kernel->current_task);
            plan->generation = rts_switch_next_generation(plan->generation);
            rts_switch_plan_clear(plan);
        }
        return false;
    }

    RTS_ASSERT(next_task->state == RTS_TASK_STATE_READY);
    if (next_task->state != RTS_TASK_STATE_READY)
    {
        return false;
    }

    if (!plan->pending)
    {
        plan->from = kernel->current_task;
        plan->to = next_task;
        plan->pending = true;
        plan->generation = rts_switch_next_generation(plan->generation);
        return true;
    }

    RTS_ASSERT(plan->from == kernel->current_task);
    RTS_ASSERT(plan->to != NULL);
    if (plan->from != kernel->current_task || plan->to == NULL)
    {
        return false;
    }

    if (plan->to != next_task)
    {
        plan->to = next_task;
        plan->generation = rts_switch_next_generation(plan->generation);
    }
    return false;
}

void rts_scheduler_request_switch_if_needed(rts_tcb_t *next_task)
{
    if (rts_scheduler_prepare_switch(next_task))
    {
        rts_port_request_context_switch();
    }
}

bool rts_scheduler_switch_acquire(rts_switch_snapshot_t *snapshot)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_plan_t *plan = &kernel->switch_plan;

    RTS_ASSERT(snapshot != NULL);
    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_RUNNING);
    RTS_ASSERT(!plan->active);
    if (snapshot == NULL || kernel->lifecycle != RTS_KERNEL_RUNNING ||
        plan->active)
    {
        return false;
    }
    if (!plan->pending)
    {
        return false;
    }

    RTS_ASSERT(plan->from == kernel->current_task);
    RTS_ASSERT(plan->from != NULL && plan->to != NULL);
    RTS_ASSERT(plan->from != plan->to);
    RTS_ASSERT(plan->from == NULL ||
               plan->from->state == RTS_TASK_STATE_RUNNING);
    RTS_ASSERT(plan->to == NULL || plan->to->state == RTS_TASK_STATE_READY);
    RTS_ASSERT(plan->from == NULL || rts_scheduler_task_is_runnable(plan->from));
    RTS_ASSERT(plan->to == NULL || rts_scheduler_task_is_runnable(plan->to));
    if (plan->from != kernel->current_task || plan->from == NULL ||
        plan->to == NULL || plan->from == plan->to ||
        plan->from->state != RTS_TASK_STATE_RUNNING ||
        plan->to->state != RTS_TASK_STATE_READY ||
        !rts_scheduler_task_is_runnable(plan->from) ||
        !rts_scheduler_task_is_runnable(plan->to))
    {
        return false;
    }

    snapshot->from = plan->from;
    snapshot->to = plan->to;
    snapshot->generation = plan->generation;
    plan->pending = false;
    plan->active = true;
    return true;
}

void rts_scheduler_switch_complete(const rts_switch_snapshot_t *snapshot)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_plan_t *plan = &kernel->switch_plan;
    bool valid;

    RTS_ASSERT(snapshot != NULL);
    valid = snapshot != NULL && kernel->lifecycle == RTS_KERNEL_RUNNING &&
            plan->active && !plan->pending &&
            snapshot->generation == plan->generation &&
            snapshot->from == plan->from && snapshot->to == plan->to &&
            snapshot->from != snapshot->to &&
            kernel->current_task == snapshot->from &&
            snapshot->from->state == RTS_TASK_STATE_RUNNING &&
            snapshot->to->state == RTS_TASK_STATE_READY &&
            rts_scheduler_task_is_runnable(snapshot->from) &&
            rts_scheduler_task_is_runnable(snapshot->to);
    RTS_ASSERT(valid);
    if (!valid)
    {
        return;
    }

    snapshot->from->state = RTS_TASK_STATE_READY;
    snapshot->to->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = snapshot->to;
    rts_switch_plan_clear(plan);
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
}

bool rts_scheduler_switch_reselection_required(void)
{
    return rts_kernel_state_get()->switch_plan.reselection_required;
}
