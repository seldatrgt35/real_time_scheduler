#ifndef RTS_TIME_INTERNAL_H
#define RTS_TIME_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "rts/rts_types.h"

#define RTS_TICK_HALF_RANGE (UINT32_C(0x80000000))
#define RTS_TICK_MAX_ADVANCE (UINT32_C(0x7fffffff))

_Static_assert(sizeof(rts_tick_t) == sizeof(uint32_t),
               "Version 1 scheduler time requires 32-bit ticks");

static inline bool rts_tick_before(rts_tick_t a, rts_tick_t b)
{
    const rts_tick_t distance = a - b;

    return distance != RTS_TICK_HALF_RANGE &&
           (distance & RTS_TICK_HALF_RANGE) != 0u;
}

static inline bool rts_tick_reached(rts_tick_t now, rts_tick_t deadline)
{
    return now == deadline || rts_tick_before(deadline, now);
}

static inline rts_tick_t rts_tick_elapsed(rts_tick_t start, rts_tick_t end)
{
    return end - start;
}

static inline bool rts_tick_relative_is_valid(rts_tick_t relative_ticks)
{
    return relative_ticks <= RTS_TICK_MAX_ADVANCE;
}

rts_tick_t rts_kernel_tick_now(void);
bool rts_kernel_tick_advance(rts_tick_t elapsed_ticks);
bool rts_kernel_time_skip(rts_tick_t elapsed_ticks);

#endif /* RTS_TIME_INTERNAL_H */
