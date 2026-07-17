#include "policy_plugin_internal.h"

#include <stddef.h>

#include "config_internal.h"
#include "scheduler_internal.h"
#include "time_internal.h"

static bool rts_policy_edf_is_before(const rts_tcb_t *first,
                                     const rts_tcb_t *second)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (first == kernel->idle_task)
    {
        return false;
    }
    if (second == kernel->idle_task)
    {
        return true;
    }
    if (first->absolute_deadline != second->absolute_deadline)
    {
        return rts_tick_before(first->absolute_deadline,
                               second->absolute_deadline);
    }
    if (first->release_sequence != second->release_sequence)
    {
        return rts_tick_before(first->release_sequence,
                               second->release_sequence);
    }
    return false;
}

void rts_policy_edf_initialize(void)
{
    rts_list_initialize(&rts_kernel_state_get()->edf_ready);
}

bool rts_policy_edf_insert(rts_tcb_t *task)
{
    rts_list_t *ready = &rts_kernel_state_get()->edf_ready;
    rts_list_node_t *position;

    if (task == NULL || task->ready_node.owner != NULL)
    {
        return false;
    }
    task->ready_node.object = task;
    position = ready->head;
    while (position != NULL)
    {
        const rts_tcb_t *queued = position->object;

        if (queued == NULL)
        {
            task->ready_node.object = NULL;
            return false;
        }
        if (rts_policy_edf_is_before(task, queued))
        {
            rts_list_insert_before(ready, position, &task->ready_node);
            return task->ready_node.owner == ready;
        }
        position = position->next;
    }
    rts_list_push_back(ready, &task->ready_node);
    return task->ready_node.owner == ready;
}

bool rts_policy_edf_remove(rts_tcb_t *task)
{
    rts_list_t *ready = &rts_kernel_state_get()->edf_ready;

    if (task == NULL || task->ready_node.owner != ready)
    {
        return false;
    }
    rts_list_remove(ready, &task->ready_node);
    task->ready_node.object = NULL;
    return task->ready_node.owner == NULL;
}

rts_tcb_t *rts_policy_edf_pick_next(void)
{
    const rts_list_node_t *head = rts_kernel_state_get()->edf_ready.head;

    return head == NULL ? NULL : head->object;
}

bool rts_policy_edf_yield(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    const rts_list_node_t *next;

    if (task == NULL || task->ready_node.owner != &kernel->edf_ready)
    {
        return false;
    }
    next = task->ready_node.next;
    if (next == NULL || next->object == NULL ||
        ((const rts_tcb_t *)next->object)->absolute_deadline !=
            task->absolute_deadline ||
        (const rts_tcb_t *)next->object == kernel->idle_task)
    {
        return false;
    }
    if (!rts_policy_edf_remove(task))
    {
        return false;
    }
    ++kernel->policy_release_sequence;
    if (kernel->policy_release_sequence == 0u)
    {
        ++kernel->policy_release_sequence;
    }
    task->release_sequence = kernel->policy_release_sequence;
    return rts_policy_edf_insert(task);
}

bool rts_policy_edf_priority_changed(rts_tcb_t *task,
                                     rts_priority_t effective_priority)
{
    if (task == NULL || effective_priority == RTS_IDLE_PRIORITY ||
        (size_t)effective_priority >= (size_t)RTS_PRIORITY_COUNT)
    {
        return false;
    }
    task->priority = effective_priority;
    return true;
}

bool rts_policy_edf_tick(rts_tick_t elapsed_ticks)
{
#if RTS_ENABLE_TIME_SLICING
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *current = rts_scheduler_current_get();

    if (current == NULL || current->state != RTS_TASK_STATE_RUNNING ||
        current == kernel->idle_task || kernel->switch_plan.active ||
        current->ready_node.owner != &kernel->edf_ready ||
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
    return rts_policy_edf_yield(current);
#else
    (void)elapsed_ticks;
    return false;
#endif
}

bool rts_policy_edf_validate(const rts_tcb_t *task, bool expected_ready)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    const rts_list_node_t *node;
    const rts_list_node_t *previous = NULL;
    const rts_tcb_t *previous_task = NULL;
    size_t count = 0u;

    if (task != NULL)
    {
        return (task->ready_node.owner == &kernel->edf_ready) ==
               expected_ready;
    }
    node = kernel->edf_ready.head;
    while (node != NULL && count <= RTS_SCHEDULABLE_TASK_CAPACITY)
    {
        const rts_tcb_t *queued = node->object;

        if (node->owner != &kernel->edf_ready ||
            node->previous != previous || queued == NULL ||
            &queued->ready_node != node ||
            (previous_task != NULL &&
             rts_policy_edf_is_before(queued, previous_task)))
        {
            return false;
        }
        previous = node;
        previous_task = queued;
        node = node->next;
        ++count;
    }
    return node == NULL && previous == kernel->edf_ready.tail &&
           count == kernel->edf_ready.count &&
           count <= RTS_SCHEDULABLE_TASK_CAPACITY;
}
