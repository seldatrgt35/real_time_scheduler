#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "port.h"
#include "port_internal.h"

#define TEST_STACK_BYTES 256u
#define TEST_FILL_BYTE   UINT8_C(0xa5)

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

static void test_entry(void *argument)
{
    (void)argument;
    ++entry_call_count;
}

static void bytes_fill(unsigned char *buffer, size_t size, unsigned char value)
{
    size_t index;

    for (index = 0u; index < size; ++index)
    {
        buffer[index] = value;
    }
}

static bool bytes_equal(const unsigned char *left,
                        const unsigned char *right,
                        size_t size)
{
    size_t index;

    for (index = 0u; index < size; ++index)
    {
        if (left[index] != right[index])
        {
            return false;
        }
    }
    return true;
}

static void test_contract_queries(void)
{
    const size_t minimum = rts_port_task_stack_minimum_size_bytes();
    const size_t granularity =
        rts_port_task_stack_size_granularity_bytes();

    CHECK(minimum >= sizeof(rts_host_initial_frame_t));
    CHECK((minimum % RTS_TASK_STACK_ALIGNMENT) == 0u);
    CHECK(granularity == RTS_TASK_STACK_ALIGNMENT);
}

static void test_initial_frame(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    static uint32_t argument;
    const size_t minimum = rts_port_task_stack_minimum_size_bytes();
    rts_port_stack_result_t result;
    rts_host_initial_frame_t frame;
    size_t index;

    bytes_fill(stack, sizeof stack, TEST_FILL_BYTE);
    result = rts_port_stack_initialize(stack, sizeof stack, test_entry,
                                       &argument);

    CHECK(result.status == RTS_STATUS_OK);
    CHECK(result.saved_stack_pointer == stack + sizeof stack - minimum);
    CHECK(((uintptr_t)result.saved_stack_pointer %
           (uintptr_t)RTS_TASK_STACK_ALIGNMENT) == 0u);
    CHECK(rts_host_port_initial_frame_read(result.saved_stack_pointer, &frame));
    CHECK(frame.magic == RTS_HOST_INITIAL_FRAME_MAGIC);
    CHECK(frame.version == RTS_HOST_INITIAL_FRAME_VERSION);
    CHECK(frame.entry == test_entry);
    CHECK(frame.argument == &argument);
    CHECK(frame.return_trap == rts_host_port_task_return_trap);
    CHECK(frame.reserved == 0u);
    CHECK(entry_call_count == 0u);
    CHECK(assertion_count == 0u);

    for (index = 0u; index < sizeof stack - minimum; ++index)
    {
        CHECK(stack[index] == TEST_FILL_BYTE);
    }
    for (index = sizeof(frame); index < minimum; ++index)
    {
        CHECK(((unsigned char *)result.saved_stack_pointer)[index] == 0u);
    }
}

static void test_deterministic_frame_and_null_argument(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second[TEST_STACK_BYTES];
    const size_t minimum = rts_port_task_stack_minimum_size_bytes();
    rts_port_stack_result_t first_result;
    rts_port_stack_result_t second_result;
    rts_host_initial_frame_t frame;

    bytes_fill(first, sizeof first, 0u);
    bytes_fill(second, sizeof second, UINT8_MAX);
    first_result = rts_port_stack_initialize(first, sizeof first, test_entry,
                                             NULL);
    second_result = rts_port_stack_initialize(second, sizeof second, test_entry,
                                              NULL);

    CHECK(first_result.status == RTS_STATUS_OK);
    CHECK(second_result.status == RTS_STATUS_OK);
    CHECK(bytes_equal(first_result.saved_stack_pointer,
                      second_result.saved_stack_pointer, minimum));
    CHECK(rts_host_port_initial_frame_read(first_result.saved_stack_pointer,
                                           &frame));
    CHECK(frame.argument == NULL);
}

static void test_rejected_inputs_do_not_write(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    unsigned char snapshot[TEST_STACK_BYTES];
    const size_t minimum = rts_port_task_stack_minimum_size_bytes();
    rts_port_stack_result_t result;

    bytes_fill(stack, sizeof stack, TEST_FILL_BYTE);
    bytes_fill(snapshot, sizeof snapshot, TEST_FILL_BYTE);

    result = rts_port_stack_initialize(NULL, sizeof stack, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    CHECK(result.saved_stack_pointer == NULL);

    result = rts_port_stack_initialize(stack, sizeof stack, NULL, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_TASK_CONFIG);
    CHECK(result.saved_stack_pointer == NULL);

    result = rts_port_stack_initialize(stack, 0u, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize(stack, minimum - 1u, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize(stack, minimum + 1u, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize(stack + 1u, minimum, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize((void *)(UINTPTR_MAX - (uintptr_t)15u),
                                       minimum, test_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    CHECK(bytes_equal(stack, snapshot, sizeof stack));
}

static void test_frame_reader_guards(void)
{
    rts_host_initial_frame_t frame;

    CHECK(!rts_host_port_initial_frame_read(NULL, &frame));
    CHECK(!rts_host_port_initial_frame_read(&frame, NULL));
}

static void test_context_and_critical_contract(void)
{
    rts_critical_token_t outer;
    rts_critical_token_t inner;

    rts_host_port_test_reset();
    CHECK(!rts_port_is_in_isr());
    rts_host_port_test_set_isr(true);
    CHECK(rts_port_is_in_isr());
    rts_host_port_test_set_isr(false);

    outer = rts_port_critical_enter();
    CHECK(rts_host_port_test_critical_depth() == 1u);
    inner = rts_port_critical_enter();
    CHECK(rts_host_port_test_critical_depth() == 2u);
    rts_port_critical_exit(inner);
    CHECK(rts_host_port_test_critical_depth() == 1u);
    rts_port_critical_exit(outer);
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

#if RTS_ENABLE_ASSERTIONS
static void test_critical_lifo_assertion(void)
{
    rts_critical_token_t outer;
    rts_critical_token_t inner;
    unsigned int before;

    rts_host_port_test_reset();
    outer = rts_port_critical_enter();
    inner = rts_port_critical_enter();
    before = assertion_count;
    rts_port_critical_exit(outer);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_host_port_test_critical_depth() == 2u);
    rts_port_critical_exit(inner);
    rts_port_critical_exit(outer);
    CHECK(rts_host_port_test_critical_depth() == 0u);
}
#endif

int rts_test_host_stack_port_run(void)
{
    test_contract_queries();
    test_initial_frame();
    test_deterministic_frame_and_null_argument();
    test_rejected_inputs_do_not_write();
    test_frame_reader_guards();
    test_context_and_critical_contract();
#if RTS_ENABLE_ASSERTIONS
    test_critical_lifo_assertion();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_host_stack_port_run();
}
