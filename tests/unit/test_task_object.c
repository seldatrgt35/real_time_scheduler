#include <stddef.h>
#include <stdint.h>

#include "task_internal.h"

static int test_failures;
static unsigned int assertion_count;
static unsigned int entry_call_count;

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
    ++entry_call_count;
}

static rts_task_config_t make_config(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT) unsigned char stack[256u];
    static uint32_t argument;
    rts_task_config_t config = {
        .entry = task_entry,
        .argument = &argument,
        .stack_buffer = stack,
        .stack_size_bytes = sizeof stack,
        .priority = 3u
    };

    return config;
}

static rts_tcb_t *reserve_clean_slot(rts_task_pool_t *pool, size_t *slot_index)
{
    rts_tcb_t *task;

    rts_task_pool_initialize(pool);
    CHECK(rts_task_pool_reserve(pool, slot_index));
    task = rts_task_pool_get(pool, *slot_index);
    rts_list_node_initialize(&task->ready_node);
    rts_list_node_initialize(&task->delay_node);
    return task;
}

static void test_complete_initialization(void)
{
    rts_task_pool_t pool = {0};
    rts_task_config_t config = make_config();
    size_t slot_index;
    rts_tcb_t *task = reserve_clean_slot(&pool, &slot_index);
    const size_t count_before = pool.allocated_count;
    const size_t hint_before = pool.next_free_hint;

    task->saved_stack_pointer = (void *)(uintptr_t)1u;
    task->wait.reason = RTS_WAIT_DELAY;
    task->wait.wake_tick = UINT32_C(99);
    task->slice_remaining = 0u;
    task->state = RTS_TASK_STATE_BLOCKED;
#if RTS_ENABLE_ASSERTIONS
    task->validation_magic = UINT32_MAX;
#endif

    CHECK(rts_task_object_initialize(&pool, task, &config,
                                     RTS_KERNEL_INITIALIZED) == RTS_STATUS_OK);
    CHECK(task->saved_stack_pointer == NULL);
    CHECK(task->stack_low == (unsigned char *)config.stack_buffer);
    CHECK(task->stack_high ==
          (unsigned char *)config.stack_buffer + config.stack_size_bytes);
    CHECK(task->entry == config.entry);
    CHECK(task->argument == config.argument);
    CHECK(task->priority == config.priority);
    CHECK(task->state == RTS_TASK_STATE_DORMANT);
    CHECK(task->wait.reason == RTS_WAIT_NONE);
    CHECK(task->wait.wake_tick == 0u);
    CHECK(task->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(task->slot_state == RTS_TASK_SLOT_RESERVED);
    CHECK(task->ready_node.previous == NULL && task->ready_node.next == NULL &&
          task->ready_node.owner == NULL && task->ready_node.object == NULL);
    CHECK(task->delay_node.previous == NULL && task->delay_node.next == NULL &&
          task->delay_node.owner == NULL && task->delay_node.object == NULL);
    CHECK(pool.allocated_count == count_before);
    CHECK(pool.next_free_hint == hint_before);
    CHECK(entry_call_count == 0u);
#if RTS_ENABLE_ASSERTIONS
    CHECK(task->validation_magic == 0u);
#endif
    CHECK(offsetof(struct rts_task, saved_stack_pointer) == 0u);
}

#if RTS_ENABLE_ASSERTIONS
static void test_precondition_assertions(void)
{
    rts_task_pool_t pool = {0};
    rts_task_pool_t other_pool = {0};
    rts_task_config_t config = make_config();
    size_t slot_index;
    rts_tcb_t *task = reserve_clean_slot(&pool, &slot_index);
    rts_tcb_t outsider = {0};
    unsigned int before;

    before = assertion_count;
    CHECK(rts_task_object_initialize(NULL, task, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, NULL, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, task, NULL,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, &outsider, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_TASK_CONFIG);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, task, &config, RTS_KERNEL_RESET) ==
          RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);

    task->slot_state = RTS_TASK_SLOT_ALLOCATED;
    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, task, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    task->slot_state = RTS_TASK_SLOT_RESERVED;

    task->ready_node.previous = &task->ready_node;
    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, task, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STATE);
    CHECK(assertion_count == before + 1u);
    task->ready_node.previous = NULL;

    config.entry = NULL;
    before = assertion_count;
    CHECK(rts_task_object_initialize(&pool, task, &config,
                                     RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_TASK_CONFIG);
    CHECK(assertion_count == before + 1u);

    (void)other_pool;
}
#endif

int rts_test_task_object_run(void)
{
    test_complete_initialization();
#if RTS_ENABLE_ASSERTIONS
    test_precondition_assertions();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_task_object_run();
}
