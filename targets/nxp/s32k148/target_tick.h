#ifndef RTS_S32K148_TARGET_TICK_H
#define RTS_S32K148_TARGET_TICK_H

#include <stdbool.h>
#include <stdint.h>

#include "rts/rts_types.h"

bool rts_s32k148_tick_reload_calculate(uint32_t core_clock_hz,
                                       uint32_t tick_rate_hz,
                                       uint32_t *out_reload);
rts_status_t rts_s32k148_tick_initialize(uint32_t core_clock_hz,
                                         uint32_t tick_rate_hz);
bool rts_s32k148_tick_is_ready(void);
bool rts_s32k148_tick_is_running(void);
uint32_t rts_s32k148_tick_reload_get(void);
bool rts_s32k148_tick_isr_hook(void);
void SysTick_Handler(void);

#endif /* RTS_S32K148_TARGET_TICK_H */
