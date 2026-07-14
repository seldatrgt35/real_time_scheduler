#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u

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

static void environment_initialize(void)
{
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    CHECK(rts_init() == RTS_STATUS_OK);
}

static rts_task_config_t make_config(void *stack,
                                     size_t stack_size,
                                     rts_priority_t priority)
{
    rts_task_config_t config = {
        .entry = task_entry,
        .argument = NULL,
        .stack_buffer = stack,
        .stack_size_bytes = stack_size,
        .priority = priority
    };
    return config;
}

static void test_idle_fallback_is_read_only(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *selected;

    environment_initialize();
    selected = rts_scheduler_select_highest_ready();
    CHECK(selected == kernel->idle_task);
    CHECK(rts_scheduler_task_is_idle(selected));
    CHECK(!rts_scheduler_task_is_idle(NULL));
    CHECK(selected->state == RTS_TASK_STATE_READY);
    CHECK(rts_ready_contains(&kernel->ready_set, selected));
    CHECK(rts_scheduler_current_get() == NULL);
    CHECK(rts_scheduler_current_is_valid());
}

static void test_highest_priority_and_fifo_selection(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first_high_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second_high_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t low_config =
        make_config(low_stack, sizeof low_stack, 2u);
    rts_task_config_t first_high_config =
        make_config(first_high_stack, sizeof first_high_stack, 6u);
    rts_task_config_t second_high_config =
        make_config(second_high_stack, sizeof second_high_stack, 6u);
    rts_task_handle_t low = NULL;
    rts_task_handle_t first_high = NULL;
    rts_task_handle_t second_high = NULL;

    environment_initialize();
    CHECK(rts_task_create(&low_config, &low) == RTS_STATUS_OK);
    CHECK(rts_task_create(&first_high_config, &first_high) == RTS_STATUS_OK);
    CHECK(rts_task_create(&second_high_config, &second_high) == RTS_STATUS_OK);
    CHECK(rts_scheduler_select_highest_ready() == first_high);
    CHECK(first_high->ready_node.next == &second_high->ready_node);
    CHECK(low->state == RTS_TASK_STATE_READY);
    CHECK(first_high->state == RTS_TASK_STATE_READY);
    CHECK(second_high->state == RTS_TASK_STATE_READY);
    CHECK(kernel->current_task == NULL);
}

static void test_establish_application_current(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char high_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t low_config =
        make_config(low_stack, sizeof low_stack, 2u);
    rts_task_config_t high_config =
        make_config(high_stack, sizeof high_stack, 7u);
    rts_task_handle_t low = NULL;
    rts_task_handle_t high = NULL;

    environment_initialize();
    CHECK(rts_task_create(&low_config, &low) == RTS_STATUS_OK);
    CHECK(rts_task_create(&high_config, &high) == RTS_STATUS_OK);
    CHECK(rts_scheduler_current_establish(high));
    CHECK(rts_scheduler_current_get() == high);
    CHECK(high->state == RTS_TASK_STATE_RUNNING);
    CHECK(low->state == RTS_TASK_STATE_READY);
    CHECK(rts_ready_contains(&kernel->ready_set, high));
    CHECK(rts_scheduler_select_highest_ready() == high);
    CHECK(rts_scheduler_current_is_valid());
    CHECK(kernel->ready_set.priority_queue[7].count == 1u);
}

static void test_establish_idle_current(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    environment_initialize();
    CHECK(rts_scheduler_current_establish(kernel->idle_task));
    CHECK(rts_scheduler_current_get() == kernel->idle_task);
    CHECK(kernel->idle_task->state == RTS_TASK_STATE_RUNNING);
    CHECK(rts_ready_contains(&kernel->ready_set, kernel->idle_task));
    CHECK(rts_scheduler_select_highest_ready() == kernel->idle_task);
    CHECK(rts_scheduler_current_is_valid());
}

static void test_release_initial_current(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *idle;

    environment_initialize();
    idle = kernel->idle_task;
    CHECK(rts_scheduler_current_establish(idle));
    CHECK(rts_scheduler_current_release_initial());
    CHECK(rts_scheduler_current_get() == NULL);
    CHECK(idle->state == RTS_TASK_STATE_READY);
    CHECK(rts_ready_contains(&kernel->ready_set, idle));
    CHECK(rts_scheduler_select_highest_ready() == idle);
    CHECK(rts_scheduler_current_is_valid());
}

#if RTS_ENABLE_ASSERTIONS
static void test_establish_rejects_nonselected_and_duplicate(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char high_stack[TEST_STACK_BYTES];
    rts_task_config_t low_config =
        make_config(low_stack, sizeof low_stack, 2u);
    rts_task_config_t high_config =
        make_config(high_stack, sizeof high_stack, 7u);
    rts_task_handle_t low = NULL;
    rts_task_handle_t high = NULL;
    unsigned int before;

    environment_initialize();
    CHECK(rts_task_create(&low_config, &low) == RTS_STATUS_OK);
    CHECK(rts_task_create(&high_config, &high) == RTS_STATUS_OK);
    before = assertion_count;
    CHECK(!rts_scheduler_current_establish(low));
    CHECK(assertion_count == before + 1u);
    CHECK(rts_scheduler_current_get() == NULL);
    CHECK(low->state == RTS_TASK_STATE_READY);

    CHECK(rts_scheduler_current_establish(high));
    before = assertion_count;
    CHECK(!rts_scheduler_current_establish(high));
    CHECK(assertion_count == before + 1u);
}

static void test_selection_rejects_reset(void)
{
    unsigned int before;

    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    before = assertion_count;
    CHECK(rts_scheduler_select_highest_ready() == NULL);
    CHECK(assertion_count == before + 1u);
}
#endif

int rts_test_scheduler_select_run(void)
{
    test_idle_fallback_is_read_only();
    test_highest_priority_and_fifo_selection();
    test_establish_application_current();
    test_establish_idle_current();
    test_release_initial_current();
#if RTS_ENABLE_ASSERTIONS
    test_establish_rejects_nonselected_and_duplicate();
    test_selection_rejects_reset();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_scheduler_select_run();
}
