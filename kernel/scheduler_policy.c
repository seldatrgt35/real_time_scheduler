#include "scheduler_policy.h"

#include <stdint.h>

#include "assert_internal.h"
#include "policy_plugin_internal.h"
#include "scheduler_internal.h"

static void rts_policy_record_release(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    ++kernel->policy_release_sequence;
    if (kernel->policy_release_sequence == 0u)
    {
        ++kernel->policy_release_sequence;
    }
    task->release_tick = kernel->current_tick;
    task->absolute_deadline = task->relative_deadline == 0u
                                  ? kernel->current_tick
                                  : kernel->current_tick +
                                        task->relative_deadline;
    task->release_sequence = kernel->policy_release_sequence;
}

void rts_policy_initialize(void)
{
    rts_kernel_state_get()->policy_release_sequence = 0u;
#if RTS_POLICY_FIXED_PRIORITY
    rts_policy_fp_initialize();
#elif RTS_POLICY_RMS
    rts_policy_rms_initialize();
#else
    rts_policy_edf_initialize();
#endif
}

bool rts_policy_insert(rts_tcb_t *task)
{
    if (task == NULL)
    {
        return false;
    }
    rts_policy_record_release(task);
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_insert(task);
#elif RTS_POLICY_RMS
    return rts_policy_rms_insert(task);
#else
    return rts_policy_edf_insert(task);
#endif
}

bool rts_policy_remove(rts_tcb_t *task)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_remove(task);
#elif RTS_POLICY_RMS
    return rts_policy_rms_remove(task);
#else
    return rts_policy_edf_remove(task);
#endif
}

rts_tcb_t *rts_policy_pick_next(void)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_pick_next();
#elif RTS_POLICY_RMS
    return rts_policy_rms_pick_next();
#else
    return rts_policy_edf_pick_next();
#endif
}

bool rts_policy_yield(rts_tcb_t *task)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_yield(task);
#elif RTS_POLICY_RMS
    return rts_policy_rms_yield(task);
#else
    return rts_policy_edf_yield(task);
#endif
}

bool rts_policy_task_block(rts_tcb_t *task)
{
    return rts_policy_remove(task);
}

bool rts_policy_task_unblock(rts_tcb_t *task)
{
    if (task == NULL)
    {
        return false;
    }
    rts_policy_record_release(task);
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_insert(task);
#elif RTS_POLICY_RMS
    return rts_policy_rms_insert(task);
#else
    return rts_policy_edf_insert(task);
#endif
}

bool rts_policy_priority_changed(rts_tcb_t *task,
                                 rts_priority_t effective_priority)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_priority_changed(task, effective_priority);
#elif RTS_POLICY_RMS
    return rts_policy_rms_priority_changed(task, effective_priority);
#else
    return rts_policy_edf_priority_changed(task, effective_priority);
#endif
}

bool rts_policy_tick(rts_tick_t elapsed_ticks)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_tick(elapsed_ticks);
#elif RTS_POLICY_RMS
    return rts_policy_rms_tick(elapsed_ticks);
#else
    return rts_policy_edf_tick(elapsed_ticks);
#endif
}

bool rts_policy_validate(const rts_tcb_t *task, bool expected_ready)
{
#if RTS_POLICY_FIXED_PRIORITY
    return rts_policy_fp_validate(task, expected_ready);
#elif RTS_POLICY_RMS
    return rts_policy_rms_validate(task, expected_ready);
#else
    return rts_policy_edf_validate(task, expected_ready);
#endif
}

rts_policy_kind_t rts_policy_kind(void)
{
#if RTS_POLICY_FIXED_PRIORITY
    return RTS_POLICY_KIND_FIXED_PRIORITY;
#elif RTS_POLICY_RMS
    return RTS_POLICY_KIND_RMS;
#else
    return RTS_POLICY_KIND_EDF;
#endif
}
