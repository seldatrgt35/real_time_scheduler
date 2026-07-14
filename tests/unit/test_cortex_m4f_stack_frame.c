#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "port.h"
#include "port_internal.h"

#define TEST_GUARD_BYTES 16u
#define TEST_FILL_BYTE   0xA5u

static int test_failures;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++test_failures;
}

static void task_entry(void *argument)
{
    (void)argument;
}

static uint32_t word_read(const unsigned char *frame, size_t word)
{
    const unsigned char *source = frame + (word * sizeof(uint32_t));
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8u) |
           ((uint32_t)source[2] << 16u) |
           ((uint32_t)source[3] << 24u);
}

static uint32_t entry_encode(rts_task_entry_t entry)
{
    uint32_t encoded = 0u;
    unsigned char *out = (unsigned char *)&encoded;
    const unsigned char *in = (const unsigned char *)&entry;
    size_t index;

    for (index = 0u; index < sizeof(encoded); ++index)
    {
        out[index] = in[index];
    }
    return encoded;
}

static uint32_t trap_encode(void)
{
    void (*trap)(void) = rts_cm4f_task_return_trap;
    uint32_t encoded = 0u;
    unsigned char *out = (unsigned char *)&encoded;
    const unsigned char *in = (const unsigned char *)&trap;
    size_t index;

    for (index = 0u; index < sizeof(encoded); ++index)
    {
        out[index] = in[index];
    }
    return encoded;
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
                        const unsigned char *right, size_t size)
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

static void test_exact_frame(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char storage[RTS_CM4F_INITIAL_FRAME_SIZE_BYTES +
                              (2u * TEST_GUARD_BYTES)];
    unsigned char *stack = storage + TEST_GUARD_BYTES;
    void *argument = &storage[0];
    rts_port_stack_result_t result;
    size_t word;

    bytes_fill(storage, sizeof(storage), TEST_FILL_BYTE);
    result = rts_port_stack_initialize(stack,
                                       RTS_CM4F_INITIAL_FRAME_SIZE_BYTES,
                                       task_entry, argument);
    CHECK(result.status == RTS_STATUS_OK);
    CHECK(result.saved_stack_pointer == stack);
    CHECK(((uintptr_t)result.saved_stack_pointer %
           RTS_TASK_STACK_ALIGNMENT) == 0u);
    for (word = 0u; word < 8u; ++word)
    {
        CHECK(word_read(stack, word) == 0u);
    }
    CHECK(word_read(stack, 8u) == (uint32_t)(uintptr_t)argument);
    for (word = 9u; word <= 12u; ++word)
    {
        CHECK(word_read(stack, word) == 0u);
    }
    CHECK(word_read(stack, 13u) == trap_encode());
    CHECK(word_read(stack, 14u) == entry_encode(task_entry));
    CHECK(word_read(stack, 15u) == RTS_CM4F_INITIAL_XPSR);
    for (word = 0u; word < TEST_GUARD_BYTES; ++word)
    {
        CHECK(storage[word] == TEST_FILL_BYTE);
        CHECK(storage[TEST_GUARD_BYTES +
                      RTS_CM4F_INITIAL_FRAME_SIZE_BYTES + word] ==
              TEST_FILL_BYTE);
    }
}

static void test_failures_do_not_write(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char storage[RTS_CM4F_INITIAL_FRAME_SIZE_BYTES + 16u];
    unsigned char original[sizeof(storage)];
    rts_port_stack_result_t result;

    bytes_fill(storage, sizeof(storage), TEST_FILL_BYTE);
    bytes_fill(original, sizeof(original), TEST_FILL_BYTE);

    result = rts_port_stack_initialize(NULL, sizeof(storage), task_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    CHECK(result.saved_stack_pointer == NULL);
    result = rts_port_stack_initialize(storage, sizeof(storage), NULL, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_TASK_CONFIG);
    result = rts_port_stack_initialize(storage + 1u,
                                       RTS_CM4F_INITIAL_FRAME_SIZE_BYTES,
                                       task_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize(storage,
                                       RTS_CM4F_INITIAL_FRAME_SIZE_BYTES - 1u,
                                       task_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize(storage,
                                       RTS_CM4F_INITIAL_FRAME_SIZE_BYTES + 1u,
                                       task_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    result = rts_port_stack_initialize((void *)(UINTPTR_MAX - 15u), 64u,
                                       task_entry, NULL);
    CHECK(result.status == RTS_STATUS_INVALID_STACK);
    CHECK(bytes_equal(storage, original, sizeof(storage)));
}

static void test_deterministic(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char first[RTS_CM4F_INITIAL_FRAME_SIZE_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char second[RTS_CM4F_INITIAL_FRAME_SIZE_BYTES];
    rts_port_stack_result_t first_result;
    rts_port_stack_result_t second_result;

    bytes_fill(first, sizeof(first), 0x11u);
    bytes_fill(second, sizeof(second), 0xEEu);
    first_result = rts_port_stack_initialize(first, sizeof(first),
                                             task_entry, NULL);
    second_result = rts_port_stack_initialize(second, sizeof(second),
                                              task_entry, NULL);
    CHECK(first_result.status == RTS_STATUS_OK);
    CHECK(second_result.status == RTS_STATUS_OK);
    CHECK(bytes_equal(first, second, sizeof(first)));
}

int main(void)
{
    CHECK(rts_port_task_stack_minimum_size_bytes() == 64u);
    CHECK(rts_port_task_stack_size_granularity_bytes() == 16u);
    CHECK(RTS_CM4F_INITIAL_FRAME_WORD_COUNT == 16u);
    test_exact_frame();
    test_failures_do_not_write();
    test_deterministic();
    return test_failures;
}
