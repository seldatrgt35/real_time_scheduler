#include "power_internal.h"

#include <stddef.h>

#include "assert_internal.h"
#include "diagnostics_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "time_internal.h"
#include "timer_internal.h"
#include "trace_internal.h"

#if defined(__GNUC__) || defined(__clang__)
#define RTS_POWER_WEAK __attribute__((weak))
#else
#define RTS_POWER_WEAK
#endif

RTS_POWER_WEAK void rts_power_prepare_sleep(rts_tick_t planned_ticks)
{
    (void)planned_ticks;
}

RTS_POWER_WEAK void rts_power_before_sleep(rts_tick_t planned_ticks)
{
    (void)planned_ticks;
}

RTS_POWER_WEAK void rts_power_resume_from_sleep(
    rts_tick_t elapsed_ticks,
    rts_port_wake_source_t source)
{
    (void)elapsed_ticks;
    (void)source;
}

RTS_POWER_WEAK void rts_power_after_sleep(rts_tick_t elapsed_ticks,
                                           rts_port_wake_source_t source)
{
    (void)elapsed_ticks;
    (void)source;
}

bool rts_power_sleep_is_allowed(void)
{
#if RTS_ENABLE_TICKLESS_IDLE
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    return kernel->lifecycle == RTS_KERNEL_RUNNING &&
           !rts_port_is_in_isr() && kernel->current_task != NULL &&
           kernel->current_task == kernel->idle_task &&
           kernel->idle_task->state == RTS_TASK_STATE_RUNNING &&
           !kernel->switch_plan.pending && !kernel->switch_plan.active &&
           rts_ready_only_contains(&kernel->ready_set, kernel->idle_task);
#else
    return false;
#endif
}

static void rts_power_consider_deadline(rts_tick_t now,
                                        rts_tick_t candidate,
                                        rts_power_deadline_source_t source,
                                        rts_power_plan_t *plan)
{
    rts_tick_t distance;

    if (rts_tick_reached(now, candidate))
    {
        plan->wake_tick = now;
        plan->sleep_ticks = 0u;
        plan->source = source;
        return;
    }

    distance = candidate - now;
    RTS_ASSERT(distance <= RTS_TICK_MAX_ADVANCE);
    if (distance <= RTS_TICK_MAX_ADVANCE && distance < plan->sleep_ticks)
    {
        plan->wake_tick = candidate;
        plan->sleep_ticks = distance;
        plan->source = source;
    }
}

bool rts_power_plan_compute(rts_power_plan_t *out_plan)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tick_t deadline;

    if (out_plan == NULL || !rts_power_sleep_is_allowed())
    {
        return false;
    }

    out_plan->current_tick = kernel->current_tick;
    out_plan->sleep_ticks = (rts_tick_t)RTS_TICKLESS_MAX_SLEEP_TICKS;
    out_plan->wake_tick = kernel->current_tick + out_plan->sleep_ticks;
    out_plan->source = RTS_POWER_DEADLINE_MAINTENANCE;

    if (rts_delay_next_deadline(&kernel->delay_queue, &deadline))
    {
        rts_power_consider_deadline(kernel->current_tick, deadline,
                                    RTS_POWER_DEADLINE_DELAY, out_plan);
    }
    if (rts_timer_queue_next_deadline(
            &rts_timer_manager_get()->active_queue, &deadline))
    {
        rts_power_consider_deadline(kernel->current_tick, deadline,
                                    RTS_POWER_DEADLINE_TIMER, out_plan);
    }
    return out_plan->sleep_ticks != 0u;
}

static void rts_power_record_wake(rts_kernel_state_t *kernel,
                                  const rts_port_sleep_result_t *result)
{
#if RTS_ENABLE_RUNTIME_STATS
    kernel->runtime_counters.tickless_suppressed_ticks =
        rts_diagnostic_counter_add(
            kernel->runtime_counters.tickless_suppressed_ticks,
            result->elapsed_ticks);
    if (result->elapsed_ticks >
        kernel->runtime_counters.tickless_longest_sleep)
    {
        kernel->runtime_counters.tickless_longest_sleep =
            result->elapsed_ticks;
    }
    if (result->wake_source == RTS_PORT_WAKE_TIMER)
    {
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.tickless_timer_wakes);
    }
    else
    {
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.tickless_external_wakes);
    }
#else
    (void)kernel;
    (void)result;
#endif
}

void rts_power_idle(void)
{
#if RTS_ENABLE_TICKLESS_IDLE
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_port_sleep_result_t result;
    rts_power_plan_t plan;
    rts_critical_token_t token;
    bool request_switch = false;
    bool valid_result;

    if (!rts_power_sleep_is_allowed())
    {
        return;
    }

    token = rts_port_critical_enter();
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.tickless_sleep_attempts);
#endif
    if (!rts_power_plan_compute(&plan))
    {
        rts_port_critical_exit(token);
        return;
    }

    rts_power_prepare_sleep(plan.sleep_ticks);
    rts_power_before_sleep(plan.sleep_ticks);
    RTS_TRACE(RTS_TRACE_SLEEP_ENTER, plan.sleep_ticks, plan.source);
    result = rts_port_power_sleep(plan.sleep_ticks);
    valid_result = result.status == RTS_STATUS_OK &&
                   result.elapsed_ticks <= plan.sleep_ticks &&
                   result.wake_source <= RTS_PORT_WAKE_OTHER;
    if (result.status == RTS_STATUS_OK)
    {
        RTS_ASSERT(valid_result);
    }
    if (!valid_result)
    {
#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.tickless_sleep_aborts);
#endif
        RTS_TRACE(RTS_TRACE_SLEEP_ABORT, result.status,
                  result.elapsed_ticks);
        rts_port_critical_exit(token);
        return;
    }

#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.tickless_sleep_entries);
#endif
    rts_power_resume_from_sleep(result.elapsed_ticks, result.wake_source);
    if (result.elapsed_ticks != 0u)
    {
        request_switch = rts_kernel_time_skip(result.elapsed_ticks);
    }
    rts_power_record_wake(kernel, &result);
    rts_power_after_sleep(result.elapsed_ticks, result.wake_source);
    RTS_TRACE(RTS_TRACE_SLEEP_EXIT, result.elapsed_ticks,
              result.wake_source);
    rts_port_critical_exit(token);

    if (request_switch)
    {
        rts_port_request_context_switch();
    }
#endif
}
