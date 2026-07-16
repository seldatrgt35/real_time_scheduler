#include "timer_callback_queue.h"

#include <stddef.h>

#include "assert_internal.h"

static size_t rts_timer_callback_queue_next(size_t index)
{
    ++index;
    return index == (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY ? 0u : index;
}

void rts_timer_callback_queue_initialize(rts_timer_callback_queue_t *queue)
{
    size_t index;

    RTS_ASSERT(queue != NULL);
    if (queue == NULL)
    {
        return;
    }
    for (index = 0u;
         index < (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY; ++index)
    {
        queue->items[index] = (rts_timer_callback_work_t){0};
    }
    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;
    queue->maximum_depth = 0u;
}

bool rts_timer_callback_queue_enqueue(
    rts_timer_callback_queue_t *queue,
    const rts_timer_callback_work_t *work)
{
    if (queue == NULL || work == NULL || work->timer == NULL ||
        queue->count >= (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY)
    {
        return false;
    }
    queue->items[queue->tail] = *work;
    queue->tail = rts_timer_callback_queue_next(queue->tail);
    ++queue->count;
    if (queue->count > queue->maximum_depth)
    {
        queue->maximum_depth = queue->count;
    }
    return true;
}

bool rts_timer_callback_queue_dequeue(rts_timer_callback_queue_t *queue,
                                      rts_timer_callback_work_t *work)
{
    if (queue == NULL || work == NULL || queue->count == 0u)
    {
        return false;
    }
    *work = queue->items[queue->head];
    queue->items[queue->head] = (rts_timer_callback_work_t){0};
    queue->head = rts_timer_callback_queue_next(queue->head);
    --queue->count;
    return true;
}

size_t rts_timer_callback_queue_remove_timer(
    rts_timer_callback_queue_t *queue,
    const struct rts_timer *timer)
{
    size_t original_count;
    size_t read_index;
    size_t write_index;
    size_t new_tail;
    size_t kept = 0u;
    size_t removed = 0u;
    size_t offset;

    if (queue == NULL || timer == NULL)
    {
        return 0u;
    }
    original_count = queue->count;
    read_index = queue->head;
    write_index = queue->head;
    for (offset = 0u; offset < original_count; ++offset)
    {
        rts_timer_callback_work_t work = queue->items[read_index];

        read_index = rts_timer_callback_queue_next(read_index);
        if (work.timer == timer)
        {
            ++removed;
        }
        else
        {
            queue->items[write_index] = work;
            write_index = rts_timer_callback_queue_next(write_index);
            ++kept;
        }
    }
    new_tail = write_index;
    for (offset = kept; offset < original_count; ++offset)
    {
        queue->items[write_index] = (rts_timer_callback_work_t){0};
        write_index = rts_timer_callback_queue_next(write_index);
    }
    queue->count = kept;
    queue->tail = new_tail;
    return removed;
}

size_t rts_timer_callback_queue_count_for(
    const rts_timer_callback_queue_t *queue,
    const struct rts_timer *timer)
{
    size_t index;
    size_t offset;
    size_t count = 0u;

    if (queue == NULL || timer == NULL)
    {
        return 0u;
    }
    index = queue->head;
    for (offset = 0u; offset < queue->count; ++offset)
    {
        if (queue->items[index].timer == timer)
        {
            ++count;
        }
        index = rts_timer_callback_queue_next(index);
    }
    return count;
}

bool rts_timer_callback_queue_validate(
    const rts_timer_callback_queue_t *queue)
{
    size_t index;
    size_t offset;

    if (queue == NULL ||
        queue->head >= (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY ||
        queue->tail >= (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY ||
        queue->count > (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY ||
        queue->maximum_depth < queue->count ||
        queue->maximum_depth >
            (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY)
    {
        return false;
    }
    index = queue->head;
    for (offset = 0u; offset < queue->count; ++offset)
    {
        if (queue->items[index].timer == NULL)
        {
            return false;
        }
        index = rts_timer_callback_queue_next(index);
    }
    return index == queue->tail;
}
