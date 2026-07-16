#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "semaphore_internal.h"
#include "time_internal.h"
#include "wait_object_internal.h"

#define TEST_STACK_BYTES 256u
#define TEST_TASK_LIMIT  8u

static _Alignas(RTS_TASK_STACK_ALIGNMENT)
    unsigned char test_stacks[TEST_TASK_LIMIT][TEST_STACK_BYTES];
static int test_failures;
static int first_failure_line;
static unsigned int assertion_count;

#define CHECK(condition)                                                   \
    do                                                                     \
    {                                                                      \
        if (!(condition))                                                  \
        {                                                                  \
            ++test_failures;                                               \
            if (first_failure_line == 0)                                   \
            {                                                              \
                first_failure_line = __LINE__;                             \
            }                                                              \
        }                                                                  \
    } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

static void task_entry(void *argument)
{
    (void)argument;
}

static void reset_environment(void)
{
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
}

static rts_task_handle_t create_task(size_t index, rts_priority_t priority)
{
    rts_task_config_t config = {
        task_entry, NULL, test_stacks[index], sizeof test_stacks[index], priority
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
}

static void complete_pending_switch(void)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_kernel_state_get()->switch_plan.pending);
    if (rts_host_port_test_switch_request_pending())
    {
        rts_host_port_test_consume_switch_request();
    }
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_switch_complete(&snapshot);
}

static bool advance_from_isr(rts_tick_t elapsed)
{
    bool notify;

    rts_host_port_test_set_isr(true);
    notify = rts_kernel_tick_advance(elapsed);
    if (notify)
    {
        rts_port_request_context_switch();
    }
    rts_host_port_test_set_isr(false);
    return notify;
}

static rts_status_t give_from_isr(rts_semaphore_t *semaphore,
                                  bool *higher_woken)
{
    rts_status_t status;

    rts_host_port_test_set_isr(true);
    status = rts_semaphore_give_from_isr(semaphore, higher_woken);
    rts_host_port_test_set_isr(false);
    return status;
}

static void test_initialization_and_immediate_operations(void)
{
    rts_semaphore_t binary = {0};
    rts_semaphore_t counting = {0};
    bool flag = true;

    reset_environment();
    CHECK(rts_semaphore_init(NULL, 0u, 1u) == RTS_STATUS_INVALID_ARGUMENT);
    CHECK(rts_semaphore_init(&binary, 0u, 0u) == RTS_STATUS_INVALID_ARGUMENT);
    CHECK(rts_semaphore_init(&binary, 2u, 1u) == RTS_STATUS_INVALID_ARGUMENT);
    rts_host_port_test_set_isr(true);
    CHECK(rts_semaphore_init(&binary, 0u, 1u) == RTS_STATUS_INVALID_CONTEXT);
    rts_host_port_test_set_isr(false);
    CHECK(rts_semaphore_init(&binary, 0u, 1u) == RTS_STATUS_OK);
    CHECK(rts_semaphore_is_valid(&binary));
    CHECK(binary.count == 0u && binary.maximum_count == 1u);
    CHECK(rts_wait_object_is_empty(&binary.waiters));
    CHECK(rts_semaphore_init(&binary, 0u, 1u) ==
          RTS_STATUS_ALREADY_INITIALIZED);
    CHECK(rts_semaphore_init(&counting, 2u, 4u) == RTS_STATUS_OK);
    CHECK(counting.count == 2u && counting.maximum_count == 4u);

    CHECK(rts_init() == RTS_STATUS_OK);
    (void)create_task(0u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_semaphore_take(&counting, 0u) == RTS_STATUS_OK);
    CHECK(counting.count == 1u);
    CHECK(rts_semaphore_give(&counting) == RTS_STATUS_OK);
    CHECK(counting.count == 2u);
    CHECK(rts_semaphore_take(&binary, 0u) == RTS_STATUS_TIMEOUT);
    CHECK(rts_semaphore_give(&binary) == RTS_STATUS_OK);
    CHECK(binary.count == 1u);
    CHECK(rts_semaphore_give(&binary) == RTS_STATUS_FULL);
    CHECK(binary.count == 1u);
    CHECK(!rts_kernel_state_get()->switch_plan.pending);
    CHECK(rts_host_port_test_switch_request_count() == 0u);

    rts_host_port_test_set_isr(true);
    CHECK(rts_semaphore_give(&counting) == RTS_STATUS_INVALID_CONTEXT);
    rts_host_port_test_set_isr(false);
    CHECK(rts_semaphore_give_from_isr(&counting, &flag) ==
          RTS_STATUS_INVALID_CONTEXT);
    CHECK(!flag);
}

static void test_blocking_forever_and_direct_handoff_order(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_semaphore_t semaphore = {0};
    rts_task_handle_t a;
    rts_task_handle_t c;
    rts_task_handle_t b;
    bool higher_woken;

    reset_environment();
    CHECK(rts_semaphore_init(&semaphore, 0u, 1u) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 5u);
    c = create_task(1u, 5u);
    b = create_task(2u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);

    CHECK(rts_semaphore_take(&semaphore, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    CHECK(a->state == RTS_TASK_STATE_BLOCKED);
    CHECK(a->wait.reason == RTS_WAIT_SEMAPHORE && a->wait.object == &semaphore);
    CHECK(!a->wait.timeout_active && a->delay_node.owner == NULL);
    CHECK(rts_wait_object_contains(&semaphore.waiters, a));
    complete_pending_switch();
    CHECK(kernel->current_task == c);

    CHECK(rts_semaphore_take(&semaphore, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == b);
    CHECK(rts_semaphore_take(&semaphore, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == kernel->idle_task);
    CHECK(semaphore.waiters.head == a && a->wait_node.next == c);
    CHECK(c->wait_node.next == b && semaphore.waiters.tail == b);
    CHECK(semaphore.waiters.count == 3u);

    CHECK(give_from_isr(&semaphore, &higher_woken) == RTS_STATUS_OK);
    CHECK(higher_woken);
    CHECK(semaphore.count == 0u && semaphore.waiters.count == 2u);
    CHECK(a->wait.result == RTS_WAIT_RESULT_ACQUIRED);
    CHECK(a->state == RTS_TASK_STATE_READY && a->wait_node.owner == NULL);
    CHECK(!rts_delay_contains(&kernel->delay_queue, a));
    CHECK(rts_host_port_test_switch_request_count() == 3u);
    rts_port_request_context_switch();
    complete_pending_switch();
    CHECK(kernel->current_task == a);
    CHECK(rts_semaphore_wait_result_consume(a) == RTS_STATUS_OK);

    /* Equal-priority C is appended READY but does not preempt A. */
    CHECK(rts_semaphore_give(&semaphore) == RTS_STATUS_OK);
    CHECK(kernel->current_task == a && !kernel->switch_plan.pending);
    CHECK(c->state == RTS_TASK_STATE_READY);
    CHECK(semaphore.waiters.head == b && semaphore.waiters.count == 1u);
    CHECK(rts_semaphore_wait_result_consume(c) == RTS_STATUS_OK);
}

static void test_finite_timeout_wrap_and_race_arbitration(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_semaphore_t semaphore = {0};
    rts_task_handle_t high;
    rts_task_handle_t low;
    bool higher_woken;

    reset_environment();
    CHECK(rts_semaphore_init(&semaphore, 0u, 2u) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 6u);
    low = create_task(1u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_C(0xfffffff0);

    CHECK(rts_semaphore_take(&semaphore, UINT32_C(0x20)) == RTS_STATUS_OK);
    CHECK(high->wait.wake_tick == UINT32_C(0x10));
    CHECK(high->wait.timeout_active);
    CHECK(rts_delay_contains(&kernel->delay_queue, high));
    complete_pending_switch();
    CHECK(!advance_from_isr(UINT32_C(0x1f)));
    CHECK(give_from_isr(&semaphore, &higher_woken) == RTS_STATUS_OK);
    CHECK(higher_woken);
    CHECK(high->wait.result == RTS_WAIT_RESULT_ACQUIRED);
    CHECK(!rts_delay_contains(&kernel->delay_queue, high));
    rts_port_request_context_switch();
    complete_pending_switch();
    CHECK(rts_semaphore_wait_result_consume(high) == RTS_STATUS_OK);
    CHECK(!advance_from_isr(1u));
    CHECK(high->state == RTS_TASK_STATE_RUNNING);

    /* Timeout wins: a later give increments count and cannot wake twice. */
    CHECK(rts_semaphore_take(&semaphore, 3u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(advance_from_isr(3u));
    CHECK(high->wait.result == RTS_WAIT_RESULT_TIMEOUT);
    CHECK(high->wait_node.owner == NULL && high->delay_node.owner == NULL);
    CHECK(semaphore.waiters.count == 0u && semaphore.count == 0u);
    CHECK(give_from_isr(&semaphore, &higher_woken) == RTS_STATUS_OK);
    CHECK(!higher_woken && semaphore.count == 1u);
    complete_pending_switch();
    CHECK(kernel->current_task == high);
    CHECK(rts_semaphore_wait_result_consume(high) == RTS_STATUS_TIMEOUT);
    CHECK(low->state == RTS_TASK_STATE_READY);
}

static void test_timeout_removal_from_middle_and_slicing(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_semaphore_t semaphore = {0};
    rts_task_handle_t first;
    rts_task_handle_t middle;
    rts_task_handle_t last;

    reset_environment();
    CHECK(rts_semaphore_init(&semaphore, 0u, 1u) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    first = create_task(0u, 5u);
    middle = create_task(1u, 5u);
    last = create_task(2u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_semaphore_take(&semaphore, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(rts_semaphore_take(&semaphore, 2u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(rts_semaphore_take(&semaphore, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == kernel->idle_task);
    CHECK(semaphore.waiters.head == first && first->wait_node.next == middle);
    CHECK(middle->wait_node.next == last);

    CHECK(advance_from_isr(2u));
    CHECK(middle->wait.result == RTS_WAIT_RESULT_TIMEOUT);
    CHECK(first->wait_node.next == last && last->wait_node.previous == first);
    CHECK(semaphore.waiters.count == 2u);
    complete_pending_switch();
    CHECK(kernel->current_task == middle);
    CHECK(rts_semaphore_wait_result_consume(middle) == RTS_STATUS_TIMEOUT);

    /* A same-priority direct handoff enters the tail and waits for slicing. */
    CHECK(rts_semaphore_give(&semaphore) == RTS_STATUS_OK);
    CHECK(first->state == RTS_TASK_STATE_READY);
    CHECK(kernel->current_task == middle && !kernel->switch_plan.pending);
#if RTS_ENABLE_TIME_SLICING
    CHECK(advance_from_isr((rts_tick_t)RTS_TIME_SLICE_TICKS));
    CHECK(kernel->switch_plan.to == first);
#endif
}

static void test_deterministic_stress(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_semaphore_t semaphore = {0};
    rts_task_handle_t task;
    size_t index;

    reset_environment();
    CHECK(rts_semaphore_init(&semaphore, 1u, 1u) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    task = create_task(0u, 4u);
    CHECK(rts_start() == RTS_STATUS_OK);

    for (index = 0u; index < 2000u; ++index)
    {
        CHECK(rts_semaphore_take(&semaphore, 0u) == RTS_STATUS_OK);
        CHECK(semaphore.count == 0u);
        CHECK(rts_semaphore_give(&semaphore) == RTS_STATUS_OK);
        CHECK(semaphore.count == 1u);
        CHECK(semaphore.waiters.count == 0u);
        CHECK(task == kernel->current_task);
        CHECK(rts_scheduler_current_is_valid());
        CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
        CHECK(rts_wait_object_validate(&semaphore.waiters));
    }
}

int main(void)
{
    test_initialization_and_immediate_operations();
    test_blocking_forever_and_direct_handoff_order();
    test_finite_timeout_wrap_and_race_arbitration();
    test_timeout_removal_from_middle_and_slicing();
    test_deterministic_stress();
    CHECK(assertion_count == 0u);
    return test_failures == 0 ? 0 : first_failure_line;
}
