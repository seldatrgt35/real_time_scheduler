#ifndef RTS_TIMER_H
#define RTS_TIMER_H

#include <stdbool.h>

#include "rts/rts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rts_timer;
typedef struct rts_timer *rts_timer_handle_t;

typedef uint8_t rts_timer_mode_t;
enum
{
    RTS_TIMER_ONE_SHOT = 0,
    RTS_TIMER_PERIODIC = 1
};

typedef void (*rts_timer_callback_t)(void *argument);

typedef struct
{
    rts_tick_t period;
    rts_timer_callback_t callback;
    void *argument;
    rts_timer_mode_t mode;
} rts_timer_config_t;

/** Register one timer in the fixed pool while the kernel is INITIALIZED. */
rts_status_t rts_timer_init(const rts_timer_config_t *config,
                            rts_timer_handle_t *out_handle);
/** Arm a STOPPED or EXPIRED timer relative to the current tick. */
rts_status_t rts_timer_start(rts_timer_handle_t timer);
/** Cancel a RUNNING timer without invoking its callback. */
rts_status_t rts_timer_stop(rts_timer_handle_t timer);
/** Replace any current arm and schedule from the current tick. */
rts_status_t rts_timer_restart(rts_timer_handle_t timer);
/** Return false for invalid, stopped, expired, or ISR-context queries. */
bool rts_timer_is_running(rts_timer_handle_t timer);

#ifdef __cplusplus
}
#endif

#endif /* RTS_TIMER_H */
