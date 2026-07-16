#include "wait_object_internal.h"

#include <stddef.h>

#include "assert_internal.h"

static bool rts_wait_node_is_unlinked(const rts_tcb_t *task)
{
    return task->wait_node.previous == NULL && task->wait_node.next == NULL &&
           task->wait_node.owner == NULL;
}

void rts_wait_object_initialize(rts_wait_object_storage_t *object)
{
    RTS_ASSERT(object != NULL);
    if (object == NULL)
    {
        return;
    }
    object->head = NULL;
    object->tail = NULL;
    object->count = 0u;
}

bool rts_wait_object_is_empty(const rts_wait_object_storage_t *object)
{
    bool valid = rts_wait_object_validate(object);

    RTS_ASSERT(valid);
    return valid && object->count == 0u;
}

bool rts_wait_object_contains(const rts_wait_object_storage_t *object,
                              const rts_tcb_t *task)
{
    return object != NULL && task != NULL && task->wait_node.owner == object;
}

void rts_wait_object_insert(rts_wait_object_storage_t *object, rts_tcb_t *task)
{
    rts_tcb_t *position;
    rts_tcb_t *previous;

    RTS_ASSERT(object != NULL);
    RTS_ASSERT(task != NULL);
    RTS_ASSERT(object != NULL && rts_wait_object_validate(object));
    RTS_ASSERT(task != NULL && rts_wait_node_is_unlinked(task));
    RTS_ASSERT(object == NULL || object->count < (size_t)RTS_MAX_TASKS);
    if (object == NULL || task == NULL || !rts_wait_object_validate(object) ||
        !rts_wait_node_is_unlinked(task) ||
        object->count >= (size_t)RTS_MAX_TASKS)
    {
        return;
    }

    position = object->head;
    while (position != NULL && position->priority >= task->priority)
    {
        position = position->wait_node.next;
    }

    previous = position == NULL ? object->tail
                                : position->wait_node.previous;
    task->wait_node.previous = previous;
    task->wait_node.next = position;
    task->wait_node.owner = object;
    if (previous == NULL)
    {
        object->head = task;
    }
    else
    {
        previous->wait_node.next = task;
    }
    if (position == NULL)
    {
        object->tail = task;
    }
    else
    {
        position->wait_node.previous = task;
    }
    ++object->count;
    RTS_FATAL_UNLESS(rts_wait_object_validate(object));
}

void rts_wait_object_remove(rts_wait_object_storage_t *object, rts_tcb_t *task)
{
    rts_tcb_t *previous;
    rts_tcb_t *next;

    RTS_ASSERT(object != NULL);
    RTS_ASSERT(task != NULL);
    RTS_ASSERT(object != NULL && task != NULL &&
               rts_wait_object_contains(object, task));
    if (object == NULL || task == NULL ||
        !rts_wait_object_contains(object, task))
    {
        return;
    }

    previous = task->wait_node.previous;
    next = task->wait_node.next;
    if (previous == NULL)
    {
        object->head = next;
    }
    else
    {
        previous->wait_node.next = next;
    }
    if (next == NULL)
    {
        object->tail = previous;
    }
    else
    {
        next->wait_node.previous = previous;
    }
    task->wait_node.previous = NULL;
    task->wait_node.next = NULL;
    task->wait_node.owner = NULL;
    RTS_FATAL_UNLESS(object->count > 0u);
    --object->count;
    RTS_FATAL_UNLESS(rts_wait_object_validate(object));
}

rts_tcb_t *rts_wait_object_pop_highest(rts_wait_object_storage_t *object)
{
    rts_tcb_t *task;

    RTS_ASSERT(object != NULL);
    RTS_ASSERT(object != NULL && rts_wait_object_validate(object));
    if (object == NULL || !rts_wait_object_validate(object))
    {
        return NULL;
    }
    task = object->head;
    if (task != NULL)
    {
        rts_wait_object_remove(object, task);
    }
    return task;
}

void rts_wait_object_reprioritize(rts_wait_object_storage_t *object,
                                  rts_tcb_t *task)
{
    RTS_ASSERT(rts_wait_object_contains(object, task));
    if (!rts_wait_object_contains(object, task))
    {
        return;
    }
    rts_wait_object_remove(object, task);
    rts_wait_object_insert(object, task);
}

bool rts_wait_object_validate(const rts_wait_object_storage_t *object)
{
    const rts_tcb_t *task;
    const rts_tcb_t *previous = NULL;
    size_t count = 0u;

    if (object == NULL ||
        ((object->head == NULL) != (object->tail == NULL)))
    {
        return false;
    }
    task = object->head;
    while (task != NULL && count <= (size_t)RTS_MAX_TASKS)
    {
        if (task->wait_node.owner != object ||
            task->wait_node.previous != previous ||
            (previous != NULL && previous->priority < task->priority))
        {
            return false;
        }
        previous = task;
        task = task->wait_node.next;
        ++count;
    }
    return task == NULL && previous == object->tail &&
           count == object->count &&
           count <= (size_t)RTS_MAX_TASKS;
}
