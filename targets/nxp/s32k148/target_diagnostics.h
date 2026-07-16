#ifndef RTS_S32K148_TARGET_DIAGNOSTICS_H
#define RTS_S32K148_TARGET_DIAGNOSTICS_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t cycle_counter_available;
    volatile uint32_t critical_start_cycle;
    volatile uint32_t maximum_critical_cycles;
} rts_s32k148_timing_record_t;

extern volatile rts_s32k148_timing_record_t g_rts_s32k148_timing_record;

void rts_s32k148_timing_initialize(void);
uint32_t rts_s32k148_cycle_now(void);
void rts_s32k148_timing_critical_begin(void);
void rts_s32k148_timing_critical_end(void);

#endif /* RTS_S32K148_TARGET_DIAGNOSTICS_H */
