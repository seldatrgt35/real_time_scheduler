#include <stddef.h>
#include <stdint.h>

#include "port.h"
#include "task_internal.h"

#define TEST_STACK_MINIMUM_BYTES 128u
#define TEST_STACK_GRANULARITY   16u

static int test_failures;
static unsigned int assertion_count;
static size_t port_minimum = TEST_STACK_MINIMUM_BYTES;
static size_t port_granularity = TEST_STACK_GRANULARITY;

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

size_t rts_port_task_stack_minimum_size_bytes(void)
{
    return port_minimum;
}

size_t rts_port_task_stack_size_granularity_bytes(void)
{
    return port_granularity;
}

static void test_entry(void *argument)
{
    (void)argument;
}

static rts_task_config_t valid_config(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_MINIMUM_BYTES];
    rts_task_config_t config = {
        .entry = test_entry,
        .argument = NULL,
        .stack_buffer = stack,
        .stack_size_bytes = sizeof stack,
        .priority = 1u
    };

    return config;
}

static void test_valid_descriptor(void)
{
    rts_task_config_t config = valid_config();
    int argument = 42;

    config.argument = &argument;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_OK);
    CHECK(argument == 42);
}

static void test_pointer_entry_and_lifecycle(void)
{
    rts_task_config_t config = valid_config();

    CHECK(rts_task_config_validate(NULL, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_RESET) ==
          RTS_STATUS_INVALID_STATE);
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_RUNNING) ==
          RTS_STATUS_INVALID_STATE);

    config.entry = NULL;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_TASK_CONFIG);
}

static void test_priority(void)
{
    rts_task_config_t config = valid_config();

    config.priority = RTS_IDLE_PRIORITY;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_PRIORITY);
    config.priority = (rts_priority_t)RTS_PRIORITY_COUNT;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_PRIORITY);
    config.priority = (rts_priority_t)(RTS_PRIORITY_COUNT - 1u);
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_OK);
}

static void test_stack_rules(void)
{
    rts_task_config_t config = valid_config();
    uintptr_t aligned_address = (uintptr_t)config.stack_buffer;

    config.stack_buffer = NULL;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);

    config = valid_config();
    config.stack_buffer = (void *)(aligned_address + 1u);
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);

    config = valid_config();
    config.stack_size_bytes = 0u;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);
    config.stack_size_bytes = TEST_STACK_MINIMUM_BYTES - TEST_STACK_GRANULARITY;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);
    config.stack_size_bytes = TEST_STACK_MINIMUM_BYTES + 1u;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);

    config = valid_config();
    config.stack_buffer = (void *)(UINTPTR_MAX - (uintptr_t)15u);
    config.stack_size_bytes = TEST_STACK_MINIMUM_BYTES;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);
}

#if RTS_ENABLE_ASSERTIONS
static void test_invalid_port_contract(void)
{
    rts_task_config_t config = valid_config();
    unsigned int before;

    port_minimum = 0u;
    before = assertion_count;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);
    CHECK(assertion_count == before + 1u);
    port_minimum = TEST_STACK_MINIMUM_BYTES;

    port_granularity = 0u;
    before = assertion_count;
    CHECK(rts_task_config_validate(&config, RTS_KERNEL_INITIALIZED) ==
          RTS_STATUS_INVALID_STACK);
    CHECK(assertion_count == before + 1u);
    port_granularity = TEST_STACK_GRANULARITY;
}
#endif

int rts_test_task_validation_run(void)
{
    test_valid_descriptor();
    test_pointer_entry_and_lifecycle();
    test_priority();
    test_stack_rules();
#if RTS_ENABLE_ASSERTIONS
    test_invalid_port_contract();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_task_validation_run();
}
