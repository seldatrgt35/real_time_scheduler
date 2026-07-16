#include "rts/rts_mutex.h"

#include <stdbool.h>
#include <stddef.h>

#include "assert_internal.h"
#include "mutex_internal.h"
#include "port.h"
#include "priority_internal.h"
#include "time_internal.h"
#include "wait_object_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"

bool rts_mutex_is_valid(const rts_mutex_t *mutex)
{
    bool ownership_valid;

    if (mutex == NULL || mutex->identity != mutex ||
        mutex->signature != RTS_MUTEX_SIGNATURE ||
        !rts_wait_object_validate(&mutex->waiters))
    {
        return false;
    }
    ownership_valid = mutex->owner == NULL
                          ? mutex->owned_previous == NULL &&
                                mutex->owned_next == NULL &&
                                mutex->waiters.count == 0u
                          : mutex->owner->owned_mutex_head == mutex ||
                                mutex->owned_previous != NULL;
    return ownership_valid;
}

rts_status_t rts_mutex_init(rts_mutex_t *mutex)
{
    if (mutex == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (rts_mutex_is_valid(mutex))
    {
        return RTS_STATUS_ALREADY_INITIALIZED;
    }
    mutex->owner = NULL;
    rts_wait_object_initialize(&mutex->waiters);
    mutex->owned_previous = NULL;
    mutex->owned_next = NULL;
    mutex->identity = mutex;
    mutex->signature = RTS_MUTEX_SIGNATURE;
    RTS_FATAL_UNLESS(rts_mutex_is_valid(mutex));
    return RTS_STATUS_OK;
}

static void rts_mutex_owner_link(rts_mutex_t *mutex, rts_tcb_t *owner)
{
    RTS_FATAL_UNLESS(mutex->owner == NULL);
    RTS_FATAL_UNLESS(mutex->owned_previous == NULL &&
                     mutex->owned_next == NULL);
    RTS_FATAL_UNLESS(owner->owned_mutex_count <
                     (size_t)RTS_MAX_MUTEXES_PER_TASK);
    mutex->owner = owner;
    mutex->owned_previous = owner->owned_mutex_tail;
    mutex->owned_next = NULL;
    if (owner->owned_mutex_tail == NULL)
    {
        owner->owned_mutex_head = mutex;
    }
    else
    {
        owner->owned_mutex_tail->owned_next = mutex;
    }
    owner->owned_mutex_tail = mutex;
    ++owner->owned_mutex_count;
}

static void rts_mutex_owner_unlink(rts_mutex_t *mutex)
{
    rts_tcb_t *owner = mutex->owner;

    RTS_FATAL_UNLESS(owner != NULL);
    if (mutex->owned_previous == NULL)
    {
        RTS_FATAL_UNLESS(owner->owned_mutex_head == mutex);
        owner->owned_mutex_head = mutex->owned_next;
    }
    else
    {
        mutex->owned_previous->owned_next = mutex->owned_next;
    }
    if (mutex->owned_next == NULL)
    {
        RTS_FATAL_UNLESS(owner->owned_mutex_tail == mutex);
        owner->owned_mutex_tail = mutex->owned_previous;
    }
    else
    {
        mutex->owned_next->owned_previous = mutex->owned_previous;
    }
    mutex->owner = NULL;
    mutex->owned_previous = NULL;
    mutex->owned_next = NULL;
    RTS_FATAL_UNLESS(owner->owned_mutex_count > 0u);
    --owner->owned_mutex_count;
}

static bool rts_mutex_current_can_block(const rts_kernel_state_t *kernel)
{
    const rts_tcb_t *current = kernel->current_task;

    return current != NULL && current->state == RTS_TASK_STATE_RUNNING &&
           !rts_scheduler_task_is_idle(current) &&
           rts_scheduler_current_is_valid() &&
           rts_ready_is_front(&kernel->ready_set, current) &&
           current->wait.reason == RTS_WAIT_NONE &&
           current->wait.result == RTS_WAIT_RESULT_NONE &&
           current->wait.object == NULL && !current->wait.timeout_active &&
           current->delay_node.owner == NULL &&
           current->wait_node.owner == NULL && !kernel->switch_plan.active;
}

static bool rts_mutex_would_cycle(const rts_tcb_t *current,
                                  const rts_mutex_t *mutex)
{
    const rts_tcb_t *task = mutex->owner;
    size_t depth;

    for (depth = 0u; task != NULL && depth < (size_t)RTS_MAX_TASKS; ++depth)
    {
        if (task == current)
        {
            return true;
        }
        if (task->state != RTS_TASK_STATE_BLOCKED ||
            task->wait.reason != RTS_WAIT_MUTEX)
        {
            return false;
        }
        task = ((const rts_mutex_t *)task->wait.object)->owner;
    }
    RTS_FATAL_UNLESS(task == NULL);
    return task != NULL;
}

static bool rts_mutex_plan_final(rts_kernel_state_t *kernel)
{
    rts_tcb_t *selected = rts_scheduler_select_highest_ready();
    rts_tcb_t *current = kernel->current_task;

    RTS_FATAL_UNLESS(selected != NULL && current != NULL);
    if (selected == NULL || current == NULL)
    {
        return false;
    }
    if (selected == current)
    {
        (void)rts_scheduler_prepare_switch(current);
        return false;
    }
    if (current->state == RTS_TASK_STATE_BLOCKED ||
        selected->priority > current->priority ||
        (selected->priority == current->priority &&
         !rts_ready_is_front(&kernel->ready_set, current)))
    {
        return rts_scheduler_prepare_switch(selected);
    }
    return false;
}

rts_status_t rts_mutex_lock(rts_mutex_t *mutex, rts_tick_t timeout)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t token;
    rts_tcb_t *current;
    rts_tcb_t *selected;
    bool notify_port;

    if (mutex == NULL ||
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
    if (!rts_mutex_is_valid(mutex))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    current = kernel->current_task;
    if (mutex->owner == NULL)
    {
        RTS_FATAL_UNLESS(mutex->waiters.count == 0u);
        if (current->owned_mutex_count >=
            (size_t)RTS_MAX_MUTEXES_PER_TASK)
        {
            rts_port_critical_exit(token);
            return RTS_STATUS_CAPACITY_EXHAUSTED;
        }
        rts_mutex_owner_link(mutex, current);
        rts_port_critical_exit(token);
        return RTS_STATUS_OK;
    }
    if (mutex->owner == current)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    if (timeout == 0u)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_TIMEOUT;
    }
    if (current->owned_mutex_count >= (size_t)RTS_MAX_MUTEXES_PER_TASK)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_CAPACITY_EXHAUSTED;
    }
    if (rts_mutex_would_cycle(current, mutex))
    {
        RTS_ASSERT(!rts_mutex_would_cycle(current, mutex));
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    if (!rts_mutex_current_can_block(kernel))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }

    rts_ready_remove(&kernel->ready_set, current);
    current->wait.reason = RTS_WAIT_MUTEX;
    current->wait.result = RTS_WAIT_RESULT_NONE;
    current->wait.object = mutex;
    current->wait.timeout_active = timeout != RTS_WAIT_FOREVER;
    current->wait.wake_tick = current->wait.timeout_active
                                  ? kernel->current_tick + timeout
                                  : 0u;
    current->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    current->state = RTS_TASK_STATE_BLOCKED;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.mutex_blocks);
    RTS_DIAG_COUNTER_INC(current->diagnostic_block_count);
#endif
    RTS_TRACE(RTS_TRACE_MUTEX, RTS_WAIT_MUTEX, current->priority);
    rts_wait_object_insert(&mutex->waiters, current);
    if (current->wait.timeout_active)
    {
        rts_delay_insert(&kernel->delay_queue, current);
    }
    RTS_FATAL_UNLESS(rts_priority_recompute_chain(mutex->owner));
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
        return rts_mutex_wait_result_consume(current);
    }
    return RTS_STATUS_OK;
}

static rts_tcb_t *rts_mutex_handoff(rts_kernel_state_t *kernel,
                                    rts_mutex_t *mutex)
{
    rts_tcb_t *waiter = mutex->waiters.head;

    RTS_FATAL_UNLESS(waiter != NULL);
    rts_wait_object_remove(&mutex->waiters, waiter);
    if (waiter->wait.timeout_active)
    {
        rts_delay_remove(&kernel->delay_queue, waiter);
    }
    rts_mutex_owner_unlink(mutex);
    rts_mutex_owner_link(mutex, waiter);
    waiter->wait.reason = RTS_WAIT_NONE;
    waiter->wait.result = RTS_WAIT_RESULT_ACQUIRED;
    waiter->wait.wake_tick = 0u;
    waiter->wait.object = NULL;
    waiter->wait.timeout_active = false;
    waiter->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    waiter->state = waiter == kernel->current_task
                        ? RTS_TASK_STATE_RUNNING
                        : RTS_TASK_STATE_READY;
    rts_ready_insert(&kernel->ready_set, waiter);
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.mutex_handoffs);
    RTS_DIAG_COUNTER_INC(waiter->diagnostic_wake_count);
#endif
    return waiter;
}

rts_status_t rts_mutex_unlock(rts_mutex_t *mutex)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t token;
    rts_tcb_t *former_owner;
    bool notify_port;

    if (mutex == NULL)
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
    if (!rts_mutex_is_valid(mutex))
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (mutex->owner != kernel->current_task)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    former_owner = mutex->owner;
    if (mutex->waiters.head == NULL)
    {
        rts_mutex_owner_unlink(mutex);
    }
    else
    {
        (void)rts_mutex_handoff(kernel, mutex);
    }
    RTS_FATAL_UNLESS(rts_priority_recompute_chain(former_owner));
    if (mutex->owner != NULL)
    {
        RTS_FATAL_UNLESS(rts_priority_recompute_chain(mutex->owner));
    }
    notify_port = rts_mutex_plan_final(kernel);
    rts_port_critical_exit(token);
    if (notify_port)
    {
        rts_port_request_context_switch();
    }
    return RTS_STATUS_OK;
}

bool rts_mutex_timeout_task(rts_kernel_state_t *kernel, rts_tcb_t *task)
{
    rts_mutex_t *mutex;
    rts_tcb_t *owner;
    bool still_executing;

    if (kernel == NULL || task == NULL ||
        task->wait.reason != RTS_WAIT_MUTEX || !task->wait.timeout_active ||
        !rts_scheduler_task_is_blocked_wait(task))
    {
        return false;
    }
    mutex = task->wait.object;
    owner = mutex->owner;
    still_executing = task == kernel->current_task;
    rts_wait_object_remove(&mutex->waiters, task);
    rts_delay_remove(&kernel->delay_queue, task);
    task->wait.reason = RTS_WAIT_NONE;
    task->wait.result = RTS_WAIT_RESULT_TIMEOUT;
    task->wait.wake_tick = 0u;
    task->wait.object = NULL;
    task->wait.timeout_active = false;
    task->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    task->state = still_executing ? RTS_TASK_STATE_RUNNING
                                  : RTS_TASK_STATE_READY;
    rts_ready_insert(&kernel->ready_set, task);
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.mutex_timeouts);
    RTS_DIAG_COUNTER_INC(task->diagnostic_wake_count);
#endif
    RTS_FATAL_UNLESS(rts_priority_recompute_chain(owner));
    return true;
}

rts_status_t rts_mutex_wait_result_consume(rts_tcb_t *task)
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
