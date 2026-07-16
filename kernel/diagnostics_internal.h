#ifndef RTS_DIAGNOSTICS_INTERNAL_H
#define RTS_DIAGNOSTICS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "rts/rts_types.h"

typedef struct
{
    uint32_t scheduler_ticks;
    uint32_t context_switches;
    uint32_t switch_requests;
    uint32_t scheduler_starts;
    uint32_t task_creations;
    uint32_t yields;
    uint32_t delay_blocks;
    uint32_t delay_wakeups;
    uint32_t semaphore_blocks;
    uint32_t semaphore_acquisitions;
    uint32_t semaphore_timeouts;
    uint32_t mutex_blocks;
    uint32_t mutex_handoffs;
    uint32_t mutex_timeouts;
    uint32_t priority_raises;
    uint32_t priority_restorations;
} rts_runtime_counters_t;

typedef struct
{
    rts_tick_t tick;
    uint32_t context_switches;
    uint32_t task_count;
    uint32_t idle_ticks;
    uint32_t non_idle_ticks;
    uint32_t fatal_reason;
} rts_diagnostics_snapshot_t;

uint32_t rts_diagnostic_counter_increment(uint32_t value);
uint32_t rts_diagnostic_counter_add(uint32_t value, uint32_t increment);
void rts_runtime_task_started(struct rts_task *task, rts_tick_t now);
void rts_runtime_task_stopped(struct rts_task *task, rts_tick_t now);
bool rts_diagnostics_snapshot_read(rts_diagnostics_snapshot_t *snapshot);

#if RTS_ENABLE_RUNTIME_STATS
#define RTS_DIAG_COUNTER_INC(counter)                                      \
    ((counter) = rts_diagnostic_counter_increment((counter)))
#else
#define RTS_DIAG_COUNTER_INC(counter) ((void)0)
#endif

#endif /* RTS_DIAGNOSTICS_INTERNAL_H */
