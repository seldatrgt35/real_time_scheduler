#include "rts/rts_task.h"

#include "assert_internal.h"
#include "port.h"
#include "scheduler_internal.h"
#include "task_internal.h"
#include "stack_check_internal.h"
#include "diagnostics_internal.h"
#include "trace_internal.h"

static void rts_task_create_rollback(rts_kernel_state_t *kernel,
                                     rts_tcb_t *task,
                                     size_t slot_index)
{
    if (rts_ready_contains(&kernel->ready_set, task))
    {
        rts_ready_remove(&kernel->ready_set, task);
    }
    rts_task_object_reset(task);
    rts_task_pool_rollback(&kernel->application_task_pool, slot_index);
}

rts_status_t rts_task_create(const rts_task_config_t *config,
                             rts_task_handle_t *out_handle)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_port_stack_result_t stack_result;
    rts_critical_token_t critical_token;
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

    critical_token = rts_port_critical_enter();
    if (kernel->lifecycle != RTS_KERNEL_INITIALIZED)
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_INVALID_STATE;
    }

    if (!rts_task_pool_reserve(&kernel->application_task_pool, &slot_index))
    {
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_CAPACITY_EXHAUSTED;
    }

    task = rts_task_pool_get(&kernel->application_task_pool, slot_index);
    status = rts_task_object_initialize(&kernel->application_task_pool, task,
                                        config, kernel->lifecycle);
    if (status != RTS_STATUS_OK)
    {
        rts_task_create_rollback(kernel, task, slot_index);
        rts_port_critical_exit(critical_token);
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
        rts_port_critical_exit(critical_token);
        return status;
    }
    task->saved_stack_pointer = stack_result.saved_stack_pointer;

    rts_ready_insert(&kernel->ready_set, task);
    if (!rts_ready_contains(&kernel->ready_set, task))
    {
        RTS_ASSERT(rts_ready_contains(&kernel->ready_set, task));
        rts_task_create_rollback(kernel, task, slot_index);
        rts_port_critical_exit(critical_token);
        return RTS_STATUS_PORT_ERROR;
    }

    task->state = RTS_TASK_STATE_READY;
    rts_task_pool_commit(&kernel->application_task_pool, slot_index);
    RTS_ASSERT(task->slot_state == RTS_TASK_SLOT_ALLOCATED);
    if (task->slot_state != RTS_TASK_SLOT_ALLOCATED)
    {
        rts_ready_remove(&kernel->ready_set, task);
        rts_task_create_rollback(kernel, task, slot_index);
        rts_port_critical_exit(critical_token);
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
    rts_port_critical_exit(critical_token);
    return RTS_STATUS_OK;
}
