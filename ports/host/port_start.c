#include "port.h"

#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "scheduler_internal.h"

static size_t rts_host_start_request_count;
static rts_task_handle_t rts_host_start_task;
static void *rts_host_start_saved_stack_pointer;
static bool rts_host_start_consumed;
static bool rts_host_fail_next_start;

rts_status_t rts_port_start_first_task(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *task = kernel->current_task;

    if (rts_host_start_request_count != 0u ||
        kernel->lifecycle != RTS_KERNEL_RUNNING || task == NULL ||
        task->state != RTS_TASK_STATE_RUNNING ||
        task->saved_stack_pointer == NULL ||
        ((uintptr_t)task->saved_stack_pointer %
         (uintptr_t)RTS_TASK_STACK_ALIGNMENT) != 0u ||
        !rts_scheduler_current_is_valid() || kernel->switch_plan.pending ||
        kernel->switch_plan.active)
    {
        return RTS_STATUS_INVALID_STATE;
    }
    if (rts_host_fail_next_start)
    {
        rts_host_fail_next_start = false;
        return RTS_STATUS_PORT_ERROR;
    }

    rts_host_start_request_count = 1u;
    rts_host_start_task = task;
    rts_host_start_saved_stack_pointer = task->saved_stack_pointer;
    rts_host_start_consumed = true;
    return RTS_STATUS_OK;
}

void rts_host_port_test_start_reset(void)
{
    rts_host_start_request_count = 0u;
    rts_host_start_task = NULL;
    rts_host_start_saved_stack_pointer = NULL;
    rts_host_start_consumed = false;
    rts_host_fail_next_start = false;
}

size_t rts_host_port_test_start_request_count(void)
{
    return rts_host_start_request_count;
}

rts_task_handle_t rts_host_port_test_start_task(void)
{
    return rts_host_start_task;
}

bool rts_host_port_test_start_consumed(void)
{
    return rts_host_start_consumed;
}

void *rts_host_port_test_start_saved_stack_pointer(void)
{
    return rts_host_start_saved_stack_pointer;
}

void rts_host_port_test_fail_next_start(bool fail)
{
    rts_host_fail_next_start = fail;
}
