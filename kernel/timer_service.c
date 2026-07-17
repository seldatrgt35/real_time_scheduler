#include "timer_internal.h"

#include <stddef.h>

#include "assert_internal.h"
#include "diagnostics_internal.h"
#include "port.h"
#include "kernel_lock.h"
#include "scheduler_internal.h"
#include "trace_internal.h"

bool rts_timer_service_process_one(void)
{
    rts_timer_manager_t *manager = rts_timer_manager_get();
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_callback_work_t work;
    rts_timer_callback_t callback;
    void *argument;
    rts_kernel_lock_token_t token;
    bool valid_context;

    valid_context = !rts_port_is_in_isr() &&
                    kernel->lifecycle == RTS_KERNEL_RUNNING &&
                    rts_scheduler_current_get() == kernel->timer_service_task &&
                    kernel->timer_service_task != NULL &&
                    kernel->timer_service_task->state ==
                        RTS_TASK_STATE_RUNNING;
    RTS_ASSERT(valid_context);
    if (!valid_context)
    {
        return false;
    }

    token = rts_kernel_lock_enter();
    if (!rts_timer_callback_queue_dequeue(&manager->callback_queue, &work))
    {
        rts_kernel_lock_exit(token);
        return false;
    }
    if (!rts_timer_handle_is_valid(work.timer) ||
        work.timer->generation != work.generation ||
        work.timer->callback_state != RTS_TIMER_CALLBACK_PENDING)
    {
        if (rts_timer_handle_is_valid(work.timer) &&
            work.timer->callback_state == RTS_TIMER_CALLBACK_PENDING)
        {
            work.timer->callback_state = RTS_TIMER_CALLBACK_IDLE;
#if RTS_ENABLE_RUNTIME_STATS
            RTS_DIAG_COUNTER_INC(work.timer->diagnostic_stale_count);
#endif
        }
#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_stale_callbacks);
#endif
        RTS_TRACE(RTS_TRACE_TIMER_STALE_CALLBACK, work.generation,
                  work.sequence);
        rts_kernel_lock_exit(token);
        return true;
    }

    work.timer->callback_state = RTS_TIMER_CALLBACK_RUNNING;
    callback = work.timer->callback;
    argument = work.timer->argument;
    RTS_FATAL_UNLESS(callback != NULL);
    RTS_TRACE(RTS_TRACE_TIMER_CALLBACK_BEGIN, work.timer->slot_index,
              work.sequence);
    rts_kernel_lock_exit(token);

    callback(argument);

    token = rts_kernel_lock_enter();
    RTS_FATAL_UNLESS(work.timer->callback_state ==
                     RTS_TIMER_CALLBACK_RUNNING);
    work.timer->callback_state = RTS_TIMER_CALLBACK_IDLE;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(work.timer->diagnostic_callback_count);
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_callbacks_executed);
#endif
    RTS_TRACE(RTS_TRACE_TIMER_CALLBACK_END, work.timer->slot_index,
              work.sequence);
    RTS_FATAL_UNLESS(rts_timer_manager_validate(manager));
    rts_kernel_lock_exit(token);
    return true;
}

size_t rts_timer_service_drain(void)
{
    size_t processed = 0u;

    while (rts_timer_service_process_one())
    {
        ++processed;
    }
    return processed;
}

void rts_timer_service_entry(void *argument)
{
    (void)argument;
    for (;;)
    {
        rts_kernel_lock_token_t token;
        bool notify_port;

        (void)rts_timer_service_drain();
        token = rts_kernel_lock_enter();
        if (rts_timer_manager_get()->callback_queue.count != 0u)
        {
            rts_kernel_lock_exit(token);
            continue;
        }
        notify_port = rts_scheduler_timer_service_block();
        RTS_FATAL_UNLESS(rts_kernel_state_get()->timer_service_task->state ==
                         RTS_TASK_STATE_BLOCKED);
        rts_kernel_lock_exit(token);
        if (notify_port)
        {
            rts_port_request_reschedule(rts_cpu_current_id());
        }
    }
}
