#include "scheduler_internal.h"

#include "assert_internal.h"
#include "rts/rts_mutex.h"
#include "rts/rts_semaphore.h"
#include "scheduler_policy.h"

bool rts_scheduler_task_is_idle(const rts_tcb_t *task)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    return task != NULL && task == kernel->idle_task &&
           task == &kernel->idle_task_storage;
}

bool rts_scheduler_task_is_timer_service(const rts_tcb_t *task)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    return task != NULL && task == kernel->timer_service_task &&
           task == &kernel->timer_service_task_storage;
}

static bool rts_scheduler_task_is_runnable_in(
    const rts_kernel_state_t *kernel,
    const rts_tcb_t *task)
{
    (void)kernel;
    bool identity_is_valid;
    bool state_is_valid;

    if (task == NULL || task->saved_stack_pointer == NULL ||
        task->stack_low == NULL || task->stack_high == NULL ||
        (uintptr_t)task->stack_low >= (uintptr_t)task->stack_high ||
        task->entry == NULL || task->wait.reason != RTS_WAIT_NONE ||
        task->wait.object != NULL || task->wait.timeout_active ||
        task->wait_node.owner != NULL ||
        task->delay_node.owner != NULL ||
        !rts_policy_validate(task, true))
    {
        return false;
    }

    if (task->priority < task->base_priority ||
        (size_t)task->base_priority >= (size_t)RTS_PRIORITY_COUNT)
    {
        return false;
    }

    if (task->priority == RTS_IDLE_PRIORITY)
    {
        identity_is_valid = rts_scheduler_task_is_idle(task);
    }
    else
    {
        identity_is_valid =
            (size_t)task->priority < (size_t)RTS_PRIORITY_COUNT &&
            (rts_task_handle_is_application_task((rts_task_handle_t)task) ||
             rts_scheduler_task_is_timer_service(task));
    }

    state_is_valid = task->state == RTS_TASK_STATE_READY ||
                     (task == rts_scheduler_current_get() &&
                      task->state == RTS_TASK_STATE_RUNNING);

    if (!identity_is_valid || !state_is_valid ||
        task->slot_state != RTS_TASK_SLOT_ALLOCATED)
    {
        return false;
    }

#if RTS_ENABLE_ASSERTIONS
    if (task->validation_magic != RTS_TASK_VALIDATION_MAGIC)
    {
        return false;
    }
#endif
    return true;
}

bool rts_scheduler_task_is_runnable(const rts_tcb_t *task)
{
    return rts_scheduler_task_is_runnable_in(rts_kernel_state_get(), task);
}

bool rts_scheduler_task_is_blocked_delay(const rts_tcb_t *task)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    bool valid;

    if (task == NULL || task == kernel->idle_task ||
        !rts_task_handle_is_application_task((rts_task_handle_t)task))
    {
        return false;
    }

    valid = task->slot_state == RTS_TASK_SLOT_ALLOCATED &&
            task->state == RTS_TASK_STATE_BLOCKED &&
            task->wait.reason == RTS_WAIT_DELAY &&
            task->wait.object == NULL && !task->wait.timeout_active &&
            task->wait_node.owner == NULL &&
            task->saved_stack_pointer != NULL && task->stack_low != NULL &&
            task->stack_high != NULL &&
            (uintptr_t)task->stack_low < (uintptr_t)task->stack_high &&
            task->entry != NULL &&
            task->priority > RTS_IDLE_PRIORITY &&
            (size_t)task->priority < (size_t)RTS_PRIORITY_COUNT &&
            rts_policy_validate(task, false) &&
            task->ready_node.previous == NULL &&
            task->ready_node.next == NULL &&
            task->ready_node.object == NULL &&
            rts_delay_contains(&kernel->delay_queue, task);
#if RTS_ENABLE_ASSERTIONS
    valid = valid && task->validation_magic == RTS_TASK_VALIDATION_MAGIC;
#endif
    return valid;
}

bool rts_scheduler_task_is_blocked_wait(const rts_tcb_t *task)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    bool valid;

    if (task == NULL || task == kernel->idle_task ||
        (!rts_task_handle_is_application_task((rts_task_handle_t)task) &&
         !rts_scheduler_task_is_timer_service(task)))
    {
        return false;
    }

    if (rts_scheduler_task_is_timer_service(task))
    {
        valid = task->slot_state == RTS_TASK_SLOT_ALLOCATED &&
                task->state == RTS_TASK_STATE_BLOCKED &&
                task->wait.reason == RTS_WAIT_TIMER_SERVICE &&
                task->wait.result == RTS_WAIT_RESULT_NONE &&
                task->wait.object == NULL && !task->wait.timeout_active &&
                task->wait_node.owner == NULL &&
                task->delay_node.owner == NULL &&
                rts_policy_validate(task, false);
#if RTS_ENABLE_ASSERTIONS
        valid = valid &&
                task->validation_magic == RTS_TASK_VALIDATION_MAGIC;
#endif
        return valid;
    }

    valid = task->slot_state == RTS_TASK_SLOT_ALLOCATED &&
            task->state == RTS_TASK_STATE_BLOCKED &&
            (task->wait.reason == RTS_WAIT_SEMAPHORE ||
             task->wait.reason == RTS_WAIT_MUTEX) &&
            task->wait.result == RTS_WAIT_RESULT_NONE &&
            task->wait.object != NULL && task->wait_node.owner != NULL &&
            task->saved_stack_pointer != NULL && task->stack_low != NULL &&
            task->stack_high != NULL &&
            (uintptr_t)task->stack_low < (uintptr_t)task->stack_high &&
            task->entry != NULL && task->priority > RTS_IDLE_PRIORITY &&
            (size_t)task->priority < (size_t)RTS_PRIORITY_COUNT &&
            rts_policy_validate(task, false) &&
            (task->wait.timeout_active ==
             rts_delay_contains(&kernel->delay_queue, task));
    if (valid && task->wait.reason == RTS_WAIT_SEMAPHORE)
    {
        valid = task->wait_node.owner ==
                &((const rts_semaphore_t *)task->wait.object)->waiters;
    }
    else if (valid)
    {
        valid = task->wait_node.owner ==
                &((const rts_mutex_t *)task->wait.object)->waiters;
    }
#if RTS_ENABLE_ASSERTIONS
    valid = valid && task->validation_magic == RTS_TASK_VALIDATION_MAGIC;
#endif
    return valid;
}

rts_tcb_t *rts_scheduler_select_highest_ready(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *selected;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED ||
               kernel->lifecycle == RTS_KERNEL_RUNNING);
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED &&
        kernel->lifecycle != RTS_KERNEL_RUNNING)
    {
        return NULL;
    }

    selected = rts_policy_pick_next();
    RTS_FATAL_UNLESS(selected != NULL);
    if (selected == NULL)
    {
        return NULL;
    }

    RTS_FATAL_UNLESS(rts_scheduler_task_is_runnable_in(kernel, selected));
    if (!rts_scheduler_task_is_runnable_in(kernel, selected))
    {
        return NULL;
    }
    return selected;
}

bool rts_scheduler_current_is_valid(void)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (rts_scheduler_current_get() == NULL)
    {
        return kernel->lifecycle == RTS_KERNEL_RESET ||
               kernel->lifecycle == RTS_KERNEL_INITIALIZED;
    }

    return (kernel->lifecycle == RTS_KERNEL_INITIALIZED ||
            kernel->lifecycle == RTS_KERNEL_RUNNING) &&
           rts_scheduler_current_get()->state == RTS_TASK_STATE_RUNNING &&
           rts_scheduler_task_is_runnable_in(kernel, rts_scheduler_current_get());
}

bool rts_scheduler_current_establish(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *selected;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    RTS_ASSERT(rts_scheduler_current_get() == NULL);
    RTS_ASSERT(task != NULL);
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED ||
        rts_scheduler_current_get() != NULL || task == NULL)
    {
        return false;
    }

    selected = rts_scheduler_select_highest_ready();
    RTS_ASSERT(selected == task);
    RTS_ASSERT(task->state == RTS_TASK_STATE_READY);
    if (selected != task || task->state != RTS_TASK_STATE_READY)
    {
        return false;
    }

    task->state = RTS_TASK_STATE_RUNNING;
    task->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    rts_scheduler_set_current_on_cpu(rts_cpu_current_id(), task);
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
    return rts_scheduler_current_is_valid();
}

bool rts_scheduler_current_release_initial(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *current = rts_scheduler_current_get();

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    RTS_ASSERT(current != NULL);
    RTS_ASSERT(current == NULL || current->state == RTS_TASK_STATE_RUNNING);
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED || current == NULL ||
        current->state != RTS_TASK_STATE_RUNNING)
    {
        return false;
    }

    current->state = RTS_TASK_STATE_READY;
    rts_scheduler_set_current_on_cpu(rts_cpu_current_id(), NULL);
    RTS_FATAL_UNLESS(rts_policy_validate(current, true));
    return rts_policy_validate(current, true) &&
           rts_scheduler_current_is_valid();
}

bool rts_scheduler_timer_service_wake(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *service = kernel->timer_service_task;

    if (service == NULL || service->state != RTS_TASK_STATE_BLOCKED ||
        service->wait.reason != RTS_WAIT_TIMER_SERVICE ||
        service->wait.object != NULL || service->wait.timeout_active ||
        rts_policy_validate(service, true))
    {
        return false;
    }
    service->wait.reason = RTS_WAIT_NONE;
    service->wait.result = RTS_WAIT_RESULT_NONE;
    service->state = RTS_TASK_STATE_READY;
    service->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    RTS_FATAL_UNLESS(rts_policy_task_unblock(service));
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(service->diagnostic_wake_count);
#endif
    RTS_FATAL_UNLESS(rts_scheduler_task_is_runnable(service));
    return true;
}

bool rts_scheduler_timer_service_block(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *service = kernel->timer_service_task;
    rts_tcb_t *selected;

    if (service == NULL || rts_scheduler_current_get() != service ||
        service->state != RTS_TASK_STATE_RUNNING ||
        !rts_policy_validate(service, true) ||
        service->wait.reason != RTS_WAIT_NONE)
    {
        return false;
    }
    RTS_FATAL_UNLESS(rts_policy_task_block(service));
    service->wait.reason = RTS_WAIT_TIMER_SERVICE;
    service->wait.result = RTS_WAIT_RESULT_NONE;
    service->wait.wake_tick = 0u;
    service->wait.object = NULL;
    service->wait.timeout_active = false;
    service->state = RTS_TASK_STATE_BLOCKED;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(service->diagnostic_block_count);
#endif
    selected = rts_scheduler_select_highest_ready();
    RTS_FATAL_UNLESS(selected != NULL && selected != service);
    return selected != NULL && selected != service &&
           rts_scheduler_prepare_switch(selected);
}

void rts_scheduler_timer_service_cancel_wake(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *service = kernel->timer_service_task;

    if (service == NULL || service->state != RTS_TASK_STATE_READY)
    {
        return;
    }
    if (kernel->switch_plan.pending && kernel->switch_plan.to == service)
    {
        (void)rts_scheduler_prepare_switch(rts_scheduler_current_get());
    }
    RTS_FATAL_UNLESS(rts_policy_task_block(service));
    service->wait.reason = RTS_WAIT_TIMER_SERVICE;
    service->wait.result = RTS_WAIT_RESULT_NONE;
    service->wait.wake_tick = 0u;
    service->wait.object = NULL;
    service->wait.timeout_active = false;
    service->state = RTS_TASK_STATE_BLOCKED;
}
