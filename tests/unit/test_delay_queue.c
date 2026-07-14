#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "delay_queue.h"

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

static void task_initialize(rts_tcb_t *task, rts_tick_t wake_tick)
{
    task->wait.wake_tick = wake_tick;
    task->wait.reason = RTS_WAIT_NONE;
    task->priority = 9u;
    task->state = RTS_TASK_STATE_RUNNING;
    task->slice_remaining = UINT32_C(55);
    rts_list_node_initialize(&task->ready_node);
    rts_list_node_initialize(&task->delay_node);
}

static bool queue_is_valid(const rts_delay_queue_t *delay_queue)
{
    const rts_list_t *list = &delay_queue->ordered_tasks;
    const rts_list_node_t *node = list->head;
    const rts_list_node_t *previous = NULL;
    size_t count = 0u;

    if (list->count == 0u)
    {
        return list->head == NULL && list->tail == NULL;
    }

    if (list->head == NULL || list->tail == NULL ||
        list->head->previous != NULL || list->tail->next != NULL)
    {
        return false;
    }

    while (node != NULL && count <= list->count)
    {
        if (node->owner != list || node->previous != previous ||
            node->object == NULL)
        {
            return false;
        }
        previous = node;
        node = node->next;
        ++count;
    }

    return node == NULL && previous == list->tail && count == list->count;
}

static void test_initialize_empty(void)
{
    rts_delay_queue_t queue;

    rts_delay_initialize(&queue);
    CHECK(queue.ordered_tasks.count == 0u);
    CHECK(rts_delay_peek_expired(&queue, 0u) == NULL);
    CHECK(queue_is_valid(&queue));
}

static void test_ordering_and_removal(void)
{
    rts_delay_queue_t queue;
    rts_tcb_t ten = {0};
    rts_tcb_t twenty = {0};
    rts_tcb_t thirty = {0};

    rts_delay_initialize(&queue);
    task_initialize(&thirty, 30u);
    task_initialize(&ten, 10u);
    task_initialize(&twenty, 20u);

    rts_delay_insert(&queue, &thirty);
    rts_delay_insert(&queue, &ten);
    rts_delay_insert(&queue, &twenty);

    CHECK(queue.ordered_tasks.head == &ten.delay_node);
    CHECK(ten.delay_node.next == &twenty.delay_node);
    CHECK(twenty.delay_node.next == &thirty.delay_node);
    CHECK(queue.ordered_tasks.tail == &thirty.delay_node);
    CHECK(queue.ordered_tasks.count == 3u && queue_is_valid(&queue));

    rts_delay_remove(&queue, &twenty);
    CHECK(ten.delay_node.next == &thirty.delay_node);
    CHECK(thirty.delay_node.previous == &ten.delay_node);
    CHECK(!rts_delay_contains(&queue, &twenty));
    CHECK(twenty.delay_node.owner == NULL &&
          twenty.delay_node.previous == NULL && twenty.delay_node.next == NULL &&
          twenty.delay_node.object == NULL);
    CHECK(queue.ordered_tasks.count == 2u && queue_is_valid(&queue));
}

static void test_equal_deadline_fifo_and_extraction(void)
{
    rts_delay_queue_t queue;
    rts_tcb_t tasks[3] = {0};
    size_t index;

    rts_delay_initialize(&queue);
    for (index = 0u; index < 3u; ++index)
    {
        task_initialize(&tasks[index], 100u);
        rts_delay_insert(&queue, &tasks[index]);
    }

    CHECK(queue.ordered_tasks.head == &tasks[0].delay_node);
    CHECK(tasks[0].delay_node.next == &tasks[1].delay_node);
    CHECK(tasks[1].delay_node.next == &tasks[2].delay_node);
    CHECK(rts_delay_peek_expired(&queue, 99u) == NULL);

    for (index = 0u; index < 3u; ++index)
    {
        CHECK(rts_delay_peek_expired(&queue, 100u) == &tasks[index]);
        rts_delay_remove(&queue, &tasks[index]);
        CHECK(tasks[index].state == RTS_TASK_STATE_RUNNING);
        CHECK(tasks[index].wait.reason == RTS_WAIT_NONE);
        CHECK(tasks[index].priority == 9u);
        CHECK(tasks[index].slice_remaining == UINT32_C(55));
        CHECK(!rts_list_node_is_linked(&tasks[index].ready_node));
        CHECK(tasks[index].delay_node.object == NULL);
    }

    CHECK(queue.ordered_tasks.count == 0u && queue_is_valid(&queue));
}

static void test_wraparound_order_and_due(void)
{
    rts_delay_queue_t queue;
    rts_tcb_t before_wrap = {0};
    rts_tcb_t after_wrap = {0};
    rts_tcb_t later = {0};

    rts_delay_initialize(&queue);
    task_initialize(&later, UINT32_C(5));
    task_initialize(&after_wrap, UINT32_C(1));
    task_initialize(&before_wrap, UINT32_C(0xfffffffe));
    rts_delay_insert(&queue, &later);
    rts_delay_insert(&queue, &after_wrap);
    rts_delay_insert(&queue, &before_wrap);

    CHECK(queue.ordered_tasks.head == &before_wrap.delay_node);
    CHECK(before_wrap.delay_node.next == &after_wrap.delay_node);
    CHECK(after_wrap.delay_node.next == &later.delay_node);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(0xfffffffd)) == NULL);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(0xfffffffe)) == &before_wrap);
    rts_delay_remove(&queue, &before_wrap);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(0xffffffff)) == NULL);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(1)) == &after_wrap);
    rts_delay_remove(&queue, &after_wrap);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(4)) == NULL);
    CHECK(rts_delay_peek_expired(&queue, UINT32_C(5)) == &later);
    CHECK(queue_is_valid(&queue));
}

static void test_deadline_predicate(void)
{
    CHECK(rts_tick_deadline_reached(10u, 10u));
    CHECK(rts_tick_deadline_reached(11u, 10u));
    CHECK(!rts_tick_deadline_reached(9u, 10u));
    CHECK(!rts_tick_deadline_reached(0u, RTS_DELAY_MAX));
    CHECK(rts_tick_deadline_reached(RTS_DELAY_MAX, RTS_DELAY_MAX));
    CHECK(!rts_tick_deadline_reached(UINT32_C(0xfffffffe), 1u));
    CHECK(rts_tick_deadline_reached(1u, UINT32_C(0xfffffffe)));
}

static void test_contract_assertions(void)
{
#if RTS_ENABLE_ASSERTIONS
    rts_delay_queue_t first;
    rts_delay_queue_t second;
    rts_tcb_t linked = {0};
    rts_tcb_t unlinked = {0};
    rts_tcb_t ambiguous = {0};
    unsigned int before;

    rts_delay_initialize(&first);
    rts_delay_initialize(&second);
    task_initialize(&linked, 0u);
    task_initialize(&unlinked, 10u);
    task_initialize(&ambiguous, RTS_DELAY_MAX + UINT32_C(1));
    rts_delay_insert(&first, &linked);

    before = assertion_count;
    rts_delay_insert(&first, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_delay_contains(&first, &linked));

    before = assertion_count;
    rts_delay_remove(&second, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_delay_contains(&first, &linked));

    before = assertion_count;
    rts_delay_remove(&first, &unlinked);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    rts_delay_insert(&first, &ambiguous);
    CHECK(assertion_count == before + 1u);
    CHECK(!rts_delay_contains(&first, &ambiguous));
    CHECK(ambiguous.delay_node.object == NULL);
    CHECK(first.ordered_tasks.count == 1u && queue_is_valid(&first));
#endif
}

int rts_test_delay_queue_run(void)
{
    test_initialize_empty();
    test_ordering_and_removal();
    test_equal_deadline_fifo_and_extraction();
    test_wraparound_order_and_due();
    test_deadline_predicate();
    test_contract_assertions();
    return test_failures;
}

int main(void)
{
    return rts_test_delay_queue_run();
}
