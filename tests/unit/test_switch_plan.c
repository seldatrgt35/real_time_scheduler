#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u

typedef struct
{
    rts_task_handle_t a;
    rts_task_handle_t b;
    rts_task_handle_t c;
} switch_fixture_t;

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

static void task_entry(void *argument)
{
    (void)argument;
}

static switch_fixture_t fixture_create(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char a_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char b_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char c_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t a_config = {
        task_entry, NULL, a_stack, sizeof a_stack, 2u
    };
    rts_task_config_t b_config = {
        task_entry, NULL, b_stack, sizeof b_stack, 5u
    };
    rts_task_config_t c_config = {
        task_entry, NULL, c_stack, sizeof c_stack, 7u
    };
    switch_fixture_t fixture = {NULL, NULL, NULL};

    *kernel = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_task_create(&a_config, &fixture.a) == RTS_STATUS_OK);
    CHECK(rts_task_create(&b_config, &fixture.b) == RTS_STATUS_OK);
    CHECK(rts_task_create(&c_config, &fixture.c) == RTS_STATUS_OK);

    /* Private RUNNING fixture: A was dispatched before B/C became eligible. */
    fixture.a->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = fixture.a;
    kernel->lifecycle = RTS_KERNEL_RUNNING;
    CHECK(rts_scheduler_current_is_valid());
    return fixture;
}

static void check_pending(rts_tcb_t *from, rts_tcb_t *to)
{
    const rts_switch_plan_t *plan = &rts_kernel_state_get()->switch_plan;

    CHECK(plan->pending);
    CHECK(!plan->active);
    CHECK(plan->from == from);
    CHECK(plan->to == to);
}

static void test_prepare_coalesce_notify_and_cancel(void)
{
    switch_fixture_t f = fixture_create();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    uint32_t generation;

    CHECK(!rts_scheduler_prepare_switch(f.a));
    CHECK(!kernel->switch_plan.pending);

    rts_scheduler_request_switch_if_needed(f.b);
    check_pending(f.a, f.b);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    CHECK(rts_host_port_test_switch_request_pending());
    generation = kernel->switch_plan.generation;

    rts_scheduler_request_switch_if_needed(f.b);
    check_pending(f.a, f.b);
    CHECK(kernel->switch_plan.generation == generation);
    CHECK(rts_host_port_test_switch_request_count() == 1u);

    rts_scheduler_request_switch_if_needed(f.c);
    check_pending(f.a, f.c);
    CHECK(kernel->switch_plan.generation == generation + 1u);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    CHECK(kernel->current_task == f.a);
    CHECK(f.a->state == RTS_TASK_STATE_RUNNING);
    CHECK(f.b->state == RTS_TASK_STATE_READY);
    CHECK(f.c->state == RTS_TASK_STATE_READY);

    rts_scheduler_request_switch_if_needed(f.a);
    CHECK(!kernel->switch_plan.pending);
    CHECK(!kernel->switch_plan.active);
    CHECK(kernel->switch_plan.from == NULL && kernel->switch_plan.to == NULL);
    CHECK(kernel->current_task == f.a);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    CHECK(rts_host_port_test_switch_request_pending());
}

static void test_snapshot_and_completion(void)
{
    switch_fixture_t f = fixture_create();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_snapshot_t snapshot;
    rts_list_node_t *a_next = f.a->ready_node.next;
    rts_list_node_t *b_previous = f.b->ready_node.previous;
    unsigned int before;

    CHECK(!rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_request_switch_if_needed(f.b);
    rts_host_port_test_consume_switch_request();
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    CHECK(snapshot.from == f.a && snapshot.to == f.b);
    CHECK(kernel->switch_plan.active && !kernel->switch_plan.pending);
    CHECK(kernel->current_task == f.a);

#if RTS_ENABLE_ASSERTIONS
    before = assertion_count;
    CHECK(!rts_scheduler_switch_acquire(&snapshot));
    CHECK(assertion_count == before + 1u);
#else
    (void)before;
#endif

    rts_scheduler_switch_complete(&snapshot);
    CHECK(kernel->current_task == f.b);
    CHECK(f.a->state == RTS_TASK_STATE_READY);
    CHECK(f.b->state == RTS_TASK_STATE_RUNNING);
    CHECK(rts_ready_contains(&kernel->ready_set, f.a));
    CHECK(rts_ready_contains(&kernel->ready_set, f.b));
    CHECK(f.a->ready_node.next == a_next);
    CHECK(f.b->ready_node.previous == b_previous);
    CHECK(!kernel->switch_plan.active && !kernel->switch_plan.pending);
    CHECK(kernel->switch_plan.from == NULL && kernel->switch_plan.to == NULL);
    CHECK(kernel->lifecycle == RTS_KERNEL_RUNNING);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void test_active_snapshot_defers_reselection(void)
{
    switch_fixture_t f = fixture_create();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_snapshot_t snapshot;
    uint32_t generation;

    rts_scheduler_request_switch_if_needed(f.b);
    rts_host_port_test_consume_switch_request();
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    generation = snapshot.generation;

    rts_scheduler_request_switch_if_needed(f.a);
    CHECK(kernel->switch_plan.active);
    CHECK(kernel->switch_plan.from == f.a && kernel->switch_plan.to == f.b);
    CHECK(rts_scheduler_switch_reselection_required());
    CHECK(rts_host_port_test_switch_request_count() == 1u);

    rts_scheduler_request_switch_if_needed(f.c);
    CHECK(kernel->switch_plan.active);
    CHECK(kernel->switch_plan.from == f.a && kernel->switch_plan.to == f.b);
    CHECK(kernel->switch_plan.generation == generation);
    CHECK(snapshot.from == f.a && snapshot.to == f.b &&
          snapshot.generation == generation);
    CHECK(rts_scheduler_switch_reselection_required());
    CHECK(rts_host_port_test_switch_request_count() == 1u);

    rts_scheduler_switch_complete(&snapshot);
    CHECK(kernel->current_task == f.b);
    CHECK(rts_scheduler_switch_reselection_required());
    CHECK(rts_scheduler_select_highest_ready() == f.c);
    rts_scheduler_request_switch_if_needed(f.c);
    check_pending(f.b, f.c);
    CHECK(!rts_scheduler_switch_reselection_required());
    CHECK(rts_host_port_test_switch_request_count() == 2u);
}

#if RTS_ENABLE_ASSERTIONS
static void test_invalid_preparation(void)
{
    switch_fixture_t f = fixture_create();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    unsigned int before;

    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(NULL));
    CHECK(assertion_count == before + 1u);

    kernel->lifecycle = RTS_KERNEL_INITIALIZED;
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count == before + 1u);
    kernel->lifecycle = RTS_KERNEL_RUNNING;

    kernel->current_task = NULL;
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count >= before + 1u);
    kernel->current_task = f.a;

    f.b->state = RTS_TASK_STATE_BLOCKED;
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count >= before + 1u);
    f.b->state = RTS_TASK_STATE_READY;

    f.b->state = RTS_TASK_STATE_DORMANT;
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count >= before + 1u);
    f.b->state = RTS_TASK_STATE_READY;

    f.b->slot_state = RTS_TASK_SLOT_RESERVED;
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count >= before + 1u);
    f.b->slot_state = RTS_TASK_SLOT_ALLOCATED;

    rts_ready_remove(&kernel->ready_set, f.b);
    before = assertion_count;
    CHECK(!rts_scheduler_prepare_switch(f.b));
    CHECK(assertion_count >= before + 1u);
}

static void test_stale_and_mismatched_completion(void)
{
    switch_fixture_t f = fixture_create();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_snapshot_t snapshot;
    rts_switch_snapshot_t invalid;
    unsigned int before;

    CHECK(rts_scheduler_prepare_switch(f.b));
    CHECK(rts_scheduler_switch_acquire(&snapshot));

    invalid = snapshot;
    kernel->current_task = f.c;
    before = assertion_count;
    rts_scheduler_switch_complete(&invalid);
    CHECK(assertion_count == before + 1u);
    CHECK(f.a->state == RTS_TASK_STATE_RUNNING);
    kernel->current_task = f.a;

    invalid = snapshot;
    f.b->state = RTS_TASK_STATE_BLOCKED;
    before = assertion_count;
    rts_scheduler_switch_complete(&invalid);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == f.a);
    f.b->state = RTS_TASK_STATE_READY;

    invalid = snapshot;
    ++invalid.generation;
    before = assertion_count;
    rts_scheduler_switch_complete(&invalid);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == f.a);

    invalid = snapshot;
    invalid.to = f.c;
    before = assertion_count;
    rts_scheduler_switch_complete(&invalid);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == f.a);

    rts_scheduler_switch_complete(&snapshot);
    before = assertion_count;
    rts_scheduler_switch_complete(&snapshot);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == f.b);
}
#endif

int rts_test_switch_plan_run(void)
{
    test_prepare_coalesce_notify_and_cancel();
    test_snapshot_and_completion();
    test_active_snapshot_defers_reselection();
#if RTS_ENABLE_ASSERTIONS
    test_invalid_preparation();
    test_stale_and_mismatched_completion();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_switch_plan_run();
}
