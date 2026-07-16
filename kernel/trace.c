#include "trace_internal.h"

#include <stdbool.h>

#include "diagnostics_internal.h"
#include "port.h"
#include "scheduler_internal.h"

#if RTS_ENABLE_TRACE
static rts_trace_entry_t rts_trace_buffer[RTS_TRACE_CAPACITY];
static size_t rts_trace_head;
static size_t rts_trace_used;
static uint32_t rts_trace_sequence;
static uint32_t rts_trace_overwrites;
#endif

void rts_trace_emit(rts_trace_event_t event, uintptr_t arg0, uintptr_t arg1)
{
#if RTS_ENABLE_TRACE
    rts_critical_token_t token = rts_port_critical_enter();
    size_t index;

    if (rts_trace_used < (size_t)RTS_TRACE_CAPACITY)
    {
        index = (rts_trace_head + rts_trace_used) %
                (size_t)RTS_TRACE_CAPACITY;
        ++rts_trace_used;
    }
    else
    {
        index = rts_trace_head;
        rts_trace_head = (rts_trace_head + 1u) %
                         (size_t)RTS_TRACE_CAPACITY;
        rts_trace_overwrites =
            rts_diagnostic_counter_increment(rts_trace_overwrites);
    }
    rts_trace_sequence = rts_diagnostic_counter_increment(rts_trace_sequence);
    rts_trace_buffer[index].sequence = rts_trace_sequence;
    rts_trace_buffer[index].tick = rts_kernel_state_get()->current_tick;
    rts_trace_buffer[index].argument0 = arg0;
    rts_trace_buffer[index].argument1 = arg1;
    rts_trace_buffer[index].event = event;
    rts_port_critical_exit(token);
#else
    (void)event;
    (void)arg0;
    (void)arg1;
#endif
}

size_t rts_trace_count(void)
{
#if RTS_ENABLE_TRACE
    rts_critical_token_t token = rts_port_critical_enter();
    size_t count = rts_trace_used;

    rts_port_critical_exit(token);
    return count;
#else
    return 0u;
#endif
}

uint32_t rts_trace_overwrite_count(void)
{
#if RTS_ENABLE_TRACE
    rts_critical_token_t token = rts_port_critical_enter();
    uint32_t count = rts_trace_overwrites;

    rts_port_critical_exit(token);
    return count;
#else
    return 0u;
#endif
}

bool rts_trace_read(size_t oldest_index, rts_trace_entry_t *entry)
{
#if RTS_ENABLE_TRACE
    rts_critical_token_t token;
    size_t index;

    if (entry == NULL)
    {
        return false;
    }
    token = rts_port_critical_enter();
    if (oldest_index >= rts_trace_used)
    {
        rts_port_critical_exit(token);
        return false;
    }
    index = (rts_trace_head + oldest_index) % (size_t)RTS_TRACE_CAPACITY;
    *entry = rts_trace_buffer[index];
    rts_port_critical_exit(token);
    return true;
#else
    (void)oldest_index;
    (void)entry;
    return false;
#endif
}

void rts_trace_reset_for_test(void)
{
#if RTS_ENABLE_TRACE
    rts_trace_head = 0u;
    rts_trace_used = 0u;
    rts_trace_sequence = 0u;
    rts_trace_overwrites = 0u;
#endif
}
