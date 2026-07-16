#ifndef RTS_SCHEDULER_INTERNAL_H
#define RTS_SCHEDULER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "delay_queue.h"
#include "lifecycle_internal.h"
#include "ready_queue.h"

typedef struct
{
    rts_tcb_t *from;
    rts_tcb_t *to;
    bool pending;
    bool active;
    bool reselection_required;
    uint32_t generation;
} rts_switch_plan_t;

typedef struct
{
    rts_tcb_t *from;
    rts_tcb_t *to;
    uint32_t generation;
} rts_switch_snapshot_t;

typedef struct
{
    rts_task_pool_t application_task_pool;
    rts_tcb_t idle_task_storage;
    _Alignas(RTS_TASK_STACK_ALIGNMENT)
    unsigned char idle_stack[RTS_IDLE_STACK_SIZE_BYTES];

    rts_kernel_lifecycle_t lifecycle;
    rts_tcb_t *current_task;
    rts_tcb_t *idle_task;
    rts_ready_set_t ready_set;
    rts_delay_queue_t delay_queue;
    rts_tick_t current_tick;
    rts_switch_plan_t switch_plan;
} rts_kernel_state_t;

rts_kernel_state_t *rts_kernel_state_get(void);

rts_tcb_t *rts_scheduler_select_highest_ready(void);
bool rts_scheduler_task_is_idle(const rts_tcb_t *task);
bool rts_scheduler_task_is_runnable(const rts_tcb_t *task);
bool rts_scheduler_task_is_blocked_delay(const rts_tcb_t *task);
bool rts_scheduler_task_is_blocked_wait(const rts_tcb_t *task);
rts_tcb_t *rts_scheduler_current_get(void);
bool rts_scheduler_current_is_valid(void);
bool rts_scheduler_current_establish(rts_tcb_t *task);
bool rts_scheduler_current_release_initial(void);

rts_tick_t rts_kernel_tick_now(void);
bool rts_kernel_tick_advance(rts_tick_t elapsed_ticks);
bool rts_scheduler_prepare_switch(rts_tcb_t *next_task);
void rts_scheduler_request_switch_if_needed(rts_tcb_t *next_task);
bool rts_scheduler_switch_acquire(rts_switch_snapshot_t *snapshot);
void rts_scheduler_switch_complete(const rts_switch_snapshot_t *snapshot);
bool rts_scheduler_switch_reselection_required(void);
bool rts_scheduler_reselect_after_switch(void);

#endif /* RTS_SCHEDULER_INTERNAL_H */
