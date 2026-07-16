#ifndef RTS_TRACE_INTERNAL_H
#define RTS_TRACE_INTERNAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t rts_trace_event_t;
enum
{
    RTS_TRACE_TASK_CREATED = 1,
    RTS_TRACE_SCHEDULER_STARTED,
    RTS_TRACE_TASK_SWITCHED,
    RTS_TRACE_TASK_BLOCKED,
    RTS_TRACE_TASK_WOKE,
    RTS_TRACE_YIELD,
    RTS_TRACE_SEMAPHORE,
    RTS_TRACE_MUTEX,
    RTS_TRACE_PRIORITY,
    RTS_TRACE_TICK,
    RTS_TRACE_FATAL
};

typedef struct
{
    uint32_t sequence;
    uint32_t tick;
    uintptr_t argument0;
    uintptr_t argument1;
    rts_trace_event_t event;
    uint8_t reserved[3];
} rts_trace_entry_t;

void rts_trace_emit(rts_trace_event_t event, uintptr_t arg0, uintptr_t arg1);
size_t rts_trace_count(void);
uint32_t rts_trace_overwrite_count(void);
bool rts_trace_read(size_t oldest_index, rts_trace_entry_t *entry);
void rts_trace_reset_for_test(void);

#if RTS_ENABLE_TRACE
#define RTS_TRACE(event, arg0, arg1) \
    rts_trace_emit((event), (uintptr_t)(arg0), (uintptr_t)(arg1))
#else
#define RTS_TRACE(event, arg0, arg1) ((void)0)
#endif

#endif /* RTS_TRACE_INTERNAL_H */
