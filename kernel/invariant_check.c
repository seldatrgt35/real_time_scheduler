#include "invariant_check_internal.h"

#include <stddef.h>

#include "rts/rts_mutex.h"
#include "rts/rts_semaphore.h"
#include "mutex_internal.h"
#include "config_internal.h"
#include "semaphore_internal.h"
#include "scheduler_internal.h"
#include "stack_check_internal.h"
#include "time_internal.h"
#include "timer_internal.h"
#include "wait_object_internal.h"

#if RTS_ENABLE_INVARIANT_CHECKS
static bool rts_task_membership_is_valid(const rts_kernel_state_t *kernel,
                                         const rts_tcb_t *task)
{
    bool ready = rts_ready_contains(&kernel->ready_set, task);
    bool delayed = rts_delay_contains(&kernel->delay_queue, task);
    bool waiting = task->wait_node.owner != NULL;

    if (task->state == RTS_TASK_STATE_READY)
    {
        return ready && !delayed && !waiting &&
               task->wait.reason == RTS_WAIT_NONE;
    }
    if (task->state == RTS_TASK_STATE_RUNNING)
    {
        return task == kernel->current_task && ready && !delayed && !waiting &&
               task->wait.reason == RTS_WAIT_NONE;
    }
    if (task->state != RTS_TASK_STATE_BLOCKED || ready)
    {
        return false;
    }
    if (task->wait.reason == RTS_WAIT_TIMER_SERVICE)
    {
        return rts_scheduler_task_is_timer_service(task) && !delayed &&
               !waiting && task->wait.object == NULL &&
               !task->wait.timeout_active;
    }
    if (task->wait.reason == RTS_WAIT_DELAY)
    {
        return delayed && !waiting && !task->wait.timeout_active;
    }
    if (task->wait.reason == RTS_WAIT_SEMAPHORE ||
        task->wait.reason == RTS_WAIT_MUTEX)
    {
        return waiting && delayed == task->wait.timeout_active &&
               task->wait.object != NULL;
    }
    return false;
}
#endif

bool rts_task_validate_internal(const rts_tcb_t *task)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (task == NULL || task->slot_state != RTS_TASK_SLOT_ALLOCATED ||
        task->priority < task->base_priority ||
        (size_t)task->priority >= (size_t)RTS_PRIORITY_COUNT ||
        !rts_stack_guard_is_valid(task) ||
        !rts_stack_saved_sp_is_valid(task) ||
        !rts_task_membership_is_valid(kernel, task))
    {
        return false;
    }
    if (task->wait.reason == RTS_WAIT_SEMAPHORE)
    {
        const rts_semaphore_t *semaphore = task->wait.object;
        if (!rts_semaphore_is_valid(semaphore) ||
            !rts_wait_object_contains(&semaphore->waiters, task))
        {
            return false;
        }
    }
    if (task->wait.reason == RTS_WAIT_MUTEX)
    {
        const rts_mutex_t *mutex = task->wait.object;
        if (!rts_mutex_is_valid(mutex) ||
            !rts_wait_object_contains(&mutex->waiters, task))
        {
            return false;
        }
    }
    return true;
#else
    (void)task;
    return true;
#endif
}

#if RTS_ENABLE_INVARIANT_CHECKS
static bool rts_ready_structure_is_valid(const rts_kernel_state_t *kernel)
{
    size_t priority;

    for (priority = 0u; priority < (size_t)RTS_PRIORITY_COUNT; ++priority)
    {
        const rts_list_t *list = &kernel->ready_set.priority_queue[priority];
        const rts_list_node_t *node = list->head;
        const rts_list_node_t *previous = NULL;
        size_t count = 0u;
        bool bit = (kernel->ready_set.ready_bitmap[priority / 32u] &
                    (UINT32_C(1) << (priority % 32u))) != 0u;

        while (node != NULL && count <= RTS_SCHEDULABLE_TASK_CAPACITY)
        {
            const rts_tcb_t *task = node->object;
            if (node->owner != list || node->previous != previous ||
                task == NULL || &task->ready_node != node ||
                (size_t)task->priority != priority)
            {
                return false;
            }
            previous = node;
            node = node->next;
            ++count;
        }
        if (node != NULL || previous != list->tail || count != list->count ||
            bit != (count != 0u))
        {
            return false;
        }
    }
    return true;
}
#endif

bool rts_scheduler_validate_internal(void)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    const rts_list_node_t *node = kernel->delay_queue.ordered_tasks.head;
    const rts_tcb_t *previous = NULL;
    size_t count = 0u;

    if (kernel->lifecycle == RTS_KERNEL_RESET)
    {
        return kernel->current_task == NULL && kernel->idle_task == NULL &&
               kernel->timer_service_task == NULL &&
               kernel->application_task_pool.allocated_count == 0u;
    }

    if (!rts_ready_structure_is_valid(kernel) || kernel->idle_task == NULL ||
        kernel->idle_task != &kernel->idle_task_storage ||
        !rts_task_validate_internal(kernel->idle_task) ||
        kernel->timer_service_task !=
            &kernel->timer_service_task_storage ||
        !rts_task_validate_internal(kernel->timer_service_task))
    {
        return false;
    }
    if ((kernel->timer_service_task->state == RTS_TASK_STATE_BLOCKED &&
         rts_timer_manager_get()->callback_queue.count != 0u) ||
        (kernel->timer_service_task->state == RTS_TASK_STATE_READY &&
         rts_timer_manager_get()->callback_queue.count == 0u))
    {
        return false;
    }
    while (node != NULL && count <= RTS_SCHEDULABLE_TASK_CAPACITY)
    {
        const rts_tcb_t *task = node->object;
        if (task == NULL || &task->delay_node != node ||
            node->owner != &kernel->delay_queue.ordered_tasks ||
            (previous != NULL &&
             rts_tick_before(task->wait.wake_tick,
                             previous->wait.wake_tick)))
        {
            return false;
        }
        previous = task;
        node = node->next;
        ++count;
    }
    if (node != NULL || count != kernel->delay_queue.ordered_tasks.count)
    {
        return false;
    }
    if (kernel->lifecycle == RTS_KERNEL_RUNNING &&
        kernel->current_task == NULL)
    {
        return false;
    }
    if (kernel->lifecycle == RTS_KERNEL_RUNNING &&
        kernel->current_task->state != RTS_TASK_STATE_RUNNING)
    {
        bool blocked_switch =
            kernel->current_task->state == RTS_TASK_STATE_BLOCKED &&
            (kernel->switch_plan.pending || kernel->switch_plan.active) &&
            (rts_scheduler_task_is_blocked_delay(kernel->current_task) ||
             rts_scheduler_task_is_blocked_wait(kernel->current_task));

        if (!blocked_switch)
        {
            return false;
        }
    }
    if (kernel->switch_plan.active && kernel->switch_plan.pending)
    {
        return false;
    }
    return true;
#else
    return true;
#endif
}

bool rts_sync_validate_internal(void)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t index;

    if (kernel->lifecycle == RTS_KERNEL_RESET)
    {
        return true;
    }

    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        const rts_tcb_t *task = &kernel->application_task_pool.slots[index];
        const rts_mutex_t *mutex;
        size_t owned = 0u;
        rts_priority_t required;

        if (task->slot_state != RTS_TASK_SLOT_ALLOCATED)
        {
            continue;
        }
        {
            const rts_tcb_t *cursor = task;
            size_t chain_depth = 0u;

            while (cursor->state == RTS_TASK_STATE_BLOCKED &&
                   cursor->wait.reason == RTS_WAIT_MUTEX)
            {
                const rts_mutex_t *waited_mutex = cursor->wait.object;
                const rts_tcb_t *owner;

                if (!rts_mutex_is_valid(waited_mutex))
                {
                    return false;
                }
                owner = waited_mutex->owner;
                if (owner == NULL || owner == task ||
                    (!rts_task_handle_is_application_task(
                         (rts_task_handle_t)owner) &&
                     owner != kernel->idle_task &&
                     owner != kernel->timer_service_task) ||
                    ++chain_depth > RTS_SCHEDULABLE_TASK_CAPACITY)
                {
                    return false;
                }
                cursor = owner;
            }
        }
        required = task->base_priority;
        mutex = task->owned_mutex_head;
        while (mutex != NULL &&
               owned < (size_t)RTS_MAX_MUTEXES_PER_TASK)
        {
            if (!rts_mutex_is_valid(mutex) || mutex->owner != task)
            {
                return false;
            }
            if (mutex->waiters.head != NULL &&
                mutex->waiters.head->priority > required)
            {
                required = mutex->waiters.head->priority;
            }
            mutex = mutex->owned_next;
            ++owned;
        }
        if (mutex != NULL || owned != task->owned_mutex_count ||
            required != task->priority)
        {
            return false;
        }
    }
    {
        const rts_tcb_t *task = kernel->timer_service_task;
        const rts_mutex_t *mutex;
        rts_priority_t required;
        size_t owned = 0u;

        if (task == NULL)
        {
            return false;
        }
        required = task->base_priority;
        mutex = task->owned_mutex_head;
        while (mutex != NULL &&
               owned < (size_t)RTS_MAX_MUTEXES_PER_TASK)
        {
            if (!rts_mutex_is_valid(mutex) || mutex->owner != task)
            {
                return false;
            }
            if (mutex->waiters.head != NULL &&
                mutex->waiters.head->priority > required)
            {
                required = mutex->waiters.head->priority;
            }
            mutex = mutex->owned_next;
            ++owned;
        }
        if (mutex != NULL || owned != task->owned_mutex_count ||
            required != task->priority)
        {
            return false;
        }
    }
    return true;
#else
    return true;
#endif
}

bool rts_kernel_validate_all(void)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t index;
    size_t allocated = 0u;

    if (!rts_scheduler_validate_internal() || !rts_sync_validate_internal() ||
        !rts_timer_manager_validate(rts_timer_manager_get()))
    {
        return false;
    }
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        const rts_tcb_t *task = &kernel->application_task_pool.slots[index];
        if (task->slot_state == RTS_TASK_SLOT_ALLOCATED)
        {
            ++allocated;
            if (!rts_task_validate_internal(task))
            {
                return false;
            }
        }
        else if (task->slot_state != RTS_TASK_SLOT_FREE &&
                 task->slot_state != RTS_TASK_SLOT_RESERVED)
        {
            return false;
        }
    }
    return allocated == kernel->application_task_pool.allocated_count;
#else
    return true;
#endif
}
