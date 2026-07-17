#include "rts/rts_task.h"

#include "assert_internal.h"
#include "port.h"
#include "kernel_lock.h"
#include "scheduler_internal.h"
#include "task_internal.h"
#include "stack_check_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"
#include "scheduler_policy.h"

static void rts_task_create_rollback(rts_kernel_state_t *kernel,
                                     rts_tcb_t *task,
                                     size_t slot_index)
{
    if (rts_policy_validate(task, true))
    {
        (void)rts_policy_remove(task);
    }
    rts_task_object_reset(task);
    rts_task_pool_rollback(&kernel->application_task_pool, slot_index);
}

rts_status_t rts_task_create(const rts_task_config_t *config,
                             rts_task_handle_t *out_handle)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_port_stack_result_t stack_result;
    rts_kernel_lock_token_t critical_token;
    rts_status_t status;
    rts_tcb_t *task;
    size_t slot_index;
    bool in_isr;

    if (out_handle == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }
    *out_handle = NULL;

    in_isr = rts_port_is_in_isr();
    if (in_isr)
    {
        RTS_ASSERT(!in_isr);
        return RTS_STATUS_INVALID_CONTEXT;
    }

    status = rts_task_config_validate(config, kernel->lifecycle);
    if (status != RTS_STATUS_OK)
    {
        return status;
    }

    critical_token = rts_kernel_lock_enter();
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    if (!rts_task_pool_reserve(&kernel->application_task_pool, &slot_index))
    {
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_CAPACITY_EXHAUSTED;
    }

    task = rts_task_pool_get(&kernel->application_task_pool, slot_index);
    status = rts_task_object_initialize(&kernel->application_task_pool, task,
                                        config, kernel->lifecycle);
    if (status != RTS_STATUS_OK)
    {
        rts_task_create_rollback(kernel, task, slot_index);
        rts_kernel_lock_exit(critical_token);
        return status;
    }

    rts_stack_diagnostics_prepare(task->stack_low, task->stack_high);
    stack_result = rts_port_stack_initialize(config->stack_buffer,
                                             config->stack_size_bytes,
                                             config->entry,
                                             config->argument);
    if (stack_result.status != RTS_STATUS_OK ||
        stack_result.saved_stack_pointer == NULL)
    {
        status = stack_result.status != RTS_STATUS_OK
                     ? stack_result.status
                     : RTS_STATUS_PORT_ERROR;
        rts_task_create_rollback(kernel, task, slot_index);
        rts_kernel_lock_exit(critical_token);
        return status;
    }
    task->saved_stack_pointer = stack_result.saved_stack_pointer;

    if (!rts_policy_insert(task))
    {
        RTS_ASSERT(rts_policy_validate(task, true));
        rts_task_create_rollback(kernel, task, slot_index);
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    task->state = RTS_TASK_STATE_READY;
    rts_task_pool_commit(&kernel->application_task_pool, slot_index);
    RTS_ASSERT(task->slot_state == RTS_TASK_SLOT_ALLOCATED);
    if (task->slot_state != RTS_TASK_SLOT_ALLOCATED)
    {
        (void)rts_policy_remove(task);
        rts_task_create_rollback(kernel, task, slot_index);
        rts_kernel_lock_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

#if RTS_ENABLE_ASSERTIONS
    task->validation_magic = RTS_TASK_VALIDATION_MAGIC;
#endif
    *out_handle = task;
#if RTS_ENABLE_RUNTIME_STATS
    RTS_DIAG_COUNTER_INC(kernel->runtime_counters.task_creations);
#endif
    RTS_TRACE(RTS_TRACE_TASK_CREATED, task->priority, slot_index);
    rts_kernel_lock_exit(critical_token);
    return RTS_STATUS_OK;
}
