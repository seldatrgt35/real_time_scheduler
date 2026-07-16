#include "diagnostics_internal.h"

#include <limits.h>

#include "fatal_internal.h"
#include "port.h"
#include "scheduler_internal.h"

uint32_t rts_diagnostic_counter_increment(uint32_t value)
{
    return value == UINT32_MAX ? UINT32_MAX : value + UINT32_C(1);
}

uint32_t rts_diagnostic_counter_add(uint32_t value, uint32_t increment)
{
    return increment > UINT32_MAX - value ? UINT32_MAX : value + increment;
}

void rts_runtime_task_started(rts_tcb_t *task, rts_tick_t now)
{
#if RTS_ENABLE_RUNTIME_STATS
    if (task != NULL)
    {
        task->diagnostic_last_start_tick = now;
        task->diagnostic_dispatch_count =
            rts_diagnostic_counter_increment(task->diagnostic_dispatch_count);
    }
#else
    (void)task;
    (void)now;
#endif
}

void rts_runtime_task_stopped(rts_tcb_t *task, rts_tick_t now)
{
#if RTS_ENABLE_RUNTIME_STATS
    if (task != NULL)
    {
        task->diagnostic_running_ticks +=
            now - task->diagnostic_last_start_tick;
    }
#else
    (void)task;
    (void)now;
#endif
}

bool rts_diagnostics_snapshot_read(rts_diagnostics_snapshot_t *snapshot)
{
#if RTS_ENABLE_DIAGNOSTICS
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_critical_token_t token;
    uint32_t idle_ticks = 0u;
    uint32_t total_ticks;

    if (snapshot == NULL)
    {
        return false;
    }
    token = rts_port_critical_enter();
#if RTS_ENABLE_RUNTIME_STATS
    idle_ticks = kernel->idle_task == NULL
                     ? 0u
                     : kernel->idle_task->diagnostic_running_ticks;
    if (kernel->current_task == kernel->idle_task &&
        kernel->lifecycle == RTS_KERNEL_RUNNING)
    {
        idle_ticks += kernel->current_tick -
                      kernel->idle_task->diagnostic_last_start_tick;
    }
    total_ticks = kernel->runtime_counters.scheduler_ticks;
    snapshot->context_switches = kernel->runtime_counters.context_switches;
    snapshot->tickless_sleep_entries =
        kernel->runtime_counters.tickless_sleep_entries;
    snapshot->tickless_suppressed_ticks =
        kernel->runtime_counters.tickless_suppressed_ticks;
    snapshot->tickless_longest_sleep =
        kernel->runtime_counters.tickless_longest_sleep;
#else
    total_ticks = kernel->current_tick;
    snapshot->context_switches = 0u;
    snapshot->tickless_sleep_entries = 0u;
    snapshot->tickless_suppressed_ticks = 0u;
    snapshot->tickless_longest_sleep = 0u;
#endif
    if (idle_ticks > total_ticks)
    {
        /* Saturation or a sampling interval beyond the modulo contract. */
        idle_ticks = total_ticks;
    }
    snapshot->tick = kernel->current_tick;
    snapshot->task_count =
        (uint32_t)rts_task_pool_allocated_count(&kernel->application_task_pool);
    snapshot->idle_ticks = idle_ticks;
    snapshot->non_idle_ticks = total_ticks - idle_ticks;
    snapshot->fatal_reason = g_rts_fatal_record.reason;
    rts_port_critical_exit(token);
    return true;
#else
    (void)snapshot;
    return false;
#endif
}
