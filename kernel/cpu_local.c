#include "scheduler_internal.h"

#include "assert_internal.h"

rts_cpu_local_state_t *rts_scheduler_cpu_local(rts_cpu_id_t cpu)
{
    RTS_ASSERT(rts_cpu_id_is_valid(cpu));
    return rts_cpu_id_is_valid(cpu)
               ? &rts_kernel_state_get()->cpu_local
               : NULL;
}

const rts_cpu_local_state_t *rts_scheduler_cpu_local_const(rts_cpu_id_t cpu)
{
    RTS_ASSERT(rts_cpu_id_is_valid(cpu));
    return rts_cpu_id_is_valid(cpu)
               ? &rts_kernel_state_get()->cpu_local
               : NULL;
}

rts_tcb_t *rts_scheduler_current_on_cpu(rts_cpu_id_t cpu)
{
    const rts_cpu_local_state_t *local = rts_scheduler_cpu_local_const(cpu);
    return local == NULL ? NULL : local->current_task;
}

rts_tcb_t *rts_scheduler_current_get(void)
{
    return rts_scheduler_current_on_cpu(rts_cpu_current_id());
}

void rts_scheduler_set_current_on_cpu(rts_cpu_id_t cpu, rts_tcb_t *task)
{
    rts_cpu_local_state_t *local = rts_scheduler_cpu_local(cpu);
    if (local != NULL)
    {
        local->current_task = task;
    }
}

rts_switch_plan_t *rts_scheduler_switch_plan_on_cpu(rts_cpu_id_t cpu)
{
    rts_cpu_local_state_t *local = rts_scheduler_cpu_local(cpu);
    return local == NULL ? NULL : &local->switch_plan;
}
