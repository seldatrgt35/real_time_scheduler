#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port_internal.h"
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

static void kernel_prepare(rts_kernel_lifecycle_t lifecycle)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    *kernel = (rts_kernel_state_t){0};
    rts_task_pool_initialize(&kernel->application_task_pool);
    rts_ready_initialize(&kernel->ready_set);
    kernel->lifecycle = lifecycle;
    rts_host_port_test_reset();
}

static rts_task_config_t make_config(void *stack,
                                     size_t stack_size,
                                     rts_priority_t priority,
                                     void *argument)
{
    rts_task_config_t config = {
        .entry = task_entry,
        .argument = argument,
        .stack_buffer = stack,
        .stack_size_bytes = stack_size,
        .priority = priority
    };
    return config;
}

static void test_successful_creation(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    static uint32_t argument;
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config = make_config(stack, sizeof stack, 4u, &argument);
    rts_task_handle_t handle = (rts_task_handle_t)(uintptr_t)1u;
    rts_host_initial_frame_t frame;
    rts_tcb_t outsider = {0};

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
    CHECK(handle != NULL);
    CHECK(handle == &kernel->application_task_pool.slots[0]);
    CHECK(handle->slot_state == RTS_TASK_SLOT_ALLOCATED);
    CHECK(handle->state == RTS_TASK_STATE_READY);
    CHECK(handle->saved_stack_pointer != NULL);
    CHECK(handle->stack_low == stack);
    CHECK(handle->stack_high == stack + sizeof stack);
    CHECK(handle->entry == task_entry);
    CHECK(handle->argument == &argument);
    CHECK(handle->priority == 4u);
    CHECK(handle->wait.reason == RTS_WAIT_NONE);
    CHECK(rts_ready_contains(&kernel->ready_set, handle));
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == handle);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 1u);
    CHECK(rts_task_handle_is_application_task(handle));
    CHECK(rts_task_object_is_valid(handle));
    CHECK(!rts_task_handle_is_application_task(NULL));
    CHECK(!rts_task_handle_is_application_task(&kernel->idle_task_storage));
    CHECK(!rts_task_handle_is_application_task(&outsider));
    CHECK(rts_host_port_initial_frame_read(handle->saved_stack_pointer, &frame));
    CHECK(frame.entry == task_entry);
    CHECK(frame.argument == &argument);
#if RTS_ENABLE_ASSERTIONS
    CHECK(handle->validation_magic == RTS_TASK_VALIDATION_MAGIC);
#endif
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

static void test_mixed_priority_registration(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char high_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char middle_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t low_config =
        make_config(low_stack, sizeof low_stack, 1u, NULL);
    rts_task_config_t high_config =
        make_config(high_stack, sizeof high_stack, 7u, NULL);
    rts_task_config_t middle_config =
        make_config(middle_stack, sizeof middle_stack, 4u, NULL);
    rts_task_handle_t low = NULL;
    rts_task_handle_t high = NULL;
    rts_task_handle_t middle = NULL;

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    CHECK(rts_task_create(&low_config, &low) == RTS_STATUS_OK);
    CHECK(rts_task_create(&high_config, &high) == RTS_STATUS_OK);
    CHECK(rts_task_create(&middle_config, &middle) == RTS_STATUS_OK);
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == high);
    CHECK(rts_ready_contains(&kernel->ready_set, low));
    CHECK(rts_ready_contains(&kernel->ready_set, middle));
}

static void test_fifo_registration(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t first_config =
        make_config(first_stack, sizeof first_stack, 2u, NULL);
    rts_task_config_t second_config =
        make_config(second_stack, sizeof second_stack, 2u, NULL);
    rts_task_handle_t first = NULL;
    rts_task_handle_t second = NULL;

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    CHECK(rts_task_create(&first_config, &first) == RTS_STATUS_OK);
    CHECK(rts_task_create(&second_config, &second) == RTS_STATUS_OK);
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == first);
    CHECK(first->ready_node.next == &second->ready_node);
    CHECK(second->ready_node.previous == &first->ready_node);
    CHECK(kernel->ready_set.priority_queue[2].count == 2u);
}

static void test_capacity_exhaustion(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stacks[RTS_MAX_TASKS][TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config;
    rts_task_handle_t handles[RTS_MAX_TASKS];
    rts_task_handle_t exhausted = (rts_task_handle_t)(uintptr_t)1u;
    size_t index;

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        config = make_config(stacks[index], sizeof stacks[index], 1u, NULL);
        CHECK(rts_task_create(&config, &handles[index]) == RTS_STATUS_OK);
    }
    config = make_config(stacks[0], sizeof stacks[0], 1u, NULL);
    CHECK(rts_task_create(&config, &exhausted) ==
          RTS_STATUS_CAPACITY_EXHAUSTED);
    CHECK(exhausted == NULL);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) ==
          (size_t)RTS_MAX_TASKS);
    CHECK(kernel->ready_set.priority_queue[1].count ==
          (size_t)RTS_MAX_TASKS);
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

static void test_validation_and_context_failures(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config = make_config(stack, sizeof stack, 1u, NULL);
    rts_task_handle_t handle = (rts_task_handle_t)(uintptr_t)1u;
    unsigned int before;

    kernel_prepare(RTS_KERNEL_RESET);
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_INVALID_STATE);
    CHECK(handle == NULL);

    kernel_prepare(RTS_KERNEL_RUNNING);
    handle = (rts_task_handle_t)(uintptr_t)1u;
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_INVALID_STATE);
    CHECK(handle == NULL);

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    config.priority = RTS_IDLE_PRIORITY;
    handle = (rts_task_handle_t)(uintptr_t)1u;
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_INVALID_PRIORITY);
    CHECK(handle == NULL);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 0u);

    config.priority = 1u;
    rts_host_port_test_set_isr(true);
    before = assertion_count;
    handle = (rts_task_handle_t)(uintptr_t)1u;
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_INVALID_CONTEXT);
    CHECK(handle == NULL);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    (void)before;
#endif

    CHECK(rts_task_create(&config, NULL) == RTS_STATUS_INVALID_ARGUMENT);
}

static void test_stack_failure_rolls_back(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config = make_config(stack, sizeof stack, 3u, NULL);
    rts_task_handle_t handle = (rts_task_handle_t)(uintptr_t)1u;
    const rts_tcb_t *slot;

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    rts_host_port_test_fail_next_stack_initialize(true);
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_PORT_ERROR);
    CHECK(handle == NULL);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 0u);
    CHECK(rts_task_pool_next_free_hint(&kernel->application_task_pool) == 0u);
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == NULL);
    slot = rts_task_pool_get_const(&kernel->application_task_pool, 0u);
    CHECK(slot->slot_state == RTS_TASK_SLOT_FREE);
    CHECK(slot->saved_stack_pointer == NULL);
    CHECK(slot->stack_low == NULL && slot->stack_high == NULL);
    CHECK(slot->entry == NULL && slot->argument == NULL);
    CHECK(slot->state == RTS_TASK_STATE_DORMANT);
    CHECK(slot->wait.reason == RTS_WAIT_NONE && slot->wait.wake_tick == 0u);
    CHECK(slot->slice_remaining == 0u);
    CHECK(slot->priority == RTS_IDLE_PRIORITY);
    CHECK(slot->ready_node.previous == NULL && slot->ready_node.next == NULL &&
          slot->ready_node.owner == NULL && slot->ready_node.object == NULL);
    CHECK(slot->delay_node.previous == NULL && slot->delay_node.next == NULL &&
          slot->delay_node.owner == NULL && slot->delay_node.object == NULL);
#if RTS_ENABLE_ASSERTIONS
    CHECK(slot->validation_magic == 0u);
#endif
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

static void test_repeated_failure_does_not_leak(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config = make_config(stack, sizeof stack, 3u, NULL);
    rts_task_handle_t handle;
    size_t attempt;

    kernel_prepare(RTS_KERNEL_INITIALIZED);
    for (attempt = 0u; attempt < (size_t)RTS_MAX_TASKS + 2u; ++attempt)
    {
        handle = (rts_task_handle_t)(uintptr_t)1u;
        rts_host_port_test_fail_next_stack_initialize(true);
        CHECK(rts_task_create(&config, &handle) == RTS_STATUS_PORT_ERROR);
        CHECK(handle == NULL);
        CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) ==
              0u);
        CHECK(rts_task_pool_next_free_hint(&kernel->application_task_pool) ==
              0u);
        CHECK(kernel->application_task_pool.slots[0].slot_state ==
              RTS_TASK_SLOT_FREE);
        CHECK(rts_ready_peek_highest(&kernel->ready_set) == NULL);
    }
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

int rts_test_task_create_run(void)
{
    test_successful_creation();
    test_fifo_registration();
    test_mixed_priority_registration();
    test_capacity_exhaustion();
    test_validation_and_context_failures();
    test_stack_failure_rolls_back();
    test_repeated_failure_does_not_leak();
    return test_failures;
}

int main(void)
{
    return rts_test_task_create_run();
}
