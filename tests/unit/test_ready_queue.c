#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ready_queue.h"

static int test_failures;
static unsigned int assertion_count;

#define CHECK(condition)                    \
    do                                      \
    {                                       \
        if (!(condition))                   \
        {                                   \
            ++test_failures;                \
        }                                   \
    } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

static void task_initialize(rts_tcb_t *task,
                            rts_priority_t priority,
                            rts_task_state_t state)
{
    task->priority = priority;
    task->state = state;
    task->wait.reason = RTS_WAIT_DELAY;
    task->slice_remaining = UINT32_C(77);
    rts_list_node_initialize(&task->ready_node);
}

static uint32_t priority_mask(rts_priority_t priority)
{
    return UINT32_C(1) << ((uint32_t)priority % RTS_READY_BITMAP_WORD_BITS);
}

static bool ready_set_is_valid(const rts_ready_set_t *ready_set)
{
    size_t priority;

    for (priority = 0u; priority < (size_t)RTS_PRIORITY_COUNT; ++priority)
    {
        const rts_list_t *queue = &ready_set->priority_queue[priority];
        const size_t word = priority / RTS_READY_BITMAP_WORD_BITS;
        const bool bit_is_set =
            (ready_set->ready_bitmap[word] & priority_mask((rts_priority_t)priority)) != 0u;
        const rts_list_node_t *node = queue->head;
        const rts_list_node_t *previous = NULL;
        size_t count = 0u;

        if (bit_is_set != (queue->count != 0u))
        {
            return false;
        }

        while (node != NULL && count <= queue->count)
        {
            if (node->owner != queue || node->previous != previous ||
                node->object == NULL)
            {
                return false;
            }
            previous = node;
            node = node->next;
            ++count;
        }

        if (node != NULL || count != queue->count ||
            (queue->count == 0u && (queue->head != NULL || queue->tail != NULL)) ||
            (queue->count != 0u &&
             (queue->head == NULL || queue->tail == NULL ||
              queue->head->previous != NULL || queue->tail->next != NULL ||
              previous != queue->tail)))
        {
            return false;
        }
    }

    return true;
}

static void test_initialize_empty(void)
{
    rts_ready_set_t ready_set;
    size_t index;

    rts_ready_initialize(&ready_set);
    CHECK(rts_ready_peek_highest(&ready_set) == NULL);
    CHECK(ready_set_is_valid(&ready_set));

    for (index = 0u; index < (size_t)RTS_READY_BITMAP_WORDS; ++index)
    {
        CHECK(ready_set.ready_bitmap[index] == 0u);
    }
}

static void test_priority_order_and_boundaries(void)
{
    rts_ready_set_t ready_set;
    rts_tcb_t tasks[5] = {0};
    const rts_priority_t priorities[5] = {0u, 1u, 31u, 32u, 64u};
    size_t index;

    rts_ready_initialize(&ready_set);
    for (index = 0u; index < 5u; ++index)
    {
        task_initialize(&tasks[index], priorities[index], RTS_TASK_STATE_BLOCKED);
        rts_ready_insert(&ready_set, &tasks[index]);
        CHECK(rts_ready_contains(&ready_set, &tasks[index]));
        CHECK(tasks[index].state == RTS_TASK_STATE_BLOCKED);
        CHECK(tasks[index].wait.reason == RTS_WAIT_DELAY);
        CHECK(tasks[index].slice_remaining == UINT32_C(77));
    }

    CHECK(rts_ready_peek_highest(&ready_set) == &tasks[4]);
    CHECK((ready_set.ready_bitmap[0] & priority_mask(31u)) != 0u);
    CHECK((ready_set.ready_bitmap[1] & priority_mask(32u)) != 0u);
    CHECK((ready_set.ready_bitmap[2] & priority_mask(64u)) != 0u);
    CHECK(ready_set_is_valid(&ready_set));

    for (index = 5u; index > 0u; --index)
    {
        rts_ready_remove(&ready_set, &tasks[index - 1u]);
        CHECK(!rts_ready_contains(&ready_set, &tasks[index - 1u]));
        CHECK(tasks[index - 1u].ready_node.object == NULL);
    }
    CHECK(rts_ready_peek_highest(&ready_set) == NULL);
    CHECK(ready_set_is_valid(&ready_set));
}

static void test_fifo_remove_and_peer(void)
{
    rts_ready_set_t ready_set;
    rts_tcb_t first = {0};
    rts_tcb_t second = {0};
    rts_tcb_t third = {0};

    rts_ready_initialize(&ready_set);
    task_initialize(&first, 7u, RTS_TASK_STATE_RUNNING);
    task_initialize(&second, 7u, RTS_TASK_STATE_READY);
    task_initialize(&third, 7u, RTS_TASK_STATE_READY);

    rts_ready_insert(&ready_set, &first);
    CHECK(!rts_ready_has_peer(&ready_set, &first));
    rts_ready_insert(&ready_set, &second);
    rts_ready_insert(&ready_set, &third);
    CHECK(rts_ready_has_peer(&ready_set, &first));
    CHECK(rts_ready_peek_highest(&ready_set) == &first);

    rts_ready_remove(&ready_set, &first);
    CHECK(rts_ready_peek_highest(&ready_set) == &second);
    CHECK((ready_set.ready_bitmap[0] & priority_mask(7u)) != 0u);
    rts_ready_remove(&ready_set, &second);
    CHECK(rts_ready_peek_highest(&ready_set) == &third);
    rts_ready_remove(&ready_set, &third);
    CHECK((ready_set.ready_bitmap[0] & priority_mask(7u)) == 0u);
    CHECK(ready_set_is_valid(&ready_set));
}

static void test_rotation(void)
{
    rts_ready_set_t ready_set;
    rts_tcb_t tasks[3] = {0};
    size_t index;

    rts_ready_initialize(&ready_set);
    for (index = 0u; index < 3u; ++index)
    {
        task_initialize(&tasks[index], 5u, (rts_task_state_t)(index + 1u));
        rts_ready_insert(&ready_set, &tasks[index]);
    }

    rts_ready_rotate(&ready_set, 5u);
    CHECK(rts_ready_peek_highest(&ready_set) == &tasks[1]);
    CHECK(ready_set.priority_queue[5].tail == &tasks[0].ready_node);
    CHECK(tasks[0].state == RTS_TASK_STATE_READY);
    CHECK(tasks[1].state == RTS_TASK_STATE_RUNNING);
    CHECK(tasks[2].state == RTS_TASK_STATE_BLOCKED);
    CHECK(ready_set.priority_queue[5].count == 3u);
    CHECK(ready_set_is_valid(&ready_set));

    rts_ready_remove(&ready_set, &tasks[1]);
    CHECK(rts_ready_peek_highest(&ready_set) == &tasks[2]);
    rts_ready_remove(&ready_set, &tasks[2]);
    CHECK(rts_ready_peek_highest(&ready_set) == &tasks[0]);
}

static void test_contract_assertions(void)
{
#if RTS_ENABLE_ASSERTIONS
    rts_ready_set_t first;
    rts_ready_set_t second;
    rts_tcb_t linked = {0};
    rts_tcb_t unlinked = {0};
    rts_tcb_t single = {0};
    rts_tcb_t invalid = {0};
    unsigned int before;

    rts_ready_initialize(&first);
    rts_ready_initialize(&second);
    task_initialize(&linked, 3u, RTS_TASK_STATE_READY);
    task_initialize(&unlinked, 3u, RTS_TASK_STATE_READY);
    task_initialize(&single, 4u, RTS_TASK_STATE_READY);
    task_initialize(&invalid, (rts_priority_t)RTS_PRIORITY_COUNT,
                    RTS_TASK_STATE_READY);
    rts_ready_insert(&first, &linked);
    rts_ready_insert(&first, &single);

    before = assertion_count;
    rts_ready_insert(&first, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_ready_contains(&first, &linked));

    before = assertion_count;
    rts_ready_remove(&second, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_ready_contains(&first, &linked));

    before = assertion_count;
    rts_ready_remove(&first, &unlinked);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    rts_ready_rotate(&first, 4u);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    rts_ready_insert(&first, &invalid);
    CHECK(assertion_count == before + 1u);
    CHECK(!rts_list_node_is_linked(&invalid.ready_node));
    CHECK(invalid.ready_node.object == NULL);
    CHECK(ready_set_is_valid(&first));
#endif
}

int rts_test_ready_queue_run(void)
{
    test_initialize_empty();
    test_priority_order_and_boundaries();
    test_fifo_remove_and_peer();
    test_rotation();
    test_contract_assertions();
    return test_failures;
}

int main(void)
{
    return rts_test_ready_queue_run();
}
