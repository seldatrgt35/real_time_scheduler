#ifndef RTS_TASK_H
#define RTS_TASK_H

#include "rts/rts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create one task using a kernel-owned static slot and a caller-owned stack.
 *
 * Valid only in INITIALIZED state and non-ISR context. The stack referenced by
 * config must remain valid for the system lifetime. On success, out_handle
 * receives a stable opaque handle. Returns RTS_STATUS_CAPACITY_EXHAUSTED when
 * all RTS_MAX_TASKS application-task slots are allocated.
 */
rts_status_t rts_task_create(const rts_task_config_t *config,
                             rts_task_handle_t *out_handle);

/**
 * Yield to the oldest ready peer at the current task's priority, if one exists.
 *
 * Valid only in task context while the scheduler is RUNNING. Lower-priority
 * tasks are never selected solely because of a yield. Yield by the idle task
 * is a successful no-op because priority zero has no application peers.
 */
rts_status_t rts_task_yield(void);

/**
 * Block the current task for a relative number of scheduler ticks.
 *
 * Valid only from a non-idle task in task context while the scheduler is
 * RUNNING. A zero delay has the same semantics as rts_task_yield(). Nonzero
 * delays must not exceed RTS_DELAY_MAX.
 */
rts_status_t rts_task_delay(rts_tick_t delay);

#ifdef __cplusplus
}
#endif

#endif /* RTS_TASK_H */
