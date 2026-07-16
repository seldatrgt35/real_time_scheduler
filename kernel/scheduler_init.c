#include "rts/rts.h"

#include <stdbool.h>

#include "assert_internal.h"
#include "port.h"
#include "power_internal.h"
#include "scheduler_policy.h"
#include "scheduler_internal.h"
#include "stack_check_internal.h"
#include "timer_internal.h"

/* Focused bootstrap builds do not link the optional power subsystem. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void rts_power_idle(void)
{
}
#endif

static void rts_idle_entry(void *argument)
{
    (void)argument;
    for (;;)
    {
        rts_power_idle();
    }
}

static void rts_kernel_restore_reset(rts_kernel_state_t *kernel)
{
    *kernel = (rts_kernel_state_t){0};
    rts_timer_manager_initialize();
}

static void rts_idle_object_initialize(rts_kernel_state_t *kernel)
{
    rts_tcb_t *idle = &kernel->idle_task_storage;

    idle->saved_stack_pointer = NULL;
    idle->stack_low = kernel->idle_stack;
    idle->stack_high = kernel->idle_stack + sizeof(kernel->idle_stack);
    idle->entry = rts_idle_entry;
    idle->argument = NULL;
    rts_list_node_initialize(&idle->ready_node);
    rts_list_node_initialize(&idle->delay_node);
    idle->wait_node.previous = NULL;
    idle->wait_node.next = NULL;
    idle->wait_node.owner = NULL;
    idle->wait.reason = RTS_WAIT_NONE;
    idle->wait.result = RTS_WAIT_RESULT_NONE;
    idle->wait.wake_tick = 0u;
    idle->wait.object = NULL;
    idle->wait.timeout_active = false;
    idle->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    idle->owned_mutex_head = NULL;
    idle->owned_mutex_tail = NULL;
    idle->owned_mutex_count = 0u;
    idle->base_priority = RTS_IDLE_PRIORITY;
    idle->priority = RTS_IDLE_PRIORITY;
    idle->period = 0u;
    idle->relative_deadline = 0u;
    idle->absolute_deadline = 0u;
    idle->release_tick = 0u;
    idle->execution_budget = 0u;
    idle->release_sequence = 0u;
    idle->state = RTS_TASK_STATE_DORMANT;
    idle->slot_state = RTS_TASK_SLOT_ALLOCATED;
#if RTS_ENABLE_RUNTIME_STATS
    idle->diagnostic_dispatch_count = 0u;
    idle->diagnostic_block_count = 0u;
    idle->diagnostic_wake_count = 0u;
    idle->diagnostic_running_ticks = 0u;
    idle->diagnostic_last_start_tick = 0u;
#endif
#if RTS_ENABLE_STACK_WATERMARK
    idle->diagnostic_max_stack_used = 0u;
#endif
#if RTS_ENABLE_ASSERTIONS
    idle->validation_magic = 0u;
#endif
}

static void rts_timer_service_object_initialize(rts_kernel_state_t *kernel)
{
    rts_tcb_t *service = &kernel->timer_service_task_storage;

    service->saved_stack_pointer = NULL;
    service->stack_low = kernel->timer_service_stack;
    service->stack_high = kernel->timer_service_stack +
                          sizeof(kernel->timer_service_stack);
    service->entry = rts_timer_service_entry;
    service->argument = NULL;
    rts_list_node_initialize(&service->ready_node);
    rts_list_node_initialize(&service->delay_node);
    service->wait_node.previous = NULL;
    service->wait_node.next = NULL;
    service->wait_node.owner = NULL;
    service->wait.reason = RTS_WAIT_TIMER_SERVICE;
    service->wait.result = RTS_WAIT_RESULT_NONE;
    service->wait.wake_tick = 0u;
    service->wait.object = NULL;
    service->wait.timeout_active = false;
    service->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    service->owned_mutex_head = NULL;
    service->owned_mutex_tail = NULL;
    service->owned_mutex_count = 0u;
    service->base_priority = (rts_priority_t)RTS_TIMER_SERVICE_PRIORITY;
    service->priority = (rts_priority_t)RTS_TIMER_SERVICE_PRIORITY;
    service->period = 1u;
    service->relative_deadline = 1u;
    service->absolute_deadline = 0u;
    service->release_tick = 0u;
    service->execution_budget = 0u;
    service->release_sequence = 0u;
    service->state = RTS_TASK_STATE_BLOCKED;
    service->slot_state = RTS_TASK_SLOT_ALLOCATED;
#if RTS_ENABLE_RUNTIME_STATS
    service->diagnostic_dispatch_count = 0u;
    service->diagnostic_block_count = 0u;
    service->diagnostic_wake_count = 0u;
    service->diagnostic_running_ticks = 0u;
    service->diagnostic_last_start_tick = 0u;
#endif
#if RTS_ENABLE_STACK_WATERMARK
    service->diagnostic_max_stack_used = 0u;
#endif
#if RTS_ENABLE_ASSERTIONS
    service->validation_magic = RTS_TASK_VALIDATION_MAGIC;
#endif
    kernel->timer_service_task = service;
}

rts_status_t rts_init(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_port_stack_result_t stack_result;
    rts_critical_token_t critical_token;
    rts_status_t port_status;
    bool in_isr;

    in_isr = rts_port_is_in_isr();
    if (in_isr)
    {
        RTS_ASSERT(!in_isr);
        return RTS_STATUS_INVALID_CONTEXT;
    }

    if (kernel->lifecycle == RTS_KERNEL_INITIALIZED)
    {
        return RTS_STATUS_ALREADY_INITIALIZED;
    }
    if (kernel->lifecycle == RTS_KERNEL_RUNNING)
    {
        return RTS_STATUS_ALREADY_STARTED;
    }
    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_RESET);
    if (kernel->lifecycle != RTS_KERNEL_RESET)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    critical_token = rts_port_critical_enter();
    if (kernel->lifecycle != RTS_KERNEL_RESET)
    {
        rts_status_t status = kernel->lifecycle == RTS_KERNEL_RUNNING
                                  ? RTS_STATUS_ALREADY_STARTED
                                  : RTS_STATUS_ALREADY_INITIALIZED;
        rts_port_critical_exit(critical_token);
        return status;
    }

    rts_kernel_restore_reset(kernel);
    rts_task_pool_initialize(&kernel->application_task_pool);
    rts_policy_initialize();
    rts_delay_initialize(&kernel->delay_queue);

    port_status = rts_port_initialize();
    if (port_status != RTS_STATUS_OK)
    {
        rts_kernel_restore_reset(kernel);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    rts_timer_service_object_initialize(kernel);
    rts_stack_diagnostics_prepare(
        kernel->timer_service_task_storage.stack_low,
        kernel->timer_service_task_storage.stack_high);
    stack_result = rts_port_stack_initialize(
        kernel->timer_service_stack, sizeof(kernel->timer_service_stack),
        rts_timer_service_entry, NULL);
    if (stack_result.status != RTS_STATUS_OK ||
        stack_result.saved_stack_pointer == NULL)
    {
        rts_kernel_restore_reset(kernel);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }
    kernel->timer_service_task_storage.saved_stack_pointer =
        stack_result.saved_stack_pointer;

    rts_idle_object_initialize(kernel);
    rts_stack_diagnostics_prepare(kernel->idle_task_storage.stack_low,
                                  kernel->idle_task_storage.stack_high);
    stack_result = rts_port_stack_initialize(kernel->idle_stack,
                                             sizeof(kernel->idle_stack),
                                             rts_idle_entry, NULL);
    if (stack_result.status != RTS_STATUS_OK ||
        stack_result.saved_stack_pointer == NULL)
    {
        rts_kernel_restore_reset(kernel);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    kernel->idle_task_storage.saved_stack_pointer =
        stack_result.saved_stack_pointer;
    if (!rts_policy_insert(&kernel->idle_task_storage))
    {
        rts_kernel_restore_reset(kernel);
        rts_port_critical_exit(critical_token);
        RTS_FATAL_UNLESS(false);
        return RTS_STATUS_PORT_ERROR;
    }
    kernel->idle_task_storage.state = RTS_TASK_STATE_READY;
#if RTS_ENABLE_ASSERTIONS
    kernel->idle_task_storage.validation_magic = RTS_TASK_VALIDATION_MAGIC;
#endif
    kernel->idle_task = &kernel->idle_task_storage;
    kernel->lifecycle = RTS_KERNEL_INITIALIZED;

    rts_port_critical_exit(critical_token);
    return RTS_STATUS_OK;
}
