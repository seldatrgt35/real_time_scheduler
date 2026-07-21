#ifndef RTS_S32K148_SMOKE_TEST_H
#define RTS_S32K148_SMOKE_TEST_H

#include <stdint.h>

#include "rts/rts_types.h"

#define RTS_SMOKE_TRACE_CAPACITY 128u

typedef enum
{
    RTS_SMOKE_TRACE_WORK_BEGIN = 1,
    RTS_SMOKE_TRACE_WORK_END,
    RTS_SMOKE_TRACE_BLOCK,
    RTS_SMOKE_TRACE_READY,
    RTS_SMOKE_TRACE_IDLE_ENTER,
    RTS_SMOKE_TRACE_IDLE_EXIT
} rts_smoke_trace_kind_t;

typedef struct
{
    volatile uint32_t sequence;
    volatile rts_tick_t tick;
    volatile uint32_t cycle;
    volatile uint32_t task_identifier;
    volatile uint32_t argument;
    volatile rts_smoke_trace_kind_t kind;
} rts_smoke_trace_entry_t;

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
    RTS_SMOKE_FAILURE_YIELD = UINT32_C(1) << 9,
    RTS_SMOKE_FAILURE_CREATE_C = UINT32_C(1) << 10,
    RTS_SMOKE_FAILURE_SEMAPHORE = UINT32_C(1) << 11,
    RTS_SMOKE_FAILURE_TIMER = UINT32_C(1) << 12,
    RTS_SMOKE_FAILURE_CREATE_D = UINT32_C(1) << 13,
    RTS_SMOKE_FAILURE_CREATE_E = UINT32_C(1) << 14,
    RTS_SMOKE_FAILURE_CREATE_F = UINT32_C(1) << 15,
    RTS_SMOKE_FAILURE_CREATE_G = UINT32_C(1) << 16,
    RTS_SMOKE_FAILURE_CREATE_H = UINT32_C(1) << 17
};

typedef struct
{
    volatile uint32_t failure_flags;
    volatile uint32_t task_a_count;
    volatile uint32_t task_b_count;
    volatile uint32_t task_c_count;
    volatile uint32_t task_a_psp;
    volatile uint32_t task_b_psp;
    volatile uint32_t task_a_msp;
    volatile uint32_t task_b_msp;
    volatile uint32_t task_a_control;
    volatile uint32_t task_b_control;
    volatile uint32_t task_c_psp;
    volatile uint32_t task_c_msp;
    volatile uint32_t task_c_control;
    volatile uint32_t task_a_argument_seen;
    volatile uint32_t task_b_argument_seen;
    volatile uint32_t task_c_argument_seen;
    volatile rts_tick_t observed_tick;
    volatile uint32_t current_task_identifier;
    volatile uint32_t last_low_priority_identifier;
    volatile uint32_t task_a_wakeup_count;
    volatile uint32_t time_slice_rotation_count;
    volatile uint32_t semaphore_isr_give_count;
    volatile uint32_t semaphore_acquired_count;
    volatile uint32_t semaphore_timeout_count;
    volatile uint32_t mutex_low_lock_count;
    volatile uint32_t mutex_high_handoff_count;
    volatile uint32_t mutex_low_unlock_count;
    volatile uint32_t diagnostic_context_switches;
    volatile uint32_t diagnostic_idle_ticks;
    volatile uint32_t diagnostic_non_idle_ticks;
    volatile uint32_t diagnostic_fatal_reason;
    volatile uint32_t diagnostic_invariant_failure;
    volatile uint32_t diagnostic_cycle_counter_available;
    volatile uint32_t diagnostic_maximum_critical_cycles;
    volatile uint32_t task_a_dispatch_count;
    volatile uint32_t task_b_dispatch_count;
    volatile uint32_t task_c_dispatch_count;
    volatile uint32_t task_a_running_ticks;
    volatile uint32_t task_b_running_ticks;
    volatile uint32_t task_c_running_ticks;
    volatile uint32_t task_a_stack_guard_ok;
    volatile uint32_t task_b_stack_guard_ok;
    volatile uint32_t task_c_stack_guard_ok;
    volatile uint32_t task_a_last_execution_cycles;
    volatile uint32_t task_b_last_execution_cycles;
    volatile uint32_t task_c_last_execution_cycles;
    volatile uint32_t task_a_max_execution_cycles;
    volatile uint32_t task_b_max_execution_cycles;
    volatile uint32_t task_c_max_execution_cycles;
    volatile uint32_t task_a_last_execution_us;
    volatile uint32_t task_b_last_execution_us;
    volatile uint32_t task_c_last_execution_us;
    volatile uint32_t task_a_max_execution_us;
    volatile uint32_t task_b_max_execution_us;
    volatile uint32_t task_c_max_execution_us;
    volatile uint32_t task_a_max_stack_used;
    volatile uint32_t task_b_max_stack_used;
    volatile uint32_t task_c_max_stack_used;
    volatile uint32_t task_d_count;
    volatile uint32_t task_e_count;
    volatile uint32_t task_f_count;
    volatile uint32_t task_g_count;
    volatile uint32_t task_h_count;
    volatile uint32_t task_d_argument_seen;
    volatile uint32_t task_e_argument_seen;
    volatile uint32_t task_f_argument_seen;
    volatile uint32_t task_g_argument_seen;
    volatile uint32_t task_h_argument_seen;
    volatile uint32_t task_d_psp, task_e_psp, task_f_psp, task_g_psp, task_h_psp;
    volatile uint32_t task_d_msp, task_e_msp, task_f_msp, task_g_msp, task_h_msp;
    volatile uint32_t task_d_control, task_e_control, task_f_control, task_g_control, task_h_control;
    volatile uint32_t task_d_stack_guard_ok, task_e_stack_guard_ok;
    volatile uint32_t task_f_stack_guard_ok, task_g_stack_guard_ok, task_h_stack_guard_ok;
    volatile uint32_t task_d_last_execution_cycles, task_e_last_execution_cycles;
    volatile uint32_t task_f_last_execution_cycles, task_g_last_execution_cycles, task_h_last_execution_cycles;
    volatile uint32_t task_d_max_execution_cycles, task_e_max_execution_cycles;
    volatile uint32_t task_f_max_execution_cycles, task_g_max_execution_cycles, task_h_max_execution_cycles;
    volatile uint32_t task_d_last_execution_us, task_e_last_execution_us;
    volatile uint32_t task_f_last_execution_us, task_g_last_execution_us, task_h_last_execution_us;
    volatile uint32_t task_d_max_execution_us, task_e_max_execution_us;
    volatile uint32_t task_f_max_execution_us, task_g_max_execution_us, task_h_max_execution_us;
    volatile uint32_t timer_periodic_callback_count;
    volatile uint32_t timer_one_shot_callback_count;
    volatile uint32_t timer_callback_psp;
    volatile uint32_t timer_callback_ipsr;
    volatile uint32_t timer_service_identity_valid;
    volatile uint32_t timer_stop_restart_count;
    volatile uint32_t tickless_before_sleep_count;
    volatile uint32_t tickless_after_sleep_count;
    volatile uint32_t tickless_elapsed_ticks;
} rts_s32k148_smoke_record_t;

extern rts_s32k148_smoke_record_t g_rts_s32k148_smoke_record;
extern volatile rts_smoke_trace_entry_t
    g_rts_s32k148_trace_log[RTS_SMOKE_TRACE_CAPACITY];
extern volatile uint32_t g_rts_s32k148_trace_sequence;

uint32_t rts_smoke_verify_registers(const uint32_t *patterns);

#endif /* RTS_S32K148_SMOKE_TEST_H */
