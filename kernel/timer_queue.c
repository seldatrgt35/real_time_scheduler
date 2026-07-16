#include "timer_queue.h"

#include <stddef.h>

#include "assert_internal.h"
#include "time_internal.h"
#include "timer_internal.h"

void rts_timer_queue_initialize(rts_timer_queue_t *queue)
{
    RTS_ASSERT(queue != NULL);
    if (queue != NULL)
    {
        rts_list_initialize(&queue->ordered_timers);
    }
}

void rts_timer_queue_insert(rts_timer_queue_t *queue,
                            struct rts_timer *timer)
{
    rts_list_node_t *position;

    RTS_ASSERT(queue != NULL);
    RTS_ASSERT(timer != NULL);
    RTS_ASSERT(timer != NULL && timer->queue_node.owner == NULL);
    if (queue == NULL || timer == NULL || timer->queue_node.owner != NULL)
    {
        return;
    }

    position = queue->ordered_timers.head;
    while (position != NULL)
    {
        const struct rts_timer *queued = position->object;

        if (queued == NULL)
        {
            RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, position);
        }
        if (rts_tick_before(timer->expiration_tick,
                            queued->expiration_tick))
        {
            break;
        }
        position = position->next;
    }

    timer->queue_node.object = timer;
    if (position == NULL)
    {
        rts_list_push_back(&queue->ordered_timers, &timer->queue_node);
    }
    else
    {
        rts_list_insert_before(&queue->ordered_timers, position,
                               &timer->queue_node);
    }
}

void rts_timer_queue_remove(rts_timer_queue_t *queue,
                            struct rts_timer *timer)
{
    RTS_ASSERT(queue != NULL);
    RTS_ASSERT(timer != NULL);
    RTS_ASSERT(queue != NULL && timer != NULL &&
               timer->queue_node.owner == &queue->ordered_timers);
    if (queue == NULL || timer == NULL ||
        timer->queue_node.owner != &queue->ordered_timers)
    {
        return;
    }
    rts_list_remove(&queue->ordered_timers, &timer->queue_node);
}

struct rts_timer *rts_timer_queue_peek_expired(
    const rts_timer_queue_t *queue,
    rts_tick_t now)
{
    const rts_list_node_t *head;
    struct rts_timer *timer;

    if (queue == NULL)
    {
        return NULL;
    }
    head = queue->ordered_timers.head;
    if (head == NULL)
    {
        return NULL;
    }
    timer = head->object;
    if (timer == NULL)
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, head);
    }
    return timer != NULL && rts_tick_reached(now, timer->expiration_tick)
               ? timer
               : NULL;
}

bool rts_timer_queue_next_deadline(const rts_timer_queue_t *queue,
                                   rts_tick_t *out_deadline)
{
    const rts_list_node_t *head;
    const struct rts_timer *timer;

    if (queue == NULL || out_deadline == NULL)
    {
        RTS_ASSERT(queue != NULL && out_deadline != NULL);
        return false;
    }
    head = queue->ordered_timers.head;
    if (head == NULL)
    {
        RTS_ASSERT(queue->ordered_timers.count == 0u);
        return false;
    }
    timer = head->object;
    if (head->owner != &queue->ordered_timers || timer == NULL ||
        &timer->queue_node != head)
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, head);
        return false;
    }
    *out_deadline = timer->expiration_tick;
    return true;
}

bool rts_timer_queue_contains(const rts_timer_queue_t *queue,
                              const struct rts_timer *timer)
{
    return queue != NULL && timer != NULL &&
           timer->queue_node.owner == &queue->ordered_timers;
}

bool rts_timer_queue_validate(const rts_timer_queue_t *queue)
{
    const rts_list_node_t *node;
    const rts_list_node_t *previous_node = NULL;
    const struct rts_timer *previous_timer = NULL;
    size_t count = 0u;

    if (queue == NULL)
    {
        return false;
    }
    node = queue->ordered_timers.head;
    while (node != NULL && count <= (size_t)RTS_MAX_TIMERS)
    {
        const struct rts_timer *timer = node->object;

        if (node->owner != &queue->ordered_timers ||
            node->previous != previous_node || timer == NULL ||
            &timer->queue_node != node ||
            timer->state != RTS_TIMER_ACTIVE ||
            (previous_timer != NULL &&
             rts_tick_before(timer->expiration_tick,
                             previous_timer->expiration_tick)))
        {
            return false;
        }
        previous_node = node;
        previous_timer = timer;
        node = node->next;
        ++count;
    }
    return node == NULL && previous_node == queue->ordered_timers.tail &&
           count == queue->ordered_timers.count &&
           count <= (size_t)RTS_MAX_TIMERS;
}
