#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u
#define ROTATION_ITERATIONS 48u

typedef struct
{
    rts_task_handle_t a;
    rts_task_handle_t b;
    rts_task_handle_t c;
} yield_fixture_t;

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
}

static rts_task_handle_t create_task(void *stack, size_t size,
                                     rts_priority_t priority)
{
    rts_task_config_t config = {
        .entry = task_entry,
        .argument = NULL,
        .stack_buffer = stack,
        .stack_size_bytes = size,
        .priority = priority,
        .period = 0u,
        .relative_deadline = 0u,
        .execution_budget = 0u
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
}

static yield_fixture_t create_fixture(size_t count, bool lower_priority_c)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stacks[3][TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t fixture = {NULL, NULL, NULL};

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    if (count > 0u)
    {
        fixture.a = create_task(stacks[0], sizeof stacks[0], 5u);
    }
    if (count > 1u)
    {
        fixture.b = create_task(stacks[1], sizeof stacks[1],
                                lower_priority_c ? 4u : 5u);
    }
    if (count > 2u)
    {
        fixture.c = create_task(stacks[2], sizeof stacks[2],
                                lower_priority_c ? 3u : 5u);
    }
    CHECK(rts_scheduler_current_establish(count == 0u
                                              ? kernel->idle_task
                                              : fixture.a));
    kernel->lifecycle = RTS_KERNEL_RUNNING;
    return fixture;
}

static void complete_requested_switch(rts_tcb_t *from, rts_tcb_t *to)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_host_port_test_switch_request_pending());
    rts_host_port_test_consume_switch_request();
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    CHECK(snapshot.from == from);
    CHECK(snapshot.to == to);
    rts_scheduler_switch_complete(&snapshot);
}

static void test_public_rejections(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    reset_environment();
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
    CHECK(kernel->current_task == NULL);
    CHECK(!kernel->switch_plan.pending);
    CHECK(rts_host_port_test_switch_request_count() == 0u);

    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(kernel->current_task == NULL);
    CHECK(!kernel->switch_plan.pending);

    rts_host_port_test_set_isr(true);
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_CONTEXT);
    CHECK(kernel->current_task == NULL);
    CHECK(!kernel->switch_plan.pending);
    CHECK(rts_host_port_test_switch_request_count() == 0u);
}

static void test_no_peer_lower_priority_and_idle(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t f = create_fixture(3u, true);
    rts_list_node_t *head = kernel->ready_set.priority_queue[5].head;
    f.a->slice_remaining = 3u;

    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(kernel->current_task == f.a);
    CHECK(f.a->state == RTS_TASK_STATE_RUNNING);
    CHECK(kernel->ready_set.priority_queue[5].head == head);
    CHECK(rts_scheduler_select_highest_ready() == f.a);
    CHECK(!kernel->switch_plan.pending);
    CHECK(rts_host_port_test_switch_request_count() == 0u);
    CHECK(f.a->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(f.c->priority == 3u);

    (void)create_fixture(0u, false);
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(kernel->current_task == kernel->idle_task);
    CHECK(kernel->idle_task->state == RTS_TASK_STATE_RUNNING);
    CHECK(!kernel->switch_plan.pending);
    CHECK(rts_host_port_test_switch_request_count() == 0u);
}

static void test_three_peer_yield_and_completion(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t f = create_fixture(3u, false);
    size_t count = kernel->ready_set.priority_queue[5].count;
    uint32_t bitmap = kernel->ready_set.ready_bitmap[0];
    void *a_sp = f.a->saved_stack_pointer;
    void *b_sp = f.b->saved_stack_pointer;

    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(kernel->ready_set.priority_queue[5].head == &f.b->ready_node);
    CHECK(f.b->ready_node.next == &f.c->ready_node);
    CHECK(f.c->ready_node.next == &f.a->ready_node);
    CHECK(kernel->switch_plan.pending && !kernel->switch_plan.active);
    CHECK(kernel->switch_plan.from == f.a && kernel->switch_plan.to == f.b);
    CHECK(kernel->current_task == f.a);
    CHECK(f.a->state == RTS_TASK_STATE_RUNNING);
    CHECK(f.b->state == RTS_TASK_STATE_READY);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    CHECK(rts_host_port_test_last_switch_request_critical_depth() == 0u);
    CHECK(kernel->ready_set.priority_queue[5].count == count);
    CHECK(kernel->ready_set.ready_bitmap[0] == bitmap);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(f.a->saved_stack_pointer == a_sp && f.b->saved_stack_pointer == b_sp);

    complete_requested_switch(f.a, f.b);
    CHECK(kernel->current_task == f.b);
    CHECK(f.a->state == RTS_TASK_STATE_READY);
    CHECK(f.b->state == RTS_TASK_STATE_RUNNING);
    CHECK(kernel->ready_set.priority_queue[5].head == &f.b->ready_node);
    CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    CHECK(kernel->lifecycle == RTS_KERNEL_RUNNING);
}

static void test_bounded_round_robin(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t f = create_fixture(3u, false);
    rts_tcb_t *order[3] = {f.a, f.b, f.c};
    size_t iteration;

    for (iteration = 0u; iteration < ROTATION_ITERATIONS; ++iteration)
    {
        rts_tcb_t *from = order[iteration % 3u];
        rts_tcb_t *to = order[(iteration + 1u) % 3u];
        CHECK(kernel->current_task == from);
        CHECK(rts_task_yield() == RTS_STATUS_OK);
        complete_requested_switch(from, to);
        CHECK(kernel->current_task == to);
        CHECK(kernel->ready_set.priority_queue[5].count == 3u);
        CHECK(rts_ready_contains(&kernel->ready_set, f.a));
        CHECK(rts_ready_contains(&kernel->ready_set, f.b));
        CHECK(rts_ready_contains(&kernel->ready_set, f.c));
        CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    }
    CHECK(rts_host_port_test_switch_request_count() == ROTATION_ITERATIONS);
}

static void test_critical_nesting_and_request_order(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t f = create_fixture(2u, false);
    rts_critical_token_t outer = rts_port_critical_enter();

    CHECK(rts_host_port_test_critical_depth() == 1u);
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(rts_host_port_test_critical_depth() == 1u);
    CHECK(kernel->current_task == f.a);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    CHECK(rts_host_port_test_last_switch_request_critical_depth() == 1u);
    rts_port_critical_exit(outer);
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

#if RTS_ENABLE_ASSERTIONS
static void test_internal_invariant_guards(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    yield_fixture_t f = create_fixture(2u, false);
    unsigned int before;

    kernel->current_task = NULL;
    before = assertion_count;
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_host_port_test_critical_depth() == 0u);
    kernel->current_task = f.a;

    f.a->state = RTS_TASK_STATE_READY;
    before = assertion_count;
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    f.a->state = RTS_TASK_STATE_RUNNING;

    rts_ready_rotate(&kernel->ready_set, f.a->priority);
    before = assertion_count;
    CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    CHECK(!kernel->switch_plan.pending);

    (void)create_fixture(2u, false);
    f.a = kernel->current_task;
    f.b = (rts_tcb_t *)f.a->ready_node.next->object;
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    {
        rts_switch_snapshot_t snapshot;
        CHECK(rts_scheduler_switch_acquire(&snapshot));
        before = assertion_count;
        CHECK(rts_task_yield() == RTS_STATUS_INVALID_STATE);
        CHECK(assertion_count == before + 1u);
        CHECK(kernel->switch_plan.active);
        CHECK(kernel->switch_plan.from == f.a && kernel->switch_plan.to == f.b);
    }
}
#endif

int main(void)
{
    test_public_rejections();
    test_no_peer_lower_priority_and_idle();
    test_three_peer_yield_and_completion();
    test_bounded_round_robin();
    test_critical_nesting_and_request_order();
#if RTS_ENABLE_ASSERTIONS
    test_internal_invariant_guards();
#endif
    return test_failures;
}
