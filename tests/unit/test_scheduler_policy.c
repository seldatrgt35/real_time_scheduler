#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "scheduler_policy.h"

static int failures;

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

#define POLICY_TEST_STACK_BYTES 512u
RTS_TASK_STACK_DECLARE(stack_0, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_1, POLICY_TEST_STACK_BYTES);
#if !RTS_POLICY_FIXED_PRIORITY
RTS_TASK_STACK_DECLARE(stack_2, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_3, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_4, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_5, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_6, POLICY_TEST_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_7, POLICY_TEST_STACK_BYTES);
#endif

static void task_entry(void *argument)
{
    (void)argument;
}

static void reset_initialized(void)
{
    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_policy_validate(NULL, false));
}

static rts_tcb_t *create_task(void *stack,
                              rts_priority_t priority,
                              rts_tick_t period,
                              rts_tick_t deadline)
{
    rts_task_handle_t handle = NULL;
    const rts_task_config_t config = {
        .entry = task_entry,
        .argument = NULL,
        .stack_buffer = stack,
        .stack_size_bytes = POLICY_TEST_STACK_BYTES,
        .priority = priority,
        .period = period,
        .relative_deadline = deadline,
        .execution_budget = 0u
    };

    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
    CHECK(handle != NULL);
    CHECK(handle == NULL || rts_policy_validate(handle, true));
    return handle;
}

static void test_selected_policy(void)
{
#if RTS_POLICY_FIXED_PRIORITY
    CHECK(rts_policy_kind() == RTS_POLICY_KIND_FIXED_PRIORITY);
#elif RTS_POLICY_RMS
    CHECK(rts_policy_kind() == RTS_POLICY_KIND_RMS);
#else
    CHECK(rts_policy_kind() == RTS_POLICY_KIND_EDF);
#endif
}

static void test_block_and_wakeup(void)
{
    rts_tcb_t *task;

    reset_initialized();
#if RTS_POLICY_RMS
    task = create_task(stack_0, 1u, 20u, 20u);
#elif RTS_POLICY_EDF
    task = create_task(stack_0, 1u, 0u, 20u);
#else
    task = create_task(stack_0, 1u, 0u, 0u);
#endif
    CHECK(rts_policy_task_block(task));
    CHECK(rts_policy_validate(task, false));
    task->state = RTS_TASK_STATE_BLOCKED;
    task->state = RTS_TASK_STATE_READY;
    CHECK(rts_policy_task_unblock(task));
    CHECK(rts_policy_pick_next() == task);
    CHECK(rts_policy_validate(task, true));
}

static void test_policy_metadata_validation(void)
{
    rts_task_handle_t handle = NULL;
    rts_task_config_t config = {
        .entry = task_entry,
        .argument = NULL,
        .stack_buffer = stack_1,
        .stack_size_bytes = POLICY_TEST_STACK_BYTES,
        .priority = 1u,
        .period = 0u,
        .relative_deadline = 0u,
        .execution_budget = 0u
    };

    reset_initialized();
#if RTS_POLICY_RMS
    config.period = 20u;
    config.relative_deadline = 21u;
    CHECK(rts_task_create(&config, &handle) ==
          RTS_STATUS_INVALID_TASK_CONFIG);
#elif RTS_POLICY_EDF
    CHECK(rts_task_create(&config, &handle) ==
          RTS_STATUS_INVALID_TASK_CONFIG);
#else
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
#endif
}

#if RTS_POLICY_FIXED_PRIORITY
static void test_fixed_priority_regression(void)
{
    rts_tcb_t *low;
    rts_tcb_t *high;

    reset_initialized();
    low = create_task(stack_0, 2u, 0u, 0u);
    high = create_task(stack_1, 7u, 0u, 0u);
    CHECK(rts_policy_pick_next() == high);
    CHECK(low->priority == 2u && high->priority == 7u);
}
#endif

#if RTS_POLICY_RMS
static void test_rms_period_assignment(void)
{
    rts_tcb_t *slow;
    rts_tcb_t *fast;
    rts_tcb_t *medium;
    rts_tcb_t *equal_fast;

    reset_initialized();
    slow = create_task(stack_0, 40u, 100u, 100u);
    fast = create_task(stack_1, 1u, 20u, 20u);
    medium = create_task(stack_2, 2u, 50u, 50u);
    equal_fast = create_task(stack_3, 3u, 20u, 20u);

    CHECK(slow->base_priority == 1u);
    CHECK(medium->base_priority == 2u);
    CHECK(fast->base_priority == 3u);
    CHECK(equal_fast->base_priority == 3u);
    CHECK(fast->priority == fast->base_priority);
    CHECK(rts_policy_pick_next() == fast);
    CHECK(rts_policy_yield(fast));
    CHECK(rts_policy_pick_next() == equal_fast);
}

static void test_rms_large_task_set(void)
{
    void *stacks[8] = {stack_0, stack_1, stack_2, stack_3,
                       stack_4, stack_5, stack_6, stack_7};
    rts_tcb_t *tasks[8];
    size_t index;

    reset_initialized();
    for (index = 0u; index < 8u; ++index)
    {
        tasks[index] = create_task(stacks[index], 1u,
                                   (rts_tick_t)(80u - index * 10u),
                                   (rts_tick_t)(80u - index * 10u));
    }
    CHECK(rts_policy_pick_next() == tasks[7]);
    CHECK(tasks[7]->base_priority == 8u);
    CHECK(rts_policy_validate(NULL, false));
}
#endif

#if RTS_POLICY_EDF
static void test_edf_ordering_and_fifo(void)
{
    rts_tcb_t *late;
    rts_tcb_t *early;
    rts_tcb_t *middle;
    rts_tcb_t *equal_early;

    reset_initialized();
    late = create_task(stack_0, 60u, 100u, 100u);
    early = create_task(stack_1, 1u, 20u, 20u);
    middle = create_task(stack_2, 50u, 50u, 50u);
    equal_early = create_task(stack_3, 2u, 20u, 20u);
    (void)late;
    (void)middle;

    CHECK(rts_policy_pick_next() == early);
    CHECK(early->absolute_deadline == 20u);
    CHECK(equal_early->absolute_deadline == 20u);
    CHECK(early->release_sequence < equal_early->release_sequence);
    CHECK(rts_policy_yield(early));
    CHECK(rts_policy_pick_next() == equal_early);
    CHECK(rts_policy_validate(NULL, false));
}

static void test_edf_release_update_and_wrap(void)
{
    rts_tcb_t *task;
    rts_kernel_state_t *kernel;

    reset_initialized();
    task = create_task(stack_0, 1u, 0u, 10u);
    kernel = rts_kernel_state_get();
    CHECK(rts_policy_task_block(task));
    task->state = RTS_TASK_STATE_BLOCKED;
    kernel->current_tick = UINT32_MAX - 5u;
    task->state = RTS_TASK_STATE_READY;
    CHECK(rts_policy_task_unblock(task));
    CHECK(task->release_tick == UINT32_MAX - 5u);
    CHECK(task->absolute_deadline == 4u);
    CHECK(rts_policy_pick_next() == task);
}

static void test_edf_large_task_set(void)
{
    void *stacks[8] = {stack_0, stack_1, stack_2, stack_3,
                       stack_4, stack_5, stack_6, stack_7};
    rts_tcb_t *tasks[8];
    size_t index;

    reset_initialized();
    for (index = 0u; index < 8u; ++index)
    {
        tasks[index] = create_task(stacks[index],
                                   (rts_priority_t)(index + 1u), 0u,
                                   (rts_tick_t)(80u - index * 10u));
    }
    CHECK(rts_policy_pick_next() == tasks[7]);
    CHECK(rts_policy_validate(NULL, false));
}
#endif

int main(void)
{
    test_selected_policy();
    test_block_and_wakeup();
    test_policy_metadata_validation();
#if RTS_POLICY_FIXED_PRIORITY
    test_fixed_priority_regression();
#elif RTS_POLICY_RMS
    test_rms_period_assignment();
    test_rms_large_task_set();
#else
    test_edf_ordering_and_fifo();
    test_edf_release_update_and_wrap();
    test_edf_large_task_set();
#endif
    return failures;
}
