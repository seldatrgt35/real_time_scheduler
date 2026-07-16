#include "delay_queue.h"

#include <stdint.h>

#include "assert_internal.h"
#include "time_internal.h"

#define RTS_DELAY_REQUIRE_VOID(condition)     \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return;                           \
        }                                     \
    } while (0)

#define RTS_DELAY_REQUIRE_FALSE(condition)    \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return false;                     \
        }                                     \
    } while (0)

#define RTS_DELAY_REQUIRE_NULL(condition)     \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return NULL;                      \
        }                                     \
    } while (0)

static bool rts_tick_is_before(rts_tick_t first,
                               rts_tick_t second,
                               bool *is_before)
{
    const rts_tick_t difference = first - second;

    if (difference == RTS_TICK_HALF_RANGE)
    {
        RTS_ASSERT(difference != RTS_TICK_HALF_RANGE);
        return false;
    }

    *is_before = rts_tick_before(first, second);
    return true;
}

void rts_delay_initialize(rts_delay_queue_t *delay_queue)
{
    RTS_DELAY_REQUIRE_VOID(delay_queue != NULL);
    rts_list_initialize(&delay_queue->ordered_tasks);
}

void rts_delay_insert(rts_delay_queue_t *delay_queue, rts_tcb_t *task)
{
    rts_list_t *list;
    rts_list_node_t *position;

    RTS_DELAY_REQUIRE_VOID(delay_queue != NULL);
    RTS_DELAY_REQUIRE_VOID(task != NULL);
    RTS_DELAY_REQUIRE_VOID(task->delay_node.owner == NULL);
    RTS_DELAY_REQUIRE_VOID(task->delay_node.previous == NULL);
    RTS_DELAY_REQUIRE_VOID(task->delay_node.next == NULL);
    RTS_DELAY_REQUIRE_VOID(task->delay_node.object == NULL);

    list = &delay_queue->ordered_tasks;
    position = list->head;
    while (position != NULL)
    {
        rts_tcb_t *queued_task;
        bool task_is_before;

        RTS_DELAY_REQUIRE_VOID(position->owner == list);
        RTS_DELAY_REQUIRE_VOID(position->object != NULL);
        queued_task = position->object;
        RTS_DELAY_REQUIRE_VOID(&queued_task->delay_node == position);
        if (!rts_tick_is_before(task->wait.wake_tick,
                                queued_task->wait.wake_tick,
                                &task_is_before))
        {
            return;
        }

        if (task_is_before)
        {
            task->delay_node.object = task;
            rts_list_insert_before(list, position, &task->delay_node);
            if (task->delay_node.owner != list)
            {
                task->delay_node.object = NULL;
                RTS_ASSERT(task->delay_node.owner == list);
                return;
            }
            return;
        }

        position = position->next;
    }

    task->delay_node.object = task;
    rts_list_push_back(list, &task->delay_node);
    if (task->delay_node.owner != list)
    {
        task->delay_node.object = NULL;
        RTS_ASSERT(task->delay_node.owner == list);
        return;
    }
}

void rts_delay_remove(rts_delay_queue_t *delay_queue, rts_tcb_t *task)
{
    rts_list_t *list;

    RTS_DELAY_REQUIRE_VOID(delay_queue != NULL);
    RTS_DELAY_REQUIRE_VOID(task != NULL);

    list = &delay_queue->ordered_tasks;
    RTS_DELAY_REQUIRE_VOID(task->delay_node.owner == list);
    rts_list_remove(list, &task->delay_node);
    RTS_DELAY_REQUIRE_VOID(task->delay_node.owner == NULL);
    task->delay_node.object = NULL;
}

rts_tcb_t *rts_delay_peek_expired(const rts_delay_queue_t *delay_queue,
                                  rts_tick_t now)
{
    const rts_list_t *list;
    rts_tcb_t *task;

    RTS_DELAY_REQUIRE_NULL(delay_queue != NULL);
    list = &delay_queue->ordered_tasks;

    if (list->count == 0u)
    {
        RTS_DELAY_REQUIRE_NULL(list->head == NULL);
        RTS_DELAY_REQUIRE_NULL(list->tail == NULL);
        return NULL;
    }

    RTS_DELAY_REQUIRE_NULL(list->head != NULL);
    RTS_DELAY_REQUIRE_NULL(list->head->owner == list);
    RTS_DELAY_REQUIRE_NULL(list->head->object != NULL);
    task = list->head->object;
    RTS_DELAY_REQUIRE_NULL(&task->delay_node == list->head);

    if (!rts_tick_deadline_reached(now, task->wait.wake_tick))
    {
        return NULL;
    }

    return task;
}

bool rts_delay_contains(const rts_delay_queue_t *delay_queue,
                        const rts_tcb_t *task)
{
    RTS_DELAY_REQUIRE_FALSE(delay_queue != NULL);
    RTS_DELAY_REQUIRE_FALSE(task != NULL);

    return task->delay_node.owner == &delay_queue->ordered_tasks;
}

bool rts_tick_deadline_reached(rts_tick_t now, rts_tick_t deadline)
{
    return rts_tick_reached(now, deadline);
}
