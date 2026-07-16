#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u
#define TEST_TASK_LIMIT  6u

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
    CHECK(task != NULL);
    CHECK(task == NULL ||
          task->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    return task;
}

static bool advance_tick(rts_tick_t elapsed)
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

#if RTS_ENABLE_TIME_SLICING
static void complete_switch(void)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_kernel_state_get()->switch_plan.pending);
    rts_host_port_test_consume_switch_request();
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_switch_complete(&snapshot);
    CHECK(snapshot.to->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
}

static void block_ready_for_test(rts_task_handle_t task, rts_tick_t wake)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    rts_ready_remove(&kernel->ready_set, task);
    task->wait.reason = RTS_WAIT_DELAY;
    task->wait.wake_tick = wake;
    task->state = RTS_TASK_STATE_BLOCKED;
    rts_delay_insert(&kernel->delay_queue, task);
}
#endif

static void test_no_peer_never_admits_lower_priority(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t current;
    rts_task_handle_t lower;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    current = create_task(0u, 5u);
    lower = create_task(1u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == current);

    CHECK(!advance_tick((rts_tick_t)(RTS_TIME_SLICE_TICKS + 3u)));
    CHECK(kernel->current_task == current);
    CHECK(current->state == RTS_TASK_STATE_RUNNING);
    CHECK(lower->state == RTS_TASK_STATE_READY);
    CHECK(!kernel->switch_plan.pending);
#if RTS_ENABLE_TIME_SLICING
    CHECK(current->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
#endif
}

static void test_partial_and_three_task_rotation(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t tasks[3];
    size_t index;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    tasks[0] = create_task(0u, 4u);
    tasks[1] = create_task(1u, 4u);
    tasks[2] = create_task(2u, 4u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == tasks[0]);

#if RTS_ENABLE_TIME_SLICING
    if (RTS_TIME_SLICE_TICKS > 1u)
    {
        CHECK(!advance_tick((rts_tick_t)(RTS_TIME_SLICE_TICKS - 1u)));
        CHECK(tasks[0]->slice_remaining == 1u);
        CHECK(!kernel->switch_plan.pending);
    }
    CHECK(advance_tick(1u));
    CHECK(kernel->switch_plan.from == tasks[0]);
    CHECK(kernel->switch_plan.to == tasks[1]);
    CHECK(kernel->current_task == tasks[0]);
    CHECK(kernel->ready_set.priority_queue[4].count == 3u);
    complete_switch();
    CHECK(kernel->current_task == tasks[1]);

    for (index = 0u; index < 8u; ++index)
    {
        rts_task_handle_t expected = tasks[(index + 2u) % 3u];

        CHECK(advance_tick((rts_tick_t)RTS_TIME_SLICE_TICKS));
        CHECK(kernel->switch_plan.to == expected);
        complete_switch();
        CHECK(kernel->current_task == expected);
        CHECK(kernel->ready_set.priority_queue[4].count == 3u);
        CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
        CHECK(kernel->application_task_pool.allocated_count == 3u);
    }
#else
    for (index = 0u; index < 20u; ++index)
    {
        CHECK(!advance_tick(1u));
    }
    CHECK(kernel->current_task == tasks[0]);
    CHECK(kernel->ready_set.priority_queue[4].head == &tasks[0]->ready_node);
    CHECK(!kernel->switch_plan.pending);
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(kernel->switch_plan.to == tasks[1]);
#endif
}

#if RTS_ENABLE_TIME_SLICING
static void test_large_elapsed_rotates_once(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 3u);
    b = create_task(1u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    a->slice_remaining = 2u;
    CHECK(advance_tick(10u));
    CHECK(kernel->switch_plan.from == a && kernel->switch_plan.to == b);
    CHECK(a->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(kernel->ready_set.priority_queue[3].head == &b->ready_node);
    CHECK(kernel->ready_set.priority_queue[3].tail == &a->ready_node);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void test_wakeup_and_slice_choose_once(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t high;
    rts_task_handle_t a;
    rts_task_handle_t b;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 7u);
    a = create_task(1u, 3u);
    b = create_task(2u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);

    high->state = RTS_TASK_STATE_READY;
    a->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = a;
    block_ready_for_test(high, (rts_tick_t)RTS_TIME_SLICE_TICKS);
    a->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    CHECK(rts_scheduler_current_is_valid());

    CHECK(advance_tick((rts_tick_t)RTS_TIME_SLICE_TICKS));
    CHECK(kernel->switch_plan.from == a && kernel->switch_plan.to == high);
    CHECK(kernel->ready_set.priority_queue[3].head == &b->ready_node);
    CHECK(kernel->ready_set.priority_queue[3].tail == &a->ready_node);
    CHECK(high->state == RTS_TASK_STATE_READY);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void test_bounded_rotation_stress(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;
    size_t event;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 4u);
    b = create_task(1u, 4u);
    CHECK(rts_start() == RTS_STATUS_OK);

    for (event = 0u; event < 2000u; ++event)
    {
        rts_task_handle_t expected = (event % 2u) == 0u ? b : a;

        CHECK(advance_tick((rts_tick_t)RTS_TIME_SLICE_TICKS));
        CHECK(kernel->switch_plan.to == expected);
        complete_switch();
        CHECK(kernel->current_task == expected);
        CHECK(kernel->ready_set.priority_queue[4].count == 2u);
        CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
        CHECK(rts_ready_contains(&kernel->ready_set, a));
        CHECK(rts_ready_contains(&kernel->ready_set, b));
        CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    }
}

static void test_mixed_delay_slice_wrap_stress(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t high;
    rts_task_handle_t peer_a;
    rts_task_handle_t peer_b;
    size_t cycle;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 6u);
    peer_a = create_task(1u, 3u);
    peer_b = create_task(2u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_C(0xfffffff0);

    for (cycle = 0u; cycle < 500u; ++cycle)
    {
        CHECK(kernel->current_task == high);
        CHECK(rts_task_delay(
                  (rts_tick_t)(RTS_TIME_SLICE_TICKS * 2u)) == RTS_STATUS_OK);
        complete_switch();
        CHECK(kernel->current_task == peer_a ||
              kernel->current_task == peer_b);
        CHECK(high->state == RTS_TASK_STATE_BLOCKED);

        CHECK(advance_tick((rts_tick_t)RTS_TIME_SLICE_TICKS));
        complete_switch();
        CHECK(high->state == RTS_TASK_STATE_BLOCKED);

        CHECK(advance_tick((rts_tick_t)RTS_TIME_SLICE_TICKS));
        CHECK(kernel->switch_plan.to == high);
        complete_switch();
        CHECK(kernel->current_task == high);
        CHECK(high->state == RTS_TASK_STATE_RUNNING);
        CHECK(peer_a->state == RTS_TASK_STATE_READY);
        CHECK(peer_b->state == RTS_TASK_STATE_READY);
        CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
        CHECK(kernel->ready_set.priority_queue[3].count == 2u);
        CHECK(kernel->application_task_pool.allocated_count == 3u);
        CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    }
}
#endif

int main(void)
{
    test_no_peer_never_admits_lower_priority();
    test_partial_and_three_task_rotation();
#if RTS_ENABLE_TIME_SLICING
    test_large_elapsed_rotates_once();
    test_wakeup_and_slice_choose_once();
    test_bounded_rotation_stress();
    test_mixed_delay_slice_wrap_stress();
#endif
    return test_failures;
}
