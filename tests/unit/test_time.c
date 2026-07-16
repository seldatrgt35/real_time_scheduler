#include <stdbool.h>
#include <stdint.h>

#include "scheduler_internal.h"
#include "time_internal.h"

static int test_failures;
static unsigned int assertion_count;
static bool test_in_isr;
static unsigned int switch_request_count;
static rts_kernel_state_t test_kernel;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

rts_kernel_state_t *rts_kernel_state_get(void)
{
    return &test_kernel;
}

bool rts_port_is_in_isr(void)
{
    return test_in_isr;
}

rts_tcb_t *rts_delay_peek_expired(const rts_delay_queue_t *delay_queue,
                                  rts_tick_t now)
{
    (void)delay_queue;
    (void)now;
    return NULL;
}

bool rts_task_handle_is_application_task(rts_task_handle_t handle)
{
    return handle != NULL;
}

bool rts_delay_contains(const rts_delay_queue_t *delay_queue,
                        const rts_tcb_t *task)
{
    (void)delay_queue;
    (void)task;
    return false;
}

void rts_delay_remove(rts_delay_queue_t *delay_queue, rts_tcb_t *task)
{
    (void)delay_queue;
    (void)task;
}

void rts_ready_insert(rts_ready_set_t *ready_set, rts_tcb_t *task)
{
    (void)ready_set;
    (void)task;
}

bool rts_scheduler_task_is_runnable(const rts_tcb_t *task)
{
    (void)task;
    return false;
}

bool rts_scheduler_task_is_blocked_delay(const rts_tcb_t *task)
{
    (void)task;
    return false;
}

bool rts_scheduler_task_is_idle(const rts_tcb_t *task)
{
    (void)task;
    return false;
}

bool rts_ready_is_front(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task)
{
    (void)ready_set;
    (void)task;
    return false;
}

bool rts_ready_has_peer(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task)
{
    (void)ready_set;
    (void)task;
    return false;
}

void rts_ready_rotate(rts_ready_set_t *ready_set, rts_priority_t priority)
{
    (void)ready_set;
    (void)priority;
}

rts_tcb_t *rts_scheduler_select_highest_ready(void)
{
    return NULL;
}

bool rts_scheduler_current_is_valid(void)
{
    return false;
}

bool rts_scheduler_prepare_switch(rts_tcb_t *next_task)
{
    (void)next_task;
    return false;
}

void rts_port_request_context_switch(void)
{
    ++switch_request_count;
}

static void reset_state(rts_kernel_lifecycle_t lifecycle, bool in_isr)
{
    test_kernel = (rts_kernel_state_t){0};
    test_kernel.lifecycle = lifecycle;
    test_in_isr = in_isr;
    switch_request_count = 0u;
}

static void test_wrap_helpers(void)
{
    CHECK(!rts_tick_before(0u, 0u));
    CHECK(rts_tick_before(0u, 1u));
    CHECK(!rts_tick_before(1u, 0u));
    CHECK(rts_tick_before(UINT32_MAX, 0u));
    CHECK(!rts_tick_before(0u, UINT32_MAX));
    CHECK(rts_tick_before(UINT32_C(0x80000000), 0u) == false);
    CHECK(rts_tick_before(0u, UINT32_C(0x80000000)) == false);
    CHECK(rts_tick_before(UINT32_C(0x7fffffff), UINT32_C(0x80000000)));
    CHECK(rts_tick_before(0u, UINT32_C(0x7fffffff)));
    CHECK(rts_tick_before(UINT32_C(0xfffffffe), UINT32_C(1)));

    CHECK(rts_tick_reached(0u, 0u));
    CHECK(rts_tick_reached(1u, 0u));
    CHECK(!rts_tick_reached(0u, 1u));
    CHECK(rts_tick_reached(0u, UINT32_MAX));
    CHECK(!rts_tick_reached(UINT32_C(0x80000000), 0u));

    CHECK(rts_tick_elapsed(0u, 1u) == 1u);
    CHECK(rts_tick_elapsed(UINT32_MAX, 0u) == 1u);
    CHECK(rts_tick_elapsed(UINT32_C(0xfffffffe), 1u) == 3u);
    CHECK(rts_tick_relative_is_valid(0u));
    CHECK(rts_tick_relative_is_valid(UINT32_C(0x7fffffff)));
    CHECK(!rts_tick_relative_is_valid(UINT32_C(0x80000000)));
    CHECK(!rts_tick_relative_is_valid(UINT32_MAX));
}

static void test_tick_advancement(void)
{
    reset_state(RTS_KERNEL_RUNNING, true);
    CHECK(rts_kernel_tick_now() == 0u);
    rts_kernel_tick_advance(1u);
    CHECK(rts_kernel_tick_now() == 1u);
    rts_kernel_tick_advance(9u);
    CHECK(rts_kernel_tick_now() == 10u);
    test_kernel.current_tick = UINT32_MAX;
    rts_kernel_tick_advance(1u);
    CHECK(rts_kernel_tick_now() == 0u);
    test_kernel.current_tick = UINT32_C(0xfffffffe);
    rts_kernel_tick_advance(5u);
    CHECK(rts_kernel_tick_now() == 3u);
}

static void test_invalid_entry_does_not_mutate(void)
{
    unsigned int before;

    reset_state(RTS_KERNEL_RESET, true);
    before = assertion_count;
    rts_kernel_tick_advance(1u);
    CHECK(test_kernel.current_tick == 0u);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    CHECK(assertion_count == before);
#endif

    reset_state(RTS_KERNEL_INITIALIZED, true);
    before = assertion_count;
    rts_kernel_tick_advance(1u);
    CHECK(test_kernel.current_tick == 0u);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    CHECK(assertion_count == before);
#endif

    reset_state(RTS_KERNEL_RUNNING, false);
    before = assertion_count;
    rts_kernel_tick_advance(1u);
    CHECK(test_kernel.current_tick == 0u);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    CHECK(assertion_count == before);
#endif

    reset_state(RTS_KERNEL_RUNNING, true);
    before = assertion_count;
    rts_kernel_tick_advance(0u);
    rts_kernel_tick_advance(UINT32_C(0x80000000));
    CHECK(test_kernel.current_tick == 0u);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 2u);
#else
    CHECK(assertion_count == before);
#endif
}

static void test_unrelated_state_is_unchanged(void)
{
    rts_tcb_t sentinel_task = {0};

    reset_state(RTS_KERNEL_RUNNING, true);
    test_kernel.current_task = &sentinel_task;
    test_kernel.ready_set.priority_queue[3].count = 2u;
    test_kernel.delay_queue.ordered_tasks.count = 4u;
    test_kernel.switch_plan.generation = 7u;
    rts_kernel_tick_advance(3u);
    CHECK(test_kernel.current_tick == 3u);
    CHECK(test_kernel.current_task == &sentinel_task);
    CHECK(test_kernel.ready_set.priority_queue[3].count == 2u);
    CHECK(test_kernel.delay_queue.ordered_tasks.count == 4u);
    CHECK(test_kernel.switch_plan.generation == 7u);
    CHECK(!test_kernel.switch_plan.pending && !test_kernel.switch_plan.active);
    CHECK(switch_request_count == 0u);
}

int main(void)
{
    test_wrap_helpers();
    test_tick_advancement();
    test_invalid_entry_does_not_mutate();
    test_unrelated_state_is_unchanged();
    return test_failures;
}
