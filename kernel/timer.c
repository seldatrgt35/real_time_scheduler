#include "rts/rts_timer.h"

#include <stddef.h>

#include "assert_internal.h"
#include "diagnostics_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "time_internal.h"
#include "timer_internal.h"
#include "trace_internal.h"

static rts_timer_manager_t rts_timer_manager;

static void rts_timer_object_reset(struct rts_timer *timer, size_t slot_index)
{
    timer->expiration_tick = 0u;
    timer->last_expiration_tick = 0u;
    timer->period = 0u;
    timer->callback = NULL;
    timer->argument = NULL;
    rts_list_node_initialize(&timer->queue_node);
    timer->slot_index = slot_index;
    timer->mode = RTS_TIMER_ONE_SHOT;
    timer->state = RTS_TIMER_UNINITIALIZED;
    timer->slot_state = RTS_TIMER_SLOT_FREE;
#if RTS_ENABLE_RUNTIME_STATS
    timer->diagnostic_start_count = 0u;
    timer->diagnostic_stop_count = 0u;
    timer->diagnostic_restart_count = 0u;
    timer->diagnostic_expiration_count = 0u;
#endif
#if RTS_ENABLE_ASSERTIONS
    timer->validation_magic = 0u;
#endif
}

void rts_timer_manager_initialize(void)
{
    rts_timer_manager_t *manager = &rts_timer_manager;
    size_t index;

    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        rts_timer_object_reset(&manager->slots[index], index);
    }
    rts_timer_queue_initialize(&manager->running_queue);
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
    }
    if (manager == NULL ||
        manager->next_free_hint >= (size_t)RTS_MAX_TIMERS)
    {
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
    rts_timer_manager_t *manager = &rts_timer_manager;
    size_t index;

    if (handle == NULL)
    {
        return NULL;
    }
    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        if (handle == &manager->slots[index])
        {
            return manager->slots[index].slot_state ==
                           RTS_TIMER_SLOT_ALLOCATED
                       ? &manager->slots[index]
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
    rts_critical_token_t token;
    struct rts_timer *timer;

    if (out_handle != NULL)
    {
        *out_handle = NULL;
    }
    if (config == NULL || out_handle == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    if (!rts_timer_config_is_valid(config))
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

    token = rts_port_critical_enter();
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    timer = rts_timer_reserve(manager);
    if (timer == NULL)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_CAPACITY_EXHAUSTED;
    }
    timer->expiration_tick = 0u;
    timer->last_expiration_tick = 0u;
    timer->period = config->period;
    timer->callback = config->callback;
    timer->argument = config->argument;
    rts_list_node_initialize(&timer->queue_node);
    timer->mode = config->mode;
    timer->state = RTS_TIMER_STOPPED;
#if RTS_ENABLE_RUNTIME_STATS
    timer->diagnostic_start_count = 0u;
    timer->diagnostic_stop_count = 0u;
    timer->diagnostic_restart_count = 0u;
    timer->diagnostic_expiration_count = 0u;
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_initializations);
#endif
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
    RTS_TRACE(RTS_TRACE_TIMER_INITIALIZED, timer->slot_index, timer->mode);
    if (!rts_timer_manager_validate(manager))
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
    }
    rts_port_critical_exit(token);
    return RTS_STATUS_OK;
}

static rts_status_t rts_timer_arm(rts_timer_handle_t handle, bool restart)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_manager_t *manager = &rts_timer_manager;
    rts_critical_token_t token;
    struct rts_timer *timer;

    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (!rts_timer_lifecycle_allows_control(kernel))
    {
        return RTS_STATUS_INVALID_STATE;
    }
    token = rts_port_critical_enter();
    timer = rts_timer_resolve(handle);
    if (!rts_timer_lifecycle_allows_control(kernel) || timer == NULL)
    {
        rts_port_critical_exit(token);
        return timer == NULL ? RTS_STATUS_INVALID_ARGUMENT
                             : RTS_STATUS_INVALID_STATE;
    }
    if (!restart && timer->state == RTS_TIMER_RUNNING)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    if (restart && timer->state == RTS_TIMER_RUNNING)
    {
        rts_timer_queue_remove(&manager->running_queue, timer);
    }
    else if (timer->state != RTS_TIMER_STOPPED &&
             timer->state != RTS_TIMER_EXPIRED)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }

    timer->expiration_tick = kernel->current_tick + timer->period;
    timer->state = RTS_TIMER_RUNNING;
    rts_timer_queue_insert(&manager->running_queue, timer);
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
    if (!rts_timer_manager_validate(manager))
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
    }
    rts_port_critical_exit(token);
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
    rts_critical_token_t token;
    struct rts_timer *timer;

    if (rts_port_is_in_isr())
    {
        return RTS_STATUS_INVALID_CONTEXT;
    }
    if (!rts_timer_lifecycle_allows_control(kernel))
    {
        return RTS_STATUS_INVALID_STATE;
    }
    token = rts_port_critical_enter();
    timer = rts_timer_resolve(handle);
    if (!rts_timer_lifecycle_allows_control(kernel) || timer == NULL)
    {
        rts_port_critical_exit(token);
        return timer == NULL ? RTS_STATUS_INVALID_ARGUMENT
                             : RTS_STATUS_INVALID_STATE;
    }
    if (timer->state != RTS_TIMER_RUNNING)
    {
        rts_port_critical_exit(token);
        return RTS_STATUS_INVALID_STATE;
    }
    rts_timer_queue_remove(&manager->running_queue, timer);
    timer->state = RTS_TIMER_STOPPED;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(timer->diagnostic_stop_count);
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_stops);
#endif
    RTS_TRACE(RTS_TRACE_TIMER_STOPPED, timer->slot_index, 0u);
    if (!rts_timer_manager_validate(manager))
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
    }
    rts_port_critical_exit(token);
    return RTS_STATUS_OK;
}

bool rts_timer_is_running(rts_timer_handle_t handle)
{
    rts_critical_token_t token;
    struct rts_timer *timer;
    bool running;

    if (rts_port_is_in_isr())
    {
        return false;
    }
    token = rts_port_critical_enter();
    timer = rts_timer_resolve(handle);
    running = timer != NULL && timer->state == RTS_TIMER_RUNNING &&
              rts_timer_queue_contains(
                  &rts_timer_manager.running_queue,
                  timer);
    rts_port_critical_exit(token);
    return running;
}

void rts_timer_manager_process_expired(rts_tick_t now)
{
    rts_timer_manager_t *manager = &rts_timer_manager;
    struct rts_timer *timer;
#if RTS_ENABLE_RUNTIME_STATS
    rts_kernel_state_t *kernel = rts_kernel_state_get();
#endif

    for (;;)
    {
        timer = rts_timer_queue_peek_expired(&manager->running_queue, now);
        if (timer == NULL)
        {
            break;
        }
        rts_timer_queue_remove(&manager->running_queue, timer);
        timer->last_expiration_tick = timer->expiration_tick;
        if (timer->mode == RTS_TIMER_PERIODIC)
        {
            timer->expiration_tick += timer->period;
        }
        timer->state = RTS_TIMER_EXPIRED;
#if RTS_ENABLE_RUNTIME_STATS
        RTS_DIAG_COUNTER_INC(timer->diagnostic_expiration_count);
        RTS_DIAG_COUNTER_INC(kernel->runtime_counters.timer_expirations);
#endif
        RTS_TRACE(RTS_TRACE_TIMER_EXPIRED, timer->slot_index,
                  timer->last_expiration_tick);
    }
    if (!rts_timer_manager_validate(manager))
    {
        RTS_KERNEL_FATAL(RTS_FATAL_TIMER_CORRUPTION, manager);
    }
}

bool rts_timer_manager_validate(const rts_timer_manager_t *manager)
{
#if RTS_ENABLE_INVARIANT_CHECKS
    size_t index;
    size_t allocated = 0u;

    if (manager == NULL ||
        manager->next_free_hint >= (size_t)RTS_MAX_TIMERS ||
        manager->allocated_count > (size_t)RTS_MAX_TIMERS ||
        !rts_timer_queue_validate(&manager->running_queue))
    {
        return false;
    }
    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        const struct rts_timer *timer = &manager->slots[index];
        bool linked = rts_timer_queue_contains(&manager->running_queue, timer);

        if (timer->slot_state == RTS_TIMER_SLOT_FREE)
        {
            if (timer->state != RTS_TIMER_UNINITIALIZED || linked)
            {
                return false;
            }
            continue;
        }
        if (timer->slot_state == RTS_TIMER_SLOT_RESERVED)
        {
            if (linked)
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
            timer->state == RTS_TIMER_UNINITIALIZED ||
            linked != (timer->state == RTS_TIMER_RUNNING))
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
