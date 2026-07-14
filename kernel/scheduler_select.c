#include "scheduler_internal.h"

#include "assert_internal.h"

bool rts_scheduler_task_is_idle(const rts_tcb_t *task)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    return task != NULL && task == kernel->idle_task &&
           task == &kernel->idle_task_storage;
}

static bool rts_scheduler_task_is_runnable_in(
    const rts_kernel_state_t *kernel,
    const rts_tcb_t *task)
{
    bool identity_is_valid;
    bool state_is_valid;

    if (task == NULL || task->saved_stack_pointer == NULL ||
        task->stack_low == NULL || task->stack_high == NULL ||
        (uintptr_t)task->stack_low >= (uintptr_t)task->stack_high ||
        task->entry == NULL || task->wait.reason != RTS_WAIT_NONE ||
        task->delay_node.owner != NULL ||
        !rts_ready_contains(&kernel->ready_set, task))
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
            rts_task_handle_is_application_task((rts_task_handle_t)task);
    }

    state_is_valid = task->state == RTS_TASK_STATE_READY ||
                     (task == kernel->current_task &&
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

    selected = rts_ready_peek_highest(&kernel->ready_set);
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

rts_tcb_t *rts_scheduler_current_get(void)
{
    return rts_kernel_state_get()->current_task;
}

bool rts_scheduler_current_is_valid(void)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (kernel->current_task == NULL)
    {
        return kernel->lifecycle == RTS_KERNEL_RESET ||
               kernel->lifecycle == RTS_KERNEL_INITIALIZED;
    }

    return (kernel->lifecycle == RTS_KERNEL_INITIALIZED ||
            kernel->lifecycle == RTS_KERNEL_RUNNING) &&
           kernel->current_task->state == RTS_TASK_STATE_RUNNING &&
           rts_scheduler_task_is_runnable_in(kernel, kernel->current_task);
}

bool rts_scheduler_current_establish(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *selected;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    RTS_ASSERT(kernel->current_task == NULL);
    RTS_ASSERT(task != NULL);
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED ||
        kernel->current_task != NULL || task == NULL)
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
    kernel->current_task = task;
    RTS_FATAL_UNLESS(rts_scheduler_current_is_valid());
    return rts_scheduler_current_is_valid();
}

bool rts_scheduler_current_release_initial(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *current = kernel->current_task;

    RTS_ASSERT(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    RTS_ASSERT(current != NULL);
    RTS_ASSERT(current == NULL || current->state == RTS_TASK_STATE_RUNNING);
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED || current == NULL ||
        current->state != RTS_TASK_STATE_RUNNING)
    {
        return false;
    }

    current->state = RTS_TASK_STATE_READY;
    kernel->current_task = NULL;
    RTS_FATAL_UNLESS(rts_ready_contains(&kernel->ready_set, current));
    return rts_ready_contains(&kernel->ready_set, current) &&
           rts_scheduler_current_is_valid();
}
