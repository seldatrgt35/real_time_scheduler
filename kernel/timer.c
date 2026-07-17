#include "rts/rts_timer.h"

#include <stddef.h>
#include <stdint.h>

#include "assert_internal.h"
#include "diagnostics_internal.h"
#include "port.h"
#include "kernel_lock.h"
#include "scheduler_internal.h"
#include "time_internal.h"
#include "timer_internal.h"
#include "trace_internal.h"

static rts_timer_manager_t rts_timer_manager;

static void rts_timer_object_reset(struct rts_timer *timer, size_t slot_index)
{
    *timer = (struct rts_timer){0};
    rts_list_node_initialize(&timer->queue_node);
    timer->slot_index = slot_index;
    timer->mode = RTS_TIMER_ONE_SHOT;
    timer->state = RTS_TIMER_UNINITIALIZED;
    timer->callback_state = RTS_TIMER_CALLBACK_IDLE;
    timer->slot_state = RTS_TIMER_SLOT_FREE;
}

void rts_timer_manager_initialize(void)
{
    rts_timer_manager_t *manager = &rts_timer_manager;
    size_t index;

    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        rts_timer_object_reset(&manager->slots[index], index);
    }
    rts_timer_queue_initialize(&manager->active_queue);
    rts_timer_callback_queue_initialize(&manager->callback_queue);
    manager->allocated_count = 0u;
    manager->next_free_hint = 0u;
}

rts_timer_manager_t *rts_timer_manager_get(void)
{
    return &rts_timer_manager;
}

static struct rts_timer *rts_timer_reserve(rts_timer_manager_t *manager)
{
    size_t examined;

    if (manager == NULL ||
        manager->next_free_hint >= (size_t)RTS_MAX_TIMERS)
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
        return NULL;
    }
    for (examined = 0u; examined < (size_t)RTS_MAX_TIMERS; ++examined)
    {
        size_t index = manager->next_free_hint + examined;
        struct rts_timer *timer;

        if (index >= (size_t)RTS_MAX_TIMERS)
        {
            index -= (size_t)RTS_MAX_TIMERS;
        }
        timer = &manager->slots[index];
        if (timer->slot_state == RTS_TIMER_SLOT_FREE)
        {
            manager->next_free_hint = index + 1u;
            if (manager->next_free_hint == (size_t)RTS_MAX_TIMERS)
            {
                manager->next_free_hint = 0u;
            }
            timer->slot_state = RTS_TIMER_SLOT_RESERVED;
            return timer;
        }
    }
    return NULL;
}

static bool rts_timer_config_is_valid(const rts_timer_config_t *config)
{
    return config != NULL && config->callback != NULL &&
           config->period > 0u && config->period <= RTS_DELAY_MAX &&
           (config->mode == RTS_TIMER_ONE_SHOT ||
            config->mode == RTS_TIMER_PERIODIC);
}

static bool rts_timer_lifecycle_allows_control(
    const rts_kernel_state_t *kernel)
{
    return kernel->lifecycle == RTS_KERNEL_INITIALIZED ||
           kernel->lifecycle == RTS_KERNEL_RUNNING;
}

static struct rts_timer *rts_timer_resolve(rts_timer_handle_t handle)
{
    size_t index;

    if (handle == NULL)
    {
        return NULL;
    }
    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        if (handle == &rts_timer_manager.slots[index])
        {
            return rts_timer_manager.slots[index].slot_state ==
                           RTS_TIMER_SLOT_ALLOCATED
                       ? &rts_timer_manager.slots[index]
                       : NULL;
        }
    }
    return NULL;
}

bool rts_timer_handle_is_valid(rts_timer_handle_t timer)
{
    return rts_timer_resolve(timer) != NULL;
}

size_t rts_timer_allocated_count(void)
{
    return rts_timer_manager.allocated_count;
}

rts_status_t rts_timer_init(const rts_timer_config_t *config,
                            rts_timer_handle_t *out_handle)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_manager_t *manager = &rts_timer_manager;
    rts_kernel_lock_token_t token;
    struct rts_timer *timer;

    if (out_handle != NULL)
    {
        *out_handle = NULL;
    }
    if (config == NULL || out_handle == NULL ||
        !rts_timer_config_is_valid(config))
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    token = rts_kernel_lock_enter();
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        rts_kernel_lock_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    timer = rts_timer_reserve(manager);
    if (timer == NULL)
    {
        rts_kernel_lock_exit(token);
        return RTS_STATUS_CAPACITY_EXHAUSTED;
    }
    timer->period = config->period;
    timer->callback = config->callback;
    timer->argument = config->argument;
    timer->mode = config->mode;
    timer->state = RTS_TIMER_STOPPED;
    timer->callback_state = RTS_TIMER_CALLBACK_IDLE;
    timer->generation = UINT32_C(1);
#if RTS_ENABLE_ASSERTIONS
    timer->validation_magic = RTS_TIMER_VALIDATION_MAGIC;
#endif
    if (manager->allocated_count >= (size_t)RTS_MAX_TIMERS)
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
    }
    timer->slot_state = RTS_TIMER_SLOT_ALLOCATED;
    ++manager->allocated_count;
    *out_handle = timer;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_initializations);
#endif
    RTS_TRACE(RTS_TRACE_TIMER_INITIALIZED, timer->slot_index, timer->mode);
    RTS_FATAL_UNLESS(rts_timer_manager_validate(manager));
    rts_kernel_lock_exit(token);
    return RTS_STATUS_OK;
}

static size_t rts_timer_invalidate_pending(rts_timer_manager_t *manager,
                                           struct rts_timer *timer)
{
    size_t removed = rts_timer_callback_queue_remove_timer(
        &manager->callback_queue, timer);

    if (timer->callback_state == RTS_TIMER_CALLBACK_PENDING)
    {
        RTS_FATAL_UNLESS(removed == 1u);
        timer->callback_state = RTS_TIMER_CALLBACK_IDLE;
    }
    else
    {
        RTS_FATAL_UNLESS(removed == 0u);
    }
    if (manager->callback_queue.count == 0u)
    {
        rts_scheduler_timer_service_cancel_wake();
    }
#if RTS_ENABLE_RUNTIME_STATS
    timer->diagnostic_stale_count = rts_diagnostic_counter_add(
        timer->diagnostic_stale_count, (uint32_t)removed);
    rts_kernel_state_get()->runtime_counters.timer_stale_callbacks =
        rts_diagnostic_counter_add(
            rts_kernel_state_get()->runtime_counters.timer_stale_callbacks,
            (uint32_t)removed);
#endif
    return removed;
}

static rts_status_t rts_timer_arm(rts_timer_handle_t handle, bool restart)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_manager_t *manager = &rts_timer_manager;
    rts_kernel_lock_token_t token;
    struct rts_timer *timer;

    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (!rts_timer_lifecycle_allows_control(kernel))
    {
        return RTS_STATUS_INVALID_STATE;
    }
    token = rts_kernel_lock_enter();
    timer = rts_timer_resolve(handle);
    if (!rts_timer_lifecycle_allows_control(kernel) || timer == NULL)
    {
        rts_kernel_lock_exit(token);
        return timer == NULL ? RTS_STATUS_INVALID_ARGUMENT
                             : RTS_STATUS_INVALID_STATE;
    }
    if (!restart)
    {
        if (timer->state != RTS_TIMER_STOPPED ||
            timer->callback_state != RTS_TIMER_CALLBACK_IDLE)
        {
            rts_kernel_lock_exit(token);
            return RTS_STATUS_INVALID_STATE;
        }
    }
    else
    {
        if (timer->state == RTS_TIMER_ACTIVE)
        {
            rts_timer_queue_remove(&manager->active_queue, timer);
        }
        ++timer->generation;
        (void)rts_timer_invalidate_pending(manager, timer);
    }

    timer->expiration_tick = kernel->current_tick + timer->period;
    timer->state = RTS_TIMER_ACTIVE;
    rts_timer_queue_insert(&manager->active_queue, timer);
#if RTS_ENABLE_RUNTIME_STATS
    if (restart)
    {
        RTS_DIAG_COUNTER_INC(timer->diagnostic_restart_count);
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_restarts);
    }
    else
    {
        RTS_DIAG_COUNTER_INC(timer->diagnostic_start_count);
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_starts);
    }
#endif
    RTS_TRACE(restart ? RTS_TRACE_TIMER_RESTARTED : RTS_TRACE_TIMER_STARTED,
              timer->slot_index, timer->expiration_tick);
    RTS_FATAL_UNLESS(rts_timer_manager_validate(manager));
    rts_kernel_lock_exit(token);
    return RTS_STATUS_OK;
}

rts_status_t rts_timer_start(rts_timer_handle_t timer)
{
    return rts_timer_arm(timer, false);
}

rts_status_t rts_timer_restart(rts_timer_handle_t timer)
{
    return rts_timer_arm(timer, true);
}

rts_status_t rts_timer_stop(rts_timer_handle_t handle)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_manager_t *manager = &rts_timer_manager;
    rts_kernel_lock_token_t token;
    struct rts_timer *timer;
    bool had_effect;

    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (!rts_timer_lifecycle_allows_control(kernel))
    {
        return RTS_STATUS_INVALID_STATE;
    }
    token = rts_kernel_lock_enter();
    timer = rts_timer_resolve(handle);
    if (!rts_timer_lifecycle_allows_control(kernel) || timer == NULL)
    {
        rts_kernel_lock_exit(token);
        return timer == NULL ? RTS_STATUS_INVALID_ARGUMENT
                             : RTS_STATUS_INVALID_STATE;
    }
    had_effect = timer->state == RTS_TIMER_ACTIVE ||
                 timer->callback_state != RTS_TIMER_CALLBACK_IDLE;
    if (!had_effect)
    {
        rts_kernel_lock_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    if (timer->state == RTS_TIMER_ACTIVE)
    {
        rts_timer_queue_remove(&manager->active_queue, timer);
    }
    timer->state = RTS_TIMER_STOPPED;
    ++timer->generation;
    (void)rts_timer_invalidate_pending(manager, timer);
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(timer->diagnostic_stop_count);
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_stops);
#endif
    RTS_TRACE(RTS_TRACE_TIMER_STOPPED, timer->slot_index, timer->generation);
    RTS_FATAL_UNLESS(rts_timer_manager_validate(manager));
    rts_kernel_lock_exit(token);
    return RTS_STATUS_OK;
}

bool rts_timer_is_running(rts_timer_handle_t handle)
{
    rts_kernel_lock_token_t token;
    struct rts_timer *timer;
    bool active;

    if (rts_port_is_in_isr())
    {
        return false;
    }
    token = rts_kernel_lock_enter();
    timer = rts_timer_resolve(handle);
    active = timer != NULL && timer->state == RTS_TIMER_ACTIVE &&
             rts_timer_queue_contains(&rts_timer_manager.active_queue,
                                      timer);
    rts_kernel_lock_exit(token);
    return active;
}

static rts_tick_t rts_timer_periodic_next(struct rts_timer *timer,
                                          rts_tick_t now,
                                          uint32_t *missed)
{
    rts_tick_t lateness = now - timer->expiration_tick;
    rts_tick_t skipped = lateness / timer->period;
    rts_tick_t periods = skipped + 1u;

    *missed = skipped;
    return timer->expiration_tick + periods * timer->period;
}

bool rts_timer_manager_process_expired(rts_tick_t now)
{
    rts_timer_manager_t *manager = &rts_timer_manager;
    struct rts_timer *timer;
    bool service_woken = false;
#if RTS_ENABLE_RUNTIME_STATS
    rts_kernel_state_t *kernel = rts_kernel_state_get();
#endif

    for (;;)
    {
        rts_timer_callback_work_t work;
        rts_tick_t consumed_deadline;
        uint32_t missed = 0u;

        timer = rts_timer_queue_peek_expired(&manager->active_queue, now);
        if (timer == NULL)
        {
            break;
        }
        consumed_deadline = timer->expiration_tick;
        rts_timer_queue_remove(&manager->active_queue, timer);
        timer->last_expiration_tick = consumed_deadline;
        if (timer->mode == RTS_TIMER_PERIODIC)
        {
            timer->expiration_tick =
                rts_timer_periodic_next(timer, now, &missed);
            timer->state = RTS_TIMER_ACTIVE;
            rts_timer_queue_insert(&manager->active_queue, timer);
        }
        else
        {
            timer->state = RTS_TIMER_STOPPED;
        }

#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(timer->diagnostic_expiration_count);
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_expirations);
        timer->diagnostic_missed_period_count = rts_diagnostic_counter_add(
            timer->diagnostic_missed_period_count, missed);
        kernel->runtime_counters.timer_missed_periods =
            rts_diagnostic_counter_add(
                kernel->runtime_counters.timer_missed_periods, missed);
#endif
        RTS_TRACE(RTS_TRACE_TIMER_EXPIRED, timer->slot_index,
                  consumed_deadline);

        if (timer->callback_state != RTS_TIMER_CALLBACK_IDLE)
        {
#if RTS_ENABLE_RUNTIME_STATS
            RTS_DIAG_COUNTER_INC(timer->diagnostic_overrun_count);
            RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_overruns);
            RTS_DIAG_COUNTER_INC(timer->diagnostic_missed_period_count);
            RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_missed_periods);
#endif
            RTS_TRACE(RTS_TRACE_TIMER_OVERRUN, timer->slot_index,
                      consumed_deadline);
            continue;
        }

        ++timer->expiration_sequence;
        work.timer = timer;
        work.expiration_tick = consumed_deadline;
        work.generation = timer->generation;
        work.sequence = timer->expiration_sequence;
        if (!rts_timer_callback_queue_enqueue(&manager->callback_queue,
                                              &work))
        {
#if RTS_ENABLE_RUNTIME_STATS
            RTS_DIAG_COUNTER_INC(
                kernel->runtime_counters.timer_callback_queue_overflows);
#endif
            RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CALLBACK_QUEUE_OVERFLOW,
                             manager);
        }
        timer->callback_state = RTS_TIMER_CALLBACK_PENDING;
#if RTS_ENABLE_RUNTIME_STATS
        if ((uint32_t)manager->callback_queue.maximum_depth >
            kernel->runtime_counters.timer_callback_queue_maximum_depth)
        {
            kernel->runtime_counters.timer_callback_queue_maximum_depth =
                (uint32_t)manager->callback_queue.maximum_depth;
        }
#endif
        if (rts_scheduler_timer_service_wake())
        {
            service_woken = true;
        }
    }
    RTS_FATAL_UNLESS(rts_timer_manager_validate(manager));
    return service_woken;
}

bool rts_timer_manager_validate(const rts_timer_manager_t *manager)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    size_t index;
    size_t allocated = 0u;

    if (manager == NULL ||
        manager->next_free_hint >= (size_t)RTS_MAX_TIMERS ||
        manager->allocated_count > (size_t)RTS_MAX_TIMERS ||
        !rts_timer_queue_validate(&manager->active_queue) ||
        !rts_timer_callback_queue_validate(&manager->callback_queue))
    {
        return false;
    }
    {
        size_t queue_index = manager->callback_queue.head;
        size_t work_index;

        for (work_index = 0u;
             work_index < manager->callback_queue.count; ++work_index)
        {
            const rts_timer_callback_work_t *work =
                &manager->callback_queue.items[queue_index];
            bool owned = false;
            size_t slot_index;

            for (slot_index = 0u; slot_index < (size_t)RTS_MAX_TIMERS;
                 ++slot_index)
            {
                if (work->timer == &manager->slots[slot_index])
                {
                    owned = true;
                    break;
                }
            }
            if (!owned || work->timer->slot_state !=
                              RTS_TIMER_SLOT_ALLOCATED ||
                work->timer->callback_state != RTS_TIMER_CALLBACK_PENDING ||
                work->generation != work->timer->generation ||
                rts_timer_callback_queue_count_for(
                    &manager->callback_queue, work->timer) != 1u)
            {
                return false;
            }
            ++queue_index;
            if (queue_index ==
                (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY)
            {
                queue_index = 0u;
            }
        }
    }
    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        const struct rts_timer *timer = &manager->slots[index];
        bool linked = rts_timer_queue_contains(&manager->active_queue,
                                               timer);
        size_t pending = rts_timer_callback_queue_count_for(
            &manager->callback_queue, timer);

        if (timer->slot_state == RTS_TIMER_SLOT_FREE)
        {
            if (timer->state != RTS_TIMER_UNINITIALIZED || linked ||
                pending != 0u)
            {
                return false;
            }
            continue;
        }
        if (timer->slot_state == RTS_TIMER_SLOT_RESERVED)
        {
            if (linked || pending != 0u)
            {
                return false;
            }
            continue;
        }
        if (timer->slot_state != RTS_TIMER_SLOT_ALLOCATED ||
            timer->slot_index != index || timer->callback == NULL ||
            timer->period == 0u || timer->period > RTS_DELAY_MAX ||
            (timer->mode != RTS_TIMER_ONE_SHOT &&
             timer->mode != RTS_TIMER_PERIODIC) ||
            (timer->state != RTS_TIMER_STOPPED &&
             timer->state != RTS_TIMER_ACTIVE) ||
            linked != (timer->state == RTS_TIMER_ACTIVE) ||
            pending !=
                (timer->callback_state == RTS_TIMER_CALLBACK_PENDING ? 1u
                                                                     : 0u) ||
            timer->callback_state > RTS_TIMER_CALLBACK_RUNNING)
        {
            return false;
        }
#if RTS_ENABLE_ASSERTIONS
        if (timer->validation_magic != RTS_TIMER_VALIDATION_MAGIC)
        {
            return false;
        }
#endif
        ++allocated;
    }
    return allocated == manager->allocated_count;
#else
    (void)manager;
    return true;
#endif
}
