#include "port.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port_internal.h"

static bool rts_cm4f_add_is_valid(uintptr_t base, size_t size,
                                  uintptr_t *out_end)
{
    if (out_end == NULL || size > (size_t)(UINTPTR_MAX - base))
    {
        return false;
    }
    *out_end = base + (uintptr_t)size;
    return true;
}

static void rts_cm4f_word_write(unsigned char *destination, uint32_t value)
{
    destination[0] = (unsigned char)(value & UINT32_C(0xFF));
    destination[1] = (unsigned char)((value >> 8u) & UINT32_C(0xFF));
    destination[2] = (unsigned char)((value >> 16u) & UINT32_C(0xFF));
    destination[3] = (unsigned char)((value >> 24u) & UINT32_C(0xFF));
}

static uint32_t rts_cm4f_entry_encode(rts_task_entry_t entry)
{
    uint32_t encoded = 0u;
    unsigned char *encoded_bytes = (unsigned char *)&encoded;
    const unsigned char *entry_bytes = (const unsigned char *)&entry;
    size_t index;

    for (index = 0u; index < sizeof(encoded); ++index)
    {
        encoded_bytes[index] = entry_bytes[index];
    }
    return encoded;
}

static uint32_t rts_cm4f_trap_encode(void)
{
    void (*trap)(void) = rts_cm4f_task_return_trap;
    uint32_t encoded = 0u;
    unsigned char *encoded_bytes = (unsigned char *)&encoded;
    const unsigned char *trap_bytes = (const unsigned char *)&trap;
    size_t index;

    _Static_assert(sizeof(trap) == sizeof(encoded),
                   "task-return trap pointer must fit one frame word");
    for (index = 0u; index < sizeof(encoded); ++index)
    {
        encoded_bytes[index] = trap_bytes[index];
    }
    return encoded;
}

static void rts_cm4f_frame_word_write(unsigned char *frame, size_t word_index,
                                       uint32_t value)
{
    rts_cm4f_word_write(frame + (word_index * sizeof(uint32_t)), value);
}

size_t rts_port_task_stack_minimum_size_bytes(void)
{
    return RTS_CM4F_TASK_STACK_MINIMUM_SIZE_BYTES;
}

size_t rts_port_task_stack_size_granularity_bytes(void)
{
    return RTS_CM4F_TASK_STACK_GRANULARITY_BYTES;
}

rts_port_stack_result_t rts_port_stack_initialize(void *stack_buffer,
                                                   size_t stack_size_bytes,
                                                   rts_task_entry_t entry,
                                                   void *argument)
{
    rts_port_stack_result_t result = {
        .status = RTS_STATUS_INVALID_STACK,
        .saved_stack_pointer = NULL
    };
    uintptr_t base;
    uintptr_t end;
    uintptr_t aligned_top;
    uintptr_t frame_address;
    uintptr_t argument_value;
    uint32_t entry_word;
    uint32_t trap_word;
    unsigned char *frame;
    size_t index;

    if (entry == NULL)
    {
        result.status = RTS_STATUS_INVALID_TASK_CONFIG;
        return result;
    }
    if (stack_buffer == NULL)
    {
        return result;
    }

    base = (uintptr_t)stack_buffer;
    if ((base % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) != 0u ||
        stack_size_bytes < RTS_CM4F_TASK_STACK_MINIMUM_SIZE_BYTES ||
        (stack_size_bytes % RTS_CM4F_TASK_STACK_GRANULARITY_BYTES) != 0u ||
        !rts_cm4f_add_is_valid(base, stack_size_bytes, &end))
    {
        return result;
    }

    aligned_top = end & ~((uintptr_t)RTS_TASK_STACK_ALIGNMENT - 1u);
    if (aligned_top < base ||
        (aligned_top - base) < RTS_CM4F_INITIAL_FRAME_SIZE_BYTES)
    {
        return result;
    }
    frame_address = aligned_top - RTS_CM4F_INITIAL_FRAME_SIZE_BYTES;
    if (frame_address < base || frame_address > end ||
        (end - frame_address) < RTS_CM4F_INITIAL_FRAME_SIZE_BYTES ||
        (frame_address % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) != 0u)
    {
        return result;
    }

    argument_value = (uintptr_t)argument;
    if (argument_value > UINT32_MAX)
    {
        result.status = RTS_STATUS_PORT_ERROR;
        return result;
    }
    entry_word = rts_cm4f_entry_encode(entry);
    trap_word = rts_cm4f_trap_encode();
    if ((entry_word & UINT32_C(1)) == 0u ||
        (trap_word & UINT32_C(1)) == 0u)
    {
        result.status = RTS_STATUS_PORT_ERROR;
        return result;
    }

    frame = (unsigned char *)frame_address;
    for (index = 0u; index < RTS_CM4F_INITIAL_FRAME_WORD_COUNT; ++index)
    {
        rts_cm4f_frame_word_write(frame, index, 0u);
    }
    rts_cm4f_frame_word_write(frame, 8u, (uint32_t)argument_value);
    rts_cm4f_frame_word_write(frame, 13u, trap_word);
    rts_cm4f_frame_word_write(frame, 14u, entry_word);
    rts_cm4f_frame_word_write(frame, 15u, RTS_CM4F_INITIAL_XPSR);

    result.status = RTS_STATUS_OK;
    result.saved_stack_pointer = frame;
    return result;
}

_Noreturn void rts_cm4f_task_return_trap(void)
{
    RTS_FATAL_UNLESS(false);
    for (;;)
    {
    }
}
