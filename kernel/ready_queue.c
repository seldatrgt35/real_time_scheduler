#include "ready_queue.h"

#include <stdint.h>

#include "assert_internal.h"

#define RTS_READY_REQUIRE_VOID(condition)     \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return;                           \
        }                                     \
    } while (0)

#define RTS_READY_REQUIRE_FALSE(condition)    \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return false;                     \
        }                                     \
    } while (0)

#define RTS_READY_REQUIRE_NULL(condition)     \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return NULL;                      \
        }                                     \
    } while (0)

static size_t rts_ready_word_index(rts_priority_t priority)
{
    return (size_t)priority / RTS_READY_BITMAP_WORD_BITS;
}

static uint32_t rts_ready_bit_mask(rts_priority_t priority)
{
    const uint32_t bit = (uint32_t)priority % RTS_READY_BITMAP_WORD_BITS;
    return UINT32_C(1) << bit;
}

static bool rts_ready_priority_is_valid(rts_priority_t priority)
{
    return (size_t)priority < (size_t)RTS_PRIORITY_COUNT;
}

void rts_ready_initialize(rts_ready_set_t *ready_set)
{
    size_t index;

    RTS_READY_REQUIRE_VOID(ready_set != NULL);

    for (index = 0u; index < (size_t)RTS_PRIORITY_COUNT; ++index)
    {
        rts_list_initialize(&ready_set->priority_queue[index]);
    }

    for (index = 0u; index < (size_t)RTS_READY_BITMAP_WORDS; ++index)
    {
        ready_set->ready_bitmap[index] = UINT32_C(0);
    }
}

void rts_ready_insert(rts_ready_set_t *ready_set, rts_tcb_t *task)
{
    rts_list_t *queue;
    const size_t word = task != NULL ? rts_ready_word_index(task->priority) : 0u;

    RTS_READY_REQUIRE_VOID(ready_set != NULL);
    RTS_READY_REQUIRE_VOID(task != NULL);
    RTS_READY_REQUIRE_VOID(rts_ready_priority_is_valid(task->priority));
    RTS_READY_REQUIRE_VOID(task->ready_node.owner == NULL);
    RTS_READY_REQUIRE_VOID(task->ready_node.previous == NULL);
    RTS_READY_REQUIRE_VOID(task->ready_node.next == NULL);
    RTS_READY_REQUIRE_VOID(task->ready_node.object == NULL);

    queue = &ready_set->priority_queue[task->priority];
    task->ready_node.object = task;
    rts_list_push_back(queue, &task->ready_node);
    if (task->ready_node.owner != queue)
    {
        task->ready_node.object = NULL;
        RTS_ASSERT(task->ready_node.owner == queue);
        return;
    }

    ready_set->ready_bitmap[word] |= rts_ready_bit_mask(task->priority);
}

void rts_ready_remove(rts_ready_set_t *ready_set, rts_tcb_t *task)
{
    rts_list_t *queue;
    size_t word;

    RTS_READY_REQUIRE_VOID(ready_set != NULL);
    RTS_READY_REQUIRE_VOID(task != NULL);
    RTS_READY_REQUIRE_VOID(rts_ready_priority_is_valid(task->priority));

    queue = &ready_set->priority_queue[task->priority];
    RTS_READY_REQUIRE_VOID(task->ready_node.owner == queue);
    word = rts_ready_word_index(task->priority);

    rts_list_remove(queue, &task->ready_node);
    RTS_READY_REQUIRE_VOID(task->ready_node.owner == NULL);
    task->ready_node.object = NULL;

    if (queue->count == 0u)
    {
        ready_set->ready_bitmap[word] &= ~rts_ready_bit_mask(task->priority);
    }
}

rts_tcb_t *rts_ready_peek_highest(const rts_ready_set_t *ready_set)
{
    size_t word_index;

    RTS_READY_REQUIRE_NULL(ready_set != NULL);

    word_index = (size_t)RTS_READY_BITMAP_WORDS;
    while (word_index > 0u)
    {
        uint32_t word;
        uint32_t bit_index;

        --word_index;
        word = ready_set->ready_bitmap[word_index];
        if (word == UINT32_C(0))
        {
            continue;
        }

        bit_index = RTS_READY_BITMAP_WORD_BITS;
        while (bit_index > 0u)
        {
            size_t priority;
            const rts_list_t *queue;
            rts_tcb_t *task;

            --bit_index;
            if ((word & (UINT32_C(1) << bit_index)) == UINT32_C(0))
            {
                continue;
            }

            priority = (word_index * RTS_READY_BITMAP_WORD_BITS) + bit_index;
            RTS_READY_REQUIRE_NULL(priority < (size_t)RTS_PRIORITY_COUNT);
            queue = &ready_set->priority_queue[priority];
            RTS_READY_REQUIRE_NULL(queue->count > 0u);
            RTS_READY_REQUIRE_NULL(queue->head != NULL);
            RTS_READY_REQUIRE_NULL(queue->head->owner == queue);

            RTS_READY_REQUIRE_NULL(queue->head->object != NULL);
            task = queue->head->object;
            RTS_READY_REQUIRE_NULL(&task->ready_node == queue->head);
            RTS_READY_REQUIRE_NULL((size_t)task->priority == priority);
            return task;
        }
    }

    return NULL;
}

bool rts_ready_has_peer(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task)
{
    const rts_list_t *queue;

    RTS_READY_REQUIRE_FALSE(ready_set != NULL);
    RTS_READY_REQUIRE_FALSE(task != NULL);
    RTS_READY_REQUIRE_FALSE(rts_ready_priority_is_valid(task->priority));

    queue = &ready_set->priority_queue[task->priority];
    RTS_READY_REQUIRE_FALSE(task->ready_node.owner == queue);
    return queue->count > 1u;
}

bool rts_ready_contains(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task)
{
    RTS_READY_REQUIRE_FALSE(ready_set != NULL);
    RTS_READY_REQUIRE_FALSE(task != NULL);
    RTS_READY_REQUIRE_FALSE(rts_ready_priority_is_valid(task->priority));

    return task->ready_node.owner == &ready_set->priority_queue[task->priority];
}

bool rts_ready_is_front(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task)
{
    const rts_list_t *queue;

    RTS_READY_REQUIRE_FALSE(ready_set != NULL);
    RTS_READY_REQUIRE_FALSE(task != NULL);
    RTS_READY_REQUIRE_FALSE(rts_ready_priority_is_valid(task->priority));

    queue = &ready_set->priority_queue[task->priority];
    RTS_READY_REQUIRE_FALSE(task->ready_node.owner == queue);
    RTS_READY_REQUIRE_FALSE(queue->count > 0u);
    RTS_READY_REQUIRE_FALSE(queue->head != NULL);
    return queue->head == &task->ready_node;
}

bool rts_ready_only_contains(const rts_ready_set_t *ready_set,
                             const rts_tcb_t *task)
{
    size_t priority;

    if (ready_set == NULL || task == NULL ||
        !rts_ready_contains(ready_set, task))
    {
        return false;
    }
    for (priority = 0u; priority < (size_t)RTS_PRIORITY_COUNT; ++priority)
    {
        const size_t expected = priority == (size_t)task->priority ? 1u : 0u;

        if (ready_set->priority_queue[priority].count != expected)
        {
            return false;
        }
    }
    return rts_ready_peek_highest(ready_set) == task;
}

void rts_ready_rotate(rts_ready_set_t *ready_set,
                      rts_priority_t priority)
{
    rts_list_t *queue;
    rts_list_node_t *front;

    RTS_READY_REQUIRE_VOID(ready_set != NULL);
    RTS_READY_REQUIRE_VOID(rts_ready_priority_is_valid(priority));

    queue = &ready_set->priority_queue[priority];
    RTS_READY_REQUIRE_VOID(queue->count > 1u);
    RTS_READY_REQUIRE_VOID(queue->head != NULL);

    front = queue->head;
    rts_list_remove(queue, front);
    RTS_READY_REQUIRE_VOID(front->owner == NULL);
    rts_list_push_back(queue, front);
    RTS_READY_REQUIRE_VOID(front->owner == queue);
}
