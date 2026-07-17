#include "priority_internal.h"

#include <stddef.h>

#include "assert_internal.h"
#include "rts/rts_mutex.h"
#include "rts/rts_semaphore.h"
#include "scheduler_internal.h"
#include "wait_object_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"
#include "config_internal.h"
#include "scheduler_policy.h"

static rts_wait_object_storage_t *rts_priority_waiters_for(rts_tcb_t *task)
{
    if (task->wait.reason == RTS_WAIT_MUTEX)
    {
        return &((rts_mutex_t *)task->wait.object)->waiters;
    }
    if (task->wait.reason == RTS_WAIT_SEMAPHORE)
    {
        return &((rts_semaphore_t *)task->wait.object)->waiters;
    }
    return NULL;
}

bool rts_priority_set_effective(rts_tcb_t *task,
                                rts_priority_t effective_priority)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_wait_object_storage_t *waiters;
    rts_priority_t previous;

    (void)kernel;

    if (task == NULL || effective_priority < task->base_priority ||
        effective_priority == RTS_IDLE_PRIORITY ||
        (size_t)effective_priority >= (size_t)RTS_PRIORITY_COUNT)
    {
        return false;
    }
    if (task->priority == effective_priority)
    {
        return true;
    }

    previous = task->priority;
    if (!rts_policy_priority_changed(task, effective_priority))
    {
        return false;
    }
    {
#if RTS_ENABLE_RUNTIME_STATS
        if (effective_priority > previous)
        {
            RTS_DIAG_COUNTER_INC(kernel->runtime_counters.priority_raises);
        }
        else
        {
            RTS_DIAG_COUNTER_INC(kernel->runtime_counters.priority_restorations);
        }
#endif
        RTS_TRACE(RTS_TRACE_PRIORITY, previous, effective_priority);
        (void)previous;
    }
    waiters = rts_priority_waiters_for(task);
    if (waiters != NULL)
    {
        RTS_FATAL_UNLESS(rts_wait_object_contains(waiters, task));
        rts_wait_object_reprioritize(waiters, task);
    }
    return true;
}

static rts_priority_t rts_priority_required_by_owned(const rts_tcb_t *task)
{
    const rts_mutex_t *mutex = task->owned_mutex_head;
    rts_priority_t required = task->base_priority;
    size_t count = 0u;

    while (mutex != NULL && count < (size_t)RTS_MAX_MUTEXES_PER_TASK)
    {
        if (mutex->owner != task)
        {
            RTS_FATAL_UNLESS(mutex->owner == task);
            return required;
        }
        if (mutex->waiters.head != NULL &&
            mutex->waiters.head->priority > required)
        {
            required = mutex->waiters.head->priority;
        }
        mutex = mutex->owned_next;
        ++count;
    }
    RTS_FATAL_UNLESS(mutex == NULL);
    return required;
}

bool rts_priority_recompute_chain(rts_tcb_t *task)
{
    rts_tcb_t *visited[RTS_MAX_TASKS + 2u];
    size_t depth = 0u;

    while (task != NULL && depth < RTS_SCHEDULABLE_TASK_CAPACITY)
    {
        size_t index;
        rts_priority_t required;

        for (index = 0u; index < depth; ++index)
        {
            if (visited[index] == task)
            {
                RTS_FATAL_UNLESS(visited[index] != task);
                return false;
            }
        }
        visited[depth] = task;
        ++depth;
        required = rts_priority_required_by_owned(task);
        if (!rts_priority_set_effective(task, required))
        {
            return false;
        }
        if (task->state != RTS_TASK_STATE_BLOCKED ||
            task->wait.reason != RTS_WAIT_MUTEX)
        {
            return true;
        }
        task = ((rts_mutex_t *)task->wait.object)->owner;
    }
    RTS_FATAL_UNLESS(task == NULL);
    return task == NULL;
}
