#ifndef RTS_SCHEDULER_POLICY_H
#define RTS_SCHEDULER_POLICY_H

#include <stdbool.h>

#include "task_internal.h"

typedef uint8_t rts_policy_kind_t;
enum
{
    RTS_POLICY_KIND_FIXED_PRIORITY = 1,
    RTS_POLICY_KIND_RMS,
    RTS_POLICY_KIND_EDF
};

void rts_policy_initialize(void);
bool rts_policy_insert(rts_tcb_t *task);
bool rts_policy_remove(rts_tcb_t *task);
rts_tcb_t *rts_policy_pick_next(void);
bool rts_policy_yield(rts_tcb_t *task);
bool rts_policy_task_block(rts_tcb_t *task);
bool rts_policy_task_unblock(rts_tcb_t *task);
bool rts_policy_priority_changed(rts_tcb_t *task,
                                 rts_priority_t effective_priority);
bool rts_policy_tick(rts_tick_t elapsed_ticks);
bool rts_policy_validate(const rts_tcb_t *task, bool expected_ready);
rts_policy_kind_t rts_policy_kind(void);

#endif /* RTS_SCHEDULER_POLICY_H */
