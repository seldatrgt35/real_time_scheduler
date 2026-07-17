#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"
#include "time_internal.h"

#define TEST_STACK_BYTES 256u
#define TEST_TASK_LIMIT  8u

static _Alignas(RTS_TASK_STACK_ALIGNMENT)
    unsigned char test_stacks[TEST_TASK_LIMIT][TEST_STACK_BYTES];
static int test_failures;
static unsigned int assertion_count;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

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
        task_entry, NULL, test_stacks[index], sizeof test_stacks[index],
        priority, 0u, 0u, 0u
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
}

static void complete_pending_switch(void)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_kernel_state_get()->switch_plan.pending);
    rts_host_port_test_consume_switch_request();
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_switch_complete(&snapshot);
}

static bool advance_from_test_isr(rts_tick_t elapsed)
{
    bool notify;

    rts_host_port_test_set_isr(true);
    notify = rts_kernel_tick_advance(elapsed);
    if (notify)
    {
        rts_port_request_reschedule(rts_cpu_current_id());
    }
    rts_host_port_test_set_isr(false);
    return notify;
}

static void block_ready_task_for_test(rts_task_handle_t task,
                                      rts_tick_t wake_tick)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    CHECK(task != kernel->current_task);
    CHECK(task->state == RTS_TASK_STATE_READY);
    rts_ready_remove(&kernel->ready_set, task);
    task->wait.reason = RTS_WAIT_DELAY;
    task->wait.wake_tick = wake_tick;
    task->slice_remaining = 1u;
    task->state = RTS_TASK_STATE_BLOCKED;
    rts_delay_insert(&kernel->delay_queue, task);
    CHECK(rts_scheduler_task_is_blocked_delay(task));
}

static void test_delay_validation_and_zero_yield(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;

    reset_environment();
    CHECK(rts_task_delay(1u) == RTS_STATUS_INVALID_STATE);
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_task_delay(1u) == RTS_STATUS_INVALID_STATE);
    a = create_task(0u, 4u);
    b = create_task(1u, 4u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == a);

    rts_host_port_test_set_isr(true);
    CHECK(rts_task_delay(1u) == RTS_STATUS_INVALID_CONTEXT);
    rts_host_port_test_set_isr(false);
    CHECK(rts_task_delay(UINT32_C(0x80000000)) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(a->state == RTS_TASK_STATE_RUNNING);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(!kernel->switch_plan.pending);

    CHECK(rts_task_delay(0u) == RTS_STATUS_OK);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(a->wait.reason == RTS_WAIT_NONE);
    CHECK(kernel->switch_plan.from == a && kernel->switch_plan.to == b);
}

static void test_block_switch_wake_and_wrap(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 5u);
    b = create_task(1u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_C(0xfffffff0);

    CHECK(rts_task_delay(UINT32_C(0x20)) == RTS_STATUS_OK);
    CHECK(kernel->current_task == a);
    CHECK(a->state == RTS_TASK_STATE_BLOCKED);
    CHECK(a->wait.reason == RTS_WAIT_DELAY);
    CHECK(a->wait.wake_tick == UINT32_C(0x10));
    CHECK(!rts_ready_contains(&kernel->ready_set, a));
    CHECK(rts_delay_contains(&kernel->delay_queue, a));
    CHECK(kernel->switch_plan.from == a && kernel->switch_plan.to == b);
    CHECK(rts_host_port_test_switch_request_count() == 1u);

    complete_pending_switch();
    CHECK(kernel->current_task == b);
    CHECK(a->state == RTS_TASK_STATE_BLOCKED);
    CHECK(b->state == RTS_TASK_STATE_RUNNING);
    CHECK(!advance_from_test_isr(UINT32_C(0x1f)));
    CHECK(a->state == RTS_TASK_STATE_BLOCKED);
    CHECK(advance_from_test_isr(1u));
    CHECK(kernel->current_tick == UINT32_C(0x10));
    CHECK(a->state == RTS_TASK_STATE_READY);
    CHECK(a->wait.reason == RTS_WAIT_NONE && a->wait.wake_tick == 0u);
    CHECK(a->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(rts_ready_contains(&kernel->ready_set, a));
    CHECK(!rts_delay_contains(&kernel->delay_queue, a));
    CHECK(kernel->switch_plan.from == b && kernel->switch_plan.to == a);
    CHECK(rts_host_port_test_switch_request_count() == 2u);
    complete_pending_switch();
    CHECK(kernel->current_task == a && a->state == RTS_TASK_STATE_RUNNING);
    CHECK(a->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(b->state == RTS_TASK_STATE_READY);
}

static void test_equal_lower_and_fifo_wakeup(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t current;
    rts_task_handle_t first;
    rts_task_handle_t second;
    rts_task_handle_t lower;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    current = create_task(0u, 5u);
    first = create_task(1u, 5u);
    second = create_task(2u, 5u);
    lower = create_task(3u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    block_ready_task_for_test(first, 7u);
    block_ready_task_for_test(second, 7u);
    block_ready_task_for_test(lower, 7u);

    CHECK(!advance_from_test_isr(7u));
    CHECK(kernel->current_task == current);
    CHECK(!kernel->switch_plan.pending);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(current->ready_node.next == &first->ready_node);
    CHECK(first->ready_node.next == &second->ready_node);
    CHECK(lower->state == RTS_TASK_STATE_READY);
    CHECK(rts_host_port_test_switch_request_count() == 0u);
}

static void test_expiry_before_pending_transfer_cancels_plan(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 5u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_task_delay(1u) == RTS_STATUS_OK);
    CHECK(kernel->switch_plan.pending);
    CHECK(kernel->current_task == a && a->state == RTS_TASK_STATE_BLOCKED);

    CHECK(!advance_from_test_isr(1u));
    CHECK(kernel->current_task == a && a->state == RTS_TASK_STATE_RUNNING);
    CHECK(a->wait.reason == RTS_WAIT_NONE && a->wait.wake_tick == 0u);
    CHECK(rts_ready_contains(&kernel->ready_set, a));
    CHECK(!rts_delay_contains(&kernel->delay_queue, a));
    CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void test_pending_plan_coalesces_one_notification(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;
    rts_task_handle_t c;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    c = create_task(0u, 7u);
    b = create_task(1u, 5u);
    a = create_task(2u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == c);

    /* Private fixture: A is actual current while B is already selected. */
    c->state = RTS_TASK_STATE_READY;
    a->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = a;
    CHECK(rts_scheduler_current_is_valid());
    block_ready_task_for_test(c, 1u);
    rts_scheduler_request_switch_if_needed(b);
    CHECK(rts_host_port_test_switch_request_count() == 1u);

    CHECK(!advance_from_test_isr(1u));
    CHECK(kernel->switch_plan.pending);
    CHECK(kernel->switch_plan.from == a && kernel->switch_plan.to == c);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void test_all_tasks_delayed_idle_preemption(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;
    unsigned int before;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 5u);
    b = create_task(1u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_task_delay(5u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == b);
    CHECK(rts_task_delay(10u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == kernel->idle_task);

    before = assertion_count;
    CHECK(rts_task_delay(1u) == RTS_STATUS_INVALID_STATE);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    (void)before;
#endif
    CHECK(!rts_delay_contains(&kernel->delay_queue, kernel->idle_task));

    CHECK(advance_from_test_isr(5u));
    CHECK(kernel->switch_plan.from == kernel->idle_task);
    CHECK(kernel->switch_plan.to == a);
    complete_pending_switch();
    CHECK(kernel->current_task == a);
    CHECK(!advance_from_test_isr(5u));
    CHECK(b->state == RTS_TASK_STATE_READY);
    CHECK(kernel->current_task == a);
}

int main(void)
{
    test_delay_validation_and_zero_yield();
    test_block_switch_wake_and_wrap();
    test_equal_lower_and_fifo_wakeup();
    test_expiry_before_pending_transfer_cancels_plan();
    test_pending_plan_coalesces_one_notification();
    test_all_tasks_delayed_idle_preemption();
    return test_failures;
}
