#ifndef RTS_POWER_H
#define RTS_POWER_H

#include <stdint.h>

#include "rts/rts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t rts_wake_source_t;
enum
{
    RTS_WAKE_SOURCE_TIMER = 0,
    RTS_WAKE_SOURCE_EXTERNAL,
    RTS_WAKE_SOURCE_GPIO,
    RTS_WAKE_SOURCE_OTHER
};

/** Optional bounded hook invoked before the target prepares low-power state. */
void rts_power_prepare_sleep(rts_tick_t planned_ticks);

/** Optional bounded hook invoked immediately before the port sleep call. */
void rts_power_before_sleep(rts_tick_t planned_ticks);

/** Optional bounded hook invoked after target clocks/peripherals are restored. */
void rts_power_resume_from_sleep(rts_tick_t elapsed_ticks,
                                 rts_wake_source_t source);

/** Optional bounded hook invoked after kernel-time compensation. */
void rts_power_after_sleep(rts_tick_t elapsed_ticks,
                           rts_wake_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* RTS_POWER_H */
