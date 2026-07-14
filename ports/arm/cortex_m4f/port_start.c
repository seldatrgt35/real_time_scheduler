#include "port.h"

#include <stdbool.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port_internal.h"

static rts_cm4f_start_handoff_t rts_cm4f_start_handoff;
static bool rts_cm4f_start_attempted;

static bool rts_cm4f_start_task_is_valid(const rts_kernel_state_t *kernel,
                                         const rts_tcb_t *task)
{
    return kernel->lifecycle == RTS_KERNEL_RUNNING && task != NULL &&
           task == kernel->current_task &&
           task->state == RTS_TASK_STATE_RUNNING &&
           task->saved_stack_pointer != NULL &&
           ((uintptr_t)task->saved_stack_pointer %
            (uintptr_t)RTS_TASK_STACK_ALIGNMENT) == 0u &&
           rts_scheduler_current_is_valid() &&
           !kernel->switch_plan.pending && !kernel->switch_plan.active;
}

const rts_cm4f_start_handoff_t *rts_cm4f_start_handoff_get(void)
{
    return &rts_cm4f_start_handoff;
}

void *rts_cm4f_start_handoff_consume(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *task = rts_cm4f_start_handoff.first_task;
    void *saved_stack_pointer = rts_cm4f_start_handoff.saved_stack_pointer;
    bool valid = rts_cm4f_start_handoff.valid == UINT32_C(1) &&
                 rts_cm4f_start_handoff.cookie ==
                     RTS_CM4F_START_HANDOFF_COOKIE &&
                 rts_cm4f_start_task_is_valid(kernel, task) &&
                 saved_stack_pointer == task->saved_stack_pointer;

    RTS_ASSERT(valid);
    if (!valid)
    {
        return NULL;
    }

    rts_cm4f_start_handoff.valid = UINT32_C(0);
    return saved_stack_pointer;
}

rts_status_t rts_cm4f_start_handoff_prepare(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *task = kernel->current_task;

    if (rts_cm4f_start_attempted ||
        !rts_cm4f_start_task_is_valid(kernel, task))
    {
        return RTS_STATUS_INVALID_STATE;
    }

    rts_cm4f_start_handoff.first_task = task;
    rts_cm4f_start_handoff.saved_stack_pointer =
        task->saved_stack_pointer;
    rts_cm4f_start_handoff.cookie = RTS_CM4F_START_HANDOFF_COOKIE;
    rts_cm4f_start_handoff.valid = UINT32_C(1);
    rts_cm4f_start_attempted = true;

    return RTS_STATUS_OK;
}

rts_status_t rts_port_start_first_task(void)
{
    rts_status_t status = rts_cm4f_start_handoff_prepare();

    if (status != RTS_STATUS_OK)
    {
        return status;
    }
    rts_cm4f_start_trigger();
}

_Noreturn void rts_cm4f_start_fatal(void)
{
    RTS_FATAL_UNLESS(false);
    for (;;)
    {
    }
}
