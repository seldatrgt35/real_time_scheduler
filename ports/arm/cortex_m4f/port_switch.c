#include "port_switch.h"

#include <stdbool.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port.h"
#include "port_offsets.h"

static rts_cm4f_switch_handoff_t rts_cm4f_switch_handoff;

#if !defined(RTS_CM4F_TEST_EXTERNAL_RESCHEDULE_REQUEST)
void rts_port_request_reschedule(rts_cpu_id_t cpu)
{
    RTS_ASSERT(rts_cpu_id_is_valid(cpu));
    if (rts_cpu_id_is_valid(cpu))
    {
        rts_cm4f_pend_context_switch();
    }
}
#endif

static bool rts_cm4f_saved_sp_is_valid(const rts_tcb_t *task,
                                       const void *saved_stack_pointer)
{
    uintptr_t low;
    uintptr_t high;
    uintptr_t saved;

    if (task == NULL || saved_stack_pointer == NULL ||
        task->stack_low == NULL || task->stack_high == NULL)
    {
        return false;
    }

    low = (uintptr_t)task->stack_low;
    high = (uintptr_t)task->stack_high;
    saved = (uintptr_t)saved_stack_pointer;
    return low < high && saved >= low && saved <= high &&
           /* The public stack region and initial frame are 16-byte aligned.
            * Once a task has executed, the AAPCS/Cortex-M contract only
            * requires the live PSP (and therefore the PendSV saved SP) to be
            * 8-byte aligned.  A task prologue may legally move PSP to an
            * address that is 8-byte, but not 16-byte, aligned. */
           (saved % (uintptr_t)RTS_CM4F_ARCH_STACK_ALIGNMENT) == 0u &&
           (high - saved) >= RTS_CM4F_INITIAL_FRAME_SIZE_BYTES;
}

const rts_cm4f_switch_handoff_t *rts_cm4f_switch_bridge_acquire(
    void *outgoing_saved_stack_pointer)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_switch_snapshot_t snapshot;
    bool valid;

    /* A stale/duplicate hardware pending bit is an approved harmless no-op. */
    if (!kernel->switch_plan.pending)
    {
        return NULL;
    }

    if (!rts_scheduler_switch_acquire(&snapshot))
    {
        RTS_FATAL_UNLESS(false);
        return NULL;
    }

    valid = snapshot.from == rts_scheduler_current_get() &&
            snapshot.from != NULL && snapshot.to != NULL &&
            snapshot.from != snapshot.to &&
            (snapshot.from->state == RTS_TASK_STATE_RUNNING ||
             snapshot.from->state == RTS_TASK_STATE_BLOCKED) &&
            snapshot.to->state == RTS_TASK_STATE_READY &&
            rts_cm4f_saved_sp_is_valid(snapshot.from,
                                        outgoing_saved_stack_pointer) &&
            rts_cm4f_saved_sp_is_valid(snapshot.to,
                                        snapshot.to->saved_stack_pointer);
    RTS_FATAL_UNLESS(valid);
    if (!valid)
    {
        return NULL;
    }

    rts_cm4f_switch_handoff.from = snapshot.from;
    rts_cm4f_switch_handoff.to = snapshot.to;
    rts_cm4f_switch_handoff.snapshot = snapshot;
    rts_cm4f_switch_handoff.outgoing_saved_stack_pointer =
        outgoing_saved_stack_pointer;
    rts_cm4f_switch_handoff.incoming_saved_stack_pointer =
        snapshot.to->saved_stack_pointer;
    return &rts_cm4f_switch_handoff;
}

bool rts_cm4f_switch_bridge_no_plan(void)
{
    const rts_switch_plan_t *plan = &rts_kernel_state_get()->switch_plan;

    return !plan->pending && !plan->active;
}

bool rts_cm4f_switch_bridge_complete(
    const rts_cm4f_switch_handoff_t *handoff)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    bool notify_port;
    bool valid = handoff == &rts_cm4f_switch_handoff &&
                 kernel->switch_plan.active &&
                 !kernel->switch_plan.pending &&
                 handoff->from == handoff->snapshot.from &&
                 handoff->to == handoff->snapshot.to &&
                 handoff->from == rts_scheduler_current_get() &&
                 handoff->from->saved_stack_pointer ==
                     handoff->outgoing_saved_stack_pointer &&
                 handoff->to->saved_stack_pointer ==
                     handoff->incoming_saved_stack_pointer;

    RTS_FATAL_UNLESS(valid);
    if (!valid)
    {
        return false;
    }

    rts_scheduler_switch_complete(&handoff->snapshot);
    notify_port = rts_scheduler_reselect_after_switch();
    if (notify_port)
    {
        rts_port_request_reschedule(rts_cpu_current_id());
    }
    valid = !kernel->switch_plan.active &&
            rts_scheduler_current_get() == handoff->to &&
            (handoff->from->state == RTS_TASK_STATE_READY ||
             handoff->from->state == RTS_TASK_STATE_BLOCKED) &&
            handoff->to->state == RTS_TASK_STATE_RUNNING;
    RTS_FATAL_UNLESS(valid);
    return valid;
}
