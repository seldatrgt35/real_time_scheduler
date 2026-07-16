#include "scheduler_internal.h"

#include "assert_internal.h"
#include "port.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"
#include "stack_check_internal.h"
#include "fatal_internal.h"

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

static bool rts_switch_outgoing_is_valid(const rts_kernel_state_t *kernel,
                                         const rts_tcb_t *task)
{
    return task != NULL && task == kernel->current_task &&
           ((task->state == RTS_TASK_STATE_RUNNING &&
             rts_scheduler_task_is_runnable(task)) ||
            (task->state == RTS_TASK_STATE_BLOCKED &&
             (rts_scheduler_task_is_blocked_delay(task) ||
              rts_scheduler_task_is_blocked_wait(task))));
}

bool rts_scheduler_prepare_switch(rts_tcb_t *next_task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_plan_t *plan = &kernel->switch_plan;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_RUNNING);
    RTS_ASSERT(kernel->current_task != NULL);
    RTS_ASSERT(rts_switch_outgoing_is_valid(kernel, kernel->current_task));
    RTS_ASSERT(next_task != NULL);
    RTS_ASSERT(next_task == NULL || rts_scheduler_task_is_runnable(next_task));
    if (kernel->lifecycle != RTS_KERNEL_RUNNING ||
        kernel->current_task == NULL ||
        !rts_switch_outgoing_is_valid(kernel, kernel->current_task) ||
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
#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.switch_requests);
#endif
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
               rts_switch_outgoing_is_valid(kernel, plan->from));
    RTS_ASSERT(plan->to == NULL || plan->to->state == RTS_TASK_STATE_READY);
    RTS_ASSERT(plan->to == NULL || rts_scheduler_task_is_runnable(plan->to));
    if (plan->from != kernel->current_task || plan->from == NULL ||
        plan->to == NULL || plan->from == plan->to ||
        !rts_switch_outgoing_is_valid(kernel, plan->from) ||
        plan->to->state != RTS_TASK_STATE_READY ||
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
    if (snapshot != NULL &&
        (!rts_stack_guard_is_valid(snapshot->from) ||
         !rts_stack_guard_is_valid(snapshot->to) ||
         !rts_stack_saved_sp_is_valid(snapshot->from) ||
         !rts_stack_saved_sp_is_valid(snapshot->to)))
    {
        RTS_KERNEL_FATAL(RTS_FATAL_STACK_CORRUPTION, snapshot);
    }
    valid = snapshot != NULL && kernel->lifecycle == RTS_KERNEL_RUNNING &&
            plan->active && !plan->pending &&
            snapshot->generation == plan->generation &&
            snapshot->from == plan->from && snapshot->to == plan->to &&
            snapshot->from != snapshot->to &&
            kernel->current_task == snapshot->from &&
            rts_switch_outgoing_is_valid(kernel, snapshot->from) &&
            snapshot->to->state == RTS_TASK_STATE_READY &&
            rts_scheduler_task_is_runnable(snapshot->to);
    RTS_ASSERT(valid);
    if (!valid)
    {
        return;
    }

    rts_runtime_task_stopped(snapshot->from, kernel->current_tick);
    if (snapshot->from->state == RTS_TASK_STATE_RUNNING)
    {
        snapshot->from->state = RTS_TASK_STATE_READY;
    }
    else
    {
        RTS_FATAL_UNLESS(snapshot->from->state == RTS_TASK_STATE_BLOCKED);
    }
    snapshot->to->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    snapshot->to->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = snapshot->to;
    rts_runtime_task_started(snapshot->to, kernel->current_tick);
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.context_switches);
#endif
    RTS_TRACE(RTS_TRACE_TASK_SWITCHED,
              snapshot->from->priority, snapshot->to->priority);
    rts_switch_plan_clear(plan);
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
}

bool rts_scheduler_switch_reselection_required(void)
{
    return rts_kernel_state_get()->switch_plan.reselection_required;
}

bool rts_scheduler_reselect_after_switch(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *selected;
    rts_tcb_t *current;

    if (!kernel->switch_plan.reselection_required)
    {
        return false;
    }

    current = kernel->current_task;
    RTS_FATAL_UNLESS(current != NULL);
    RTS_FATAL_UNLESS(current == NULL || current->state == RTS_TASK_STATE_RUNNING);
    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL);
    if (current == NULL || selected == NULL)
    {
        return false;
    }

    if (selected != current)
    {
        return rts_scheduler_prepare_switch(selected);
    }

    (void)rts_scheduler_prepare_switch(current);
    return false;
}
