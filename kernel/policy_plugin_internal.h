#ifndef RTS_POLICY_PLUGIN_INTERNAL_H
#define RTS_POLICY_PLUGIN_INTERNAL_H

#include "scheduler_policy.h"

void rts_policy_fp_initialize(void);
bool rts_policy_fp_insert(rts_tcb_t *task);
bool rts_policy_fp_remove(rts_tcb_t *task);
rts_tcb_t *rts_policy_fp_pick_next(void);
bool rts_policy_fp_yield(rts_tcb_t *task);
bool rts_policy_fp_priority_changed(rts_tcb_t *task,
                                    rts_priority_t effective_priority);
bool rts_policy_fp_tick(rts_tick_t elapsed_ticks);
bool rts_policy_fp_validate(const rts_tcb_t *task, bool expected_ready);

void rts_policy_rms_initialize(void);
bool rts_policy_rms_insert(rts_tcb_t *task);
bool rts_policy_rms_remove(rts_tcb_t *task);
rts_tcb_t *rts_policy_rms_pick_next(void);
bool rts_policy_rms_yield(rts_tcb_t *task);
bool rts_policy_rms_priority_changed(rts_tcb_t *task,
                                     rts_priority_t effective_priority);
bool rts_policy_rms_tick(rts_tick_t elapsed_ticks);
bool rts_policy_rms_validate(const rts_tcb_t *task, bool expected_ready);

void rts_policy_edf_initialize(void);
bool rts_policy_edf_insert(rts_tcb_t *task);
bool rts_policy_edf_remove(rts_tcb_t *task);
rts_tcb_t *rts_policy_edf_pick_next(void);
bool rts_policy_edf_yield(rts_tcb_t *task);
bool rts_policy_edf_priority_changed(rts_tcb_t *task,
                                     rts_priority_t effective_priority);
bool rts_policy_edf_tick(rts_tick_t elapsed_ticks);
bool rts_policy_edf_validate(const rts_tcb_t *task, bool expected_ready);

#endif /* RTS_POLICY_PLUGIN_INTERNAL_H */
