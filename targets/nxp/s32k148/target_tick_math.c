#include "target_tick.h"

#include <stdbool.h>
#include <stdint.h>

#define RTS_SYSTICK_COUNTER_CAPACITY UINT32_C(0x01000000)

bool rts_s32k148_tick_reload_calculate(uint32_t core_clock_hz,
                                       uint32_t tick_rate_hz,
                                       uint32_t *out_reload)
{
    uint32_t cycles;

    if (out_reload == NULL || core_clock_hz == 0u || tick_rate_hz == 0u ||
        tick_rate_hz > core_clock_hz ||
        (core_clock_hz % tick_rate_hz) != 0u)
    {
        return false;
    }

    cycles = core_clock_hz / tick_rate_hz;
    if (cycles == 0u || cycles > RTS_SYSTICK_COUNTER_CAPACITY)
    {
        return false;
    }

    *out_reload = cycles - 1u;
    return true;
}
