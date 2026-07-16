#ifndef RTS_H
#define RTS_H

#include "rts/rts_types.h"
#include "rts/rts_task.h"
#include "rts/rts_semaphore.h"
#include "rts/rts_mutex.h"
#include "rts/rts_timer.h"
#include "rts/rts_power.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize scheduler state.
 *
 * Startup-only; must be called from non-ISR context while the scheduler is in
 * RESET. Returns RTS_STATUS_ALREADY_INITIALIZED after a successful prior call.
 * Returns RTS_STATUS_PORT_ERROR only for a recoverable architecture setup
 * failure that leaves the scheduler in RESET.
 */
rts_status_t rts_init(void);

/**
 * Start scheduling.
 *
 * Startup-only; requires INITIALIZED state. On success, transfers control to
 * the first task and does not return. Returns only when startup validation or
 * architecture startup fails; a recoverable architecture failure is reported
 * as RTS_STATUS_PORT_ERROR before any task begins executing.
 */
rts_status_t rts_start(void);

#ifdef __cplusplus
}
#endif

#endif /* RTS_H */
