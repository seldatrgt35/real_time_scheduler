#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u

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

static rts_task_handle_t create_task(void *stack, rts_priority_t priority)
{
    rts_task_config_t config = {
        task_entry, NULL, stack, TEST_STACK_BYTES, priority
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
}

static void test_public_rejections(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    reset_environment();
    CHECK(rts_start() == RTS_STATUS_INVALID_STATE);
    CHECK(kernel->lifecycle == RTS_KERNEL_RESET);
    CHECK(kernel->current_task == NULL);
    CHECK(rts_host_port_test_start_request_count() == 0u);

    CHECK(rts_init() == RTS_STATUS_OK);
    rts_host_port_test_set_isr(true);
    CHECK(rts_start() == RTS_STATUS_INVALID_CONTEXT);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(kernel->current_task == NULL);
    CHECK(rts_host_port_test_start_request_count() == 0u);
    rts_host_port_test_set_isr(false);
}

static void test_idle_only_start(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *idle;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    idle = kernel->idle_task;
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->lifecycle == RTS_KERNEL_RUNNING);
    CHECK(kernel->current_task == idle);
    CHECK(idle->state == RTS_TASK_STATE_RUNNING);
    CHECK(rts_ready_contains(&kernel->ready_set, idle));
    CHECK(kernel->ready_set.priority_queue[0].count == 1u);
    CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    CHECK(rts_host_port_test_start_request_count() == 1u);
    CHECK(rts_host_port_test_start_task() == idle);
    CHECK(rts_host_port_test_start_saved_stack_pointer() ==
          idle->saved_stack_pointer);
    CHECK(rts_host_port_test_critical_depth() == 0u);
    CHECK(rts_start() == RTS_STATUS_ALREADY_STARTED);
    CHECK(rts_host_port_test_start_request_count() == 1u);
}

static void test_application_selection_and_fifo(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t low;
    rts_task_handle_t first;
    rts_task_handle_t second;
    rts_list_node_t *head;
    rts_list_node_t *first_next;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    low = create_task(low_stack, 2u);
    first = create_task(first_stack, 5u);
    second = create_task(second_stack, 5u);
    head = kernel->ready_set.priority_queue[5].head;
    first_next = first->ready_node.next;

    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == first);
    CHECK(first->state == RTS_TASK_STATE_RUNNING);
    CHECK(second->state == RTS_TASK_STATE_READY);
    CHECK(low->state == RTS_TASK_STATE_READY);
    CHECK(kernel->ready_set.priority_queue[5].head == head);
    CHECK(first->ready_node.next == first_next);
    CHECK(first_next == &second->ready_node);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(rts_host_port_test_start_task() == first);
    CHECK(rts_host_port_test_start_saved_stack_pointer() ==
          first->saved_stack_pointer);
}

static void test_port_failure_rollback_and_retry(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t task;
    rts_list_node_t *head;
    size_t allocated;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    task = create_task(stack, 6u);
    head = kernel->ready_set.priority_queue[6].head;
    allocated = kernel->application_task_pool.allocated_count;
    rts_host_port_test_fail_next_start(true);

    CHECK(rts_start() == RTS_STATUS_PORT_ERROR);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(kernel->current_task == NULL);
    CHECK(task->state == RTS_TASK_STATE_READY);
    CHECK(kernel->ready_set.priority_queue[6].head == head);
    CHECK(rts_ready_contains(&kernel->ready_set, task));
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(kernel->application_task_pool.allocated_count == allocated);
    CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    CHECK(rts_host_port_test_start_request_count() == 0u);
    CHECK(rts_host_port_test_critical_depth() == 0u);

    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == task);
    CHECK(task->state == RTS_TASK_STATE_RUNNING);
    CHECK(rts_host_port_test_start_request_count() == 1u);
}

static void test_start_then_yield_integration(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t first;
    rts_task_handle_t second;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    first = create_task(first_stack, 5u);
    second = create_task(second_stack, 5u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(kernel->current_task == first);
    CHECK(kernel->ready_set.priority_queue[5].head == &second->ready_node);
    CHECK(kernel->switch_plan.pending);
    CHECK(kernel->switch_plan.from == first);
    CHECK(kernel->switch_plan.to == second);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

#if RTS_ENABLE_ASSERTIONS
static void test_preflight_corruption_asserts(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    unsigned int before;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    kernel->current_task = kernel->idle_task;
    before = assertion_count;
    CHECK(rts_start() == RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(rts_host_port_test_critical_depth() == 0u);

    kernel->current_task = NULL;
    kernel->switch_plan.active = true;
    before = assertion_count;
    CHECK(rts_start() == RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(rts_host_port_test_start_request_count() == 0u);
}
#endif

int main(void)
{
    test_public_rejections();
    test_idle_only_start();
    test_application_selection_and_fifo();
    test_port_failure_rollback_and_retry();
    test_start_then_yield_integration();
#if RTS_ENABLE_ASSERTIONS
    test_preflight_corruption_asserts();
#endif
    return test_failures;
}
