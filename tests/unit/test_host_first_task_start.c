#include <stdbool.h>
#include <stdint.h>

#include "port_internal.h"
#include "scheduler_internal.h"

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

int main(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT) unsigned char stack[64];
    static rts_tcb_t task;

    rts_host_port_test_start_reset();
    CHECK(rts_port_start_first_task() == RTS_STATUS_INVALID_STATE);
    CHECK(rts_host_port_test_start_request_count() == 0u);

    test_kernel.lifecycle = RTS_KERNEL_RUNNING;
    test_kernel.current_task = &task;
    task.state = RTS_TASK_STATE_RUNNING;
    task.saved_stack_pointer = stack;
    test_current_valid = true;
    CHECK(rts_port_start_first_task() == RTS_STATUS_OK);
    CHECK(rts_host_port_test_start_request_count() == 1u);
    CHECK(rts_host_port_test_start_task() == &task);
    CHECK(rts_host_port_test_start_consumed());
    CHECK(test_kernel.current_task == &task);
    CHECK(task.state == RTS_TASK_STATE_RUNNING);
    CHECK(!test_kernel.switch_plan.pending && !test_kernel.switch_plan.active);

    CHECK(rts_port_start_first_task() == RTS_STATUS_INVALID_STATE);
    CHECK(rts_host_port_test_start_request_count() == 1u);
    return test_failures;
}
