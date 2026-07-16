#include "port.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port_internal.h"
#include "scheduler_internal.h"

#define RTS_HOST_FRAME_STORAGE_BYTES                                      \
    ((((sizeof(rts_host_initial_frame_t)) + RTS_TASK_STACK_ALIGNMENT - 1u) / \
      RTS_TASK_STACK_ALIGNMENT) * RTS_TASK_STACK_ALIGNMENT)

#define RTS_HOST_CRITICAL_NESTING_MAX 16u

static bool rts_host_in_isr;
static bool rts_host_interrupts_masked;
static bool rts_host_fail_next_stack_initialize;
static bool rts_host_fail_next_initialize;
static bool rts_host_initialized;
static bool rts_host_tick_ready;
static bool rts_host_tick_running;
static bool rts_host_fail_next_tick_start;
static size_t rts_host_critical_depth;
static size_t rts_host_switch_request_count;
static size_t rts_host_last_switch_request_critical_depth;
static bool rts_host_switch_request_pending;
static rts_critical_token_t
    rts_host_critical_tokens[RTS_HOST_CRITICAL_NESTING_MAX];

_Static_assert(RTS_HOST_FRAME_STORAGE_BYTES >= sizeof(rts_host_initial_frame_t),
               "host frame storage must contain the frame");
_Static_assert((RTS_HOST_FRAME_STORAGE_BYTES % RTS_TASK_STACK_ALIGNMENT) == 0u,
               "host frame storage must preserve stack alignment");

static void rts_host_bytes_copy(unsigned char *destination,
                                const unsigned char *source,
                                size_t byte_count)
{
    size_t index;

    for (index = 0u; index < byte_count; ++index)
    {
        destination[index] = source[index];
    }
}

static bool rts_host_stack_region_is_valid(void *stack_buffer,
                                           size_t stack_size_bytes,
                                           rts_task_entry_t entry)
{
    const uintptr_t stack_start = (uintptr_t)stack_buffer;

    if (stack_buffer == NULL || entry == NULL || stack_size_bytes == 0u ||
        stack_size_bytes < RTS_HOST_FRAME_STORAGE_BYTES ||
        (stack_size_bytes % RTS_TASK_STACK_ALIGNMENT) != 0u ||
        (stack_start % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) != 0u ||
        stack_size_bytes > UINTPTR_MAX)
    {
        return false;
    }

    if (stack_start > UINTPTR_MAX - (uintptr_t)stack_size_bytes)
    {
        return false;
    }

    return ((stack_start + (uintptr_t)stack_size_bytes) %
            (uintptr_t)RTS_TASK_STACK_ALIGNMENT) == 0u;
}

size_t rts_port_task_stack_minimum_size_bytes(void)
{
    return RTS_HOST_FRAME_STORAGE_BYTES;
}

rts_status_t rts_port_initialize(void)
{
    if (rts_host_fail_next_initialize)
    {
        rts_host_fail_next_initialize = false;
        return RTS_STATUS_PORT_ERROR;
    }

    rts_host_initialized = true;
    rts_host_tick_ready = true;
    return RTS_STATUS_OK;
}

size_t rts_port_task_stack_size_granularity_bytes(void)
{
    return RTS_TASK_STACK_ALIGNMENT;
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
    const rts_host_initial_frame_t frame = {
        .magic = RTS_HOST_INITIAL_FRAME_MAGIC,
        .version = RTS_HOST_INITIAL_FRAME_VERSION,
        .entry = entry,
        .argument = argument,
        .return_trap = rts_host_port_task_return_trap,
        .reserved = 0u
    };
    unsigned char *frame_start;
    size_t index;

    if (entry == NULL)
    {
        result.status = RTS_STATUS_INVALID_TASK_CONFIG;
        return result;
    }

    if (rts_host_fail_next_stack_initialize)
    {
        rts_host_fail_next_stack_initialize = false;
        result.status = RTS_STATUS_PORT_ERROR;
        return result;
    }

    if (!rts_host_stack_region_is_valid(stack_buffer, stack_size_bytes, entry))
    {
        return result;
    }

    frame_start = (unsigned char *)stack_buffer +
                  stack_size_bytes - RTS_HOST_FRAME_STORAGE_BYTES;
    for (index = 0u; index < RTS_HOST_FRAME_STORAGE_BYTES; ++index)
    {
        frame_start[index] = 0u;
    }
    rts_host_bytes_copy(frame_start + offsetof(rts_host_initial_frame_t, magic),
                        (const unsigned char *)&frame.magic,
                        sizeof(frame.magic));
    rts_host_bytes_copy(frame_start + offsetof(rts_host_initial_frame_t, version),
                        (const unsigned char *)&frame.version,
                        sizeof(frame.version));
    rts_host_bytes_copy(frame_start + offsetof(rts_host_initial_frame_t, entry),
                        (const unsigned char *)&frame.entry,
                        sizeof(frame.entry));
    rts_host_bytes_copy(frame_start + offsetof(rts_host_initial_frame_t, argument),
                        (const unsigned char *)&frame.argument,
                        sizeof(frame.argument));
    rts_host_bytes_copy(
        frame_start + offsetof(rts_host_initial_frame_t, return_trap),
        (const unsigned char *)&frame.return_trap, sizeof(frame.return_trap));
    rts_host_bytes_copy(frame_start + offsetof(rts_host_initial_frame_t, reserved),
                        (const unsigned char *)&frame.reserved,
                        sizeof(frame.reserved));

    result.saved_stack_pointer = frame_start;
    result.status = RTS_STATUS_OK;
    return result;
}

rts_critical_token_t rts_port_critical_enter(void)
{
    rts_critical_token_t token;

    RTS_ASSERT(rts_host_critical_depth < RTS_HOST_CRITICAL_NESTING_MAX);
    if (rts_host_critical_depth >= RTS_HOST_CRITICAL_NESTING_MAX)
    {
        return 0u;
    }

    token = (((rts_critical_token_t)rts_host_critical_depth + 1u) << 1u) |
            (rts_host_interrupts_masked ? 1u : 0u);
    rts_host_critical_tokens[rts_host_critical_depth] = token;
    ++rts_host_critical_depth;
    rts_host_interrupts_masked = true;
    return token;
}

void rts_port_critical_exit(rts_critical_token_t token)
{
    RTS_ASSERT(rts_host_critical_depth > 0u);
    if (rts_host_critical_depth == 0u)
    {
        return;
    }

    RTS_ASSERT(rts_host_critical_tokens[rts_host_critical_depth - 1u] == token);
    if (rts_host_critical_tokens[rts_host_critical_depth - 1u] != token)
    {
        return;
    }

    --rts_host_critical_depth;
    rts_host_interrupts_masked = (token & 1u) != 0u;
}

bool rts_port_is_in_isr(void)
{
    return rts_host_in_isr;
}

uint32_t rts_port_exception_number(void)
{
    return rts_host_in_isr ? UINT32_C(1) : UINT32_C(0);
}

void rts_port_fatal_disable(void)
{
    rts_host_interrupts_masked = true;
}

void rts_port_request_context_switch(void)
{
    RTS_ASSERT(rts_host_switch_request_count < SIZE_MAX);
    if (rts_host_switch_request_count == SIZE_MAX)
    {
        return;
    }
    ++rts_host_switch_request_count;
    rts_host_last_switch_request_critical_depth = rts_host_critical_depth;
    rts_host_switch_request_pending = true;
}

rts_status_t rts_port_tick_start(void)
{
    if (rts_host_fail_next_tick_start)
    {
        rts_host_fail_next_tick_start = false;
        return RTS_STATUS_PORT_ERROR;
    }
    if (!rts_host_tick_ready || rts_host_tick_running)
    {
        return RTS_STATUS_INVALID_STATE;
    }
    rts_host_tick_running = true;
    return RTS_STATUS_OK;
}

void rts_port_tick_stop(void)
{
    rts_host_tick_running = false;
}

bool rts_host_port_initial_frame_read(const void *saved_stack_pointer,
                                      rts_host_initial_frame_t *out_frame)
{
    if (saved_stack_pointer == NULL || out_frame == NULL)
    {
        return false;
    }

    rts_host_bytes_copy((unsigned char *)out_frame,
                        (const unsigned char *)saved_stack_pointer,
                        sizeof(*out_frame));
    return out_frame->magic == RTS_HOST_INITIAL_FRAME_MAGIC &&
           out_frame->version == RTS_HOST_INITIAL_FRAME_VERSION;
}

bool rts_port_tick_commit_start(void)
{
    return rts_host_tick_running;
}

void rts_host_port_task_return_trap(void)
{
    RTS_KERNEL_FATAL(RTS_FATAL_TASK_RETURNED,
                     rts_kernel_state_get()->current_task);
}

void rts_host_port_test_reset(void)
{
    size_t index;

    rts_host_in_isr = false;
    rts_host_interrupts_masked = false;
    rts_host_fail_next_stack_initialize = false;
    rts_host_fail_next_initialize = false;
    rts_host_initialized = false;
    rts_host_tick_ready = false;
    rts_host_tick_running = false;
    rts_host_fail_next_tick_start = false;
    rts_host_critical_depth = 0u;
    rts_host_switch_request_count = 0u;
    rts_host_last_switch_request_critical_depth = 0u;
    rts_host_switch_request_pending = false;
    for (index = 0u; index < RTS_HOST_CRITICAL_NESTING_MAX; ++index)
    {
        rts_host_critical_tokens[index] = 0u;
    }
}

void rts_host_port_test_set_isr(bool in_isr)
{
    rts_host_in_isr = in_isr;
}

void rts_host_port_test_fail_next_stack_initialize(bool fail)
{
    rts_host_fail_next_stack_initialize = fail;
}

void rts_host_port_test_fail_next_initialize(bool fail)
{
    rts_host_fail_next_initialize = fail;
}

void rts_host_port_test_fail_next_tick_start(bool fail)
{
    rts_host_fail_next_tick_start = fail;
}

bool rts_host_port_test_tick_running(void)
{
    return rts_host_tick_running;
}

bool rts_host_port_test_is_initialized(void)
{
    return rts_host_initialized;
}

size_t rts_host_port_test_critical_depth(void)
{
    return rts_host_critical_depth;
}

size_t rts_host_port_test_switch_request_count(void)
{
    return rts_host_switch_request_count;
}

size_t rts_host_port_test_last_switch_request_critical_depth(void)
{
    return rts_host_last_switch_request_critical_depth;
}

bool rts_host_port_test_switch_request_pending(void)
{
    return rts_host_switch_request_pending;
}

void rts_host_port_test_consume_switch_request(void)
{
    rts_host_switch_request_pending = false;
}
