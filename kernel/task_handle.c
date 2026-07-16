#include "task_internal.h"

#include "assert_internal.h"
#include "scheduler_internal.h"
#include "scheduler_policy.h"

bool rts_task_handle_is_application_task(rts_task_handle_t handle)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t slot_index;

    if (handle == NULL)
    {
        return false;
    }

    for (slot_index = 0u; slot_index < (size_t)RTS_MAX_TASKS; ++slot_index)
    {
        if (handle == &kernel->application_task_pool.slots[slot_index])
        {
            return true;
        }
    }

    return false;
}

bool rts_task_object_is_valid(const rts_tcb_t *task)
{
    bool valid;

    if (!rts_task_handle_is_application_task((rts_task_handle_t)task))
    {
        return false;
    }

    valid = task->slot_state == RTS_TASK_SLOT_ALLOCATED &&
            task->state == RTS_TASK_STATE_READY &&
            task->saved_stack_pointer != NULL && task->stack_low != NULL &&
            task->stack_high != NULL &&
            (uintptr_t)task->stack_low < (uintptr_t)task->stack_high &&
            task->entry != NULL && task->priority > RTS_IDLE_PRIORITY &&
            task->base_priority > RTS_IDLE_PRIORITY &&
            task->priority >= task->base_priority &&
            (size_t)task->priority < (size_t)RTS_PRIORITY_COUNT &&
            task->wait.reason == RTS_WAIT_NONE &&
            task->wait.object == NULL && !task->wait.timeout_active &&
            task->wait_node.owner == NULL && task->delay_node.owner == NULL;
#if RTS_ENABLE_ASSERTIONS
    valid = valid && task->validation_magic == RTS_TASK_VALIDATION_MAGIC;
#endif
    if (valid)
    {
        valid = rts_policy_validate(task, true);
    }
    return valid;
}
