#ifndef RTS_POWER_INTERNAL_H
#define RTS_POWER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "port.h"
#include "rts/rts_power.h"
#include "rts/rts_types.h"

typedef uint8_t rts_power_deadline_source_t;
enum
{
    RTS_POWER_DEADLINE_MAINTENANCE = 0,
    RTS_POWER_DEADLINE_DELAY,
    RTS_POWER_DEADLINE_TIMER
};

typedef struct
{
    rts_tick_t current_tick;
    rts_tick_t wake_tick;
    rts_tick_t sleep_ticks;
    rts_power_deadline_source_t source;
} rts_power_plan_t;

bool rts_power_sleep_is_allowed(void);
bool rts_power_plan_compute(rts_power_plan_t *out_plan);
void rts_power_idle(void);

#endif /* RTS_POWER_INTERNAL_H */
