#ifndef RTS_S32K148_SMOKE_TEST_H
#define RTS_S32K148_SMOKE_TEST_H

#include <stdint.h>

#include "rts/rts_types.h"

enum
{
    RTS_SMOKE_FAILURE_INIT = UINT32_C(1) << 0,
    RTS_SMOKE_FAILURE_CREATE_A = UINT32_C(1) << 1,
    RTS_SMOKE_FAILURE_CREATE_B = UINT32_C(1) << 2,
    RTS_SMOKE_FAILURE_START_RETURNED = UINT32_C(1) << 3,
    RTS_SMOKE_FAILURE_REGISTER = UINT32_C(1) << 4,
    RTS_SMOKE_FAILURE_THREAD_STACK = UINT32_C(1) << 5,
    RTS_SMOKE_FAILURE_ARGUMENT = UINT32_C(1) << 6,
    RTS_SMOKE_FAILURE_STACK_GUARD = UINT32_C(1) << 7,
    RTS_SMOKE_FAILURE_HANDLER_STACK = UINT32_C(1) << 8,
    RTS_SMOKE_FAILURE_YIELD = UINT32_C(1) << 9
};

typedef struct
{
    volatile uint32_t failure_flags;
    volatile uint32_t task_a_count;
    volatile uint32_t task_b_count;
    volatile uint32_t task_a_psp;
    volatile uint32_t task_b_psp;
    volatile uint32_t task_a_msp;
    volatile uint32_t task_b_msp;
    volatile uint32_t task_a_control;
    volatile uint32_t task_b_control;
    volatile uint32_t task_a_argument_seen;
    volatile uint32_t task_b_argument_seen;
    volatile rts_tick_t observed_tick;
} rts_s32k148_smoke_record_t;

extern rts_s32k148_smoke_record_t g_rts_s32k148_smoke_record;

uint32_t rts_smoke_verify_registers(const uint32_t *patterns);

#endif /* RTS_S32K148_SMOKE_TEST_H */
