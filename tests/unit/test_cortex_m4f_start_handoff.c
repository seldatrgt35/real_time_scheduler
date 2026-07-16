#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"

static int test_failures;
static rts_kernel_state_t test_kernel;
static bool test_current_valid;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++test_failures;
}

rts_kernel_state_t *rts_kernel_state_get(void)
{
    return &test_kernel;
}

bool rts_scheduler_current_is_valid(void)
{
    return test_current_valid;
}

bool rts_port_tick_commit_start(void)
{
    return true;
}

static void test_invalid_preconditions(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT) unsigned char stack[64];
    static rts_tcb_t task;

    test_current_valid = true;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);

    test_kernel.lifecycle = RTS_KERNEL_RUNNING;
    test_kernel.current_task = &task;
    task.state = RTS_TASK_STATE_READY;
    task.saved_stack_pointer = stack;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);

    task.state = RTS_TASK_STATE_RUNNING;
    task.saved_stack_pointer = NULL;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);

    task.saved_stack_pointer = stack + 1u;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);

    task.saved_stack_pointer = stack;
    test_current_valid = false;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);
    test_current_valid = true;

    test_kernel.switch_plan.pending = true;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);
    test_kernel.switch_plan.pending = false;
    test_kernel.switch_plan.active = true;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);
    test_kernel.switch_plan.active = false;
    test_kernel.application_task_pool.allocated_count = 3u;
    test_kernel.ready_set.ready_bitmap[0] = UINT32_C(0x21);
    test_kernel.delay_queue.ordered_tasks.count = 2u;
    task.priority = 5u;
}

static void test_valid_single_start_and_consume(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT) unsigned char stack[64];
    rts_tcb_t *task = test_kernel.current_task;
    const rts_cm4f_start_handoff_t *handoff;
    rts_tcb_t *current_before = test_kernel.current_task;
    rts_kernel_lifecycle_t lifecycle_before = test_kernel.lifecycle;
    bool pending_before = test_kernel.switch_plan.pending;
    bool active_before = test_kernel.switch_plan.active;
    size_t allocated_before =
        test_kernel.application_task_pool.allocated_count;
    uint32_t bitmap_before = test_kernel.ready_set.ready_bitmap[0];
    size_t delayed_before = test_kernel.delay_queue.ordered_tasks.count;
    rts_priority_t priority_before = task->priority;

    task->saved_stack_pointer = stack;
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_OK);
    handoff = rts_cm4f_start_handoff_get();
    CHECK(handoff->first_task == task);
    CHECK(handoff->saved_stack_pointer == stack);
    CHECK(handoff->cookie == RTS_CM4F_START_HANDOFF_COOKIE);
    CHECK(handoff->valid == UINT32_C(1));
    CHECK(test_kernel.current_task == current_before);
    CHECK(test_kernel.lifecycle == lifecycle_before);
    CHECK(test_kernel.switch_plan.pending == pending_before);
    CHECK(test_kernel.switch_plan.active == active_before);
    CHECK(test_kernel.application_task_pool.allocated_count == allocated_before);
    CHECK(test_kernel.ready_set.ready_bitmap[0] == bitmap_before);
    CHECK(test_kernel.delay_queue.ordered_tasks.count == delayed_before);
    CHECK(task->priority == priority_before);
    CHECK(task->state == RTS_TASK_STATE_RUNNING);

    CHECK(rts_cm4f_start_handoff_consume() == stack);
    CHECK(handoff->valid == UINT32_C(0));
    CHECK(rts_cm4f_start_handoff_prepare() == RTS_STATUS_INVALID_STATE);
}

int main(void)
{
    test_invalid_preconditions();
    test_valid_single_start_and_consume();
    return test_failures;
}
