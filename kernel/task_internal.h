#ifndef RTS_TASK_INTERNAL_H
#define RTS_TASK_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "rts/rts_types.h"
#include "intrusive_list.h"
#include "lifecycle_internal.h"

typedef uint8_t rts_task_slot_state_t;
enum
{
    RTS_TASK_SLOT_FREE = 0,
    RTS_TASK_SLOT_RESERVED,
    RTS_TASK_SLOT_ALLOCATED
};

typedef uint8_t rts_task_state_t;
enum
{
    RTS_TASK_STATE_DORMANT = 0,
    RTS_TASK_STATE_READY,
    RTS_TASK_STATE_RUNNING,
    RTS_TASK_STATE_BLOCKED
};

typedef uint8_t rts_wait_reason_t;
enum
{
    RTS_WAIT_NONE = 0,
    RTS_WAIT_DELAY
};

typedef struct
{
    rts_wait_reason_t reason;
    rts_tick_t wake_tick;
} rts_wait_t;

struct rts_task
{
    /* Offset zero is part of the architecture-port layout contract. */
    void *saved_stack_pointer;

    unsigned char *stack_low;
    unsigned char *stack_high;
    rts_task_entry_t entry;
    void *argument;

    rts_list_node_t ready_node;
    rts_list_node_t delay_node;
    rts_wait_t wait;
    rts_tick_t slice_remaining;

    rts_priority_t priority;
    rts_task_state_t state;
    rts_task_slot_state_t slot_state;

#if RTS_ENABLE_ASSERTIONS
    uint32_t validation_magic;
#endif
};

typedef struct rts_task rts_tcb_t;

typedef struct
{
    rts_tcb_t slots[RTS_MAX_TASKS];
    size_t allocated_count;
    size_t next_free_hint;
} rts_task_pool_t;

#define RTS_TASK_POOL_INVALID_INDEX SIZE_MAX

_Static_assert(offsetof(struct rts_task, saved_stack_pointer) == 0,
               "saved stack pointer must remain at TCB offset zero");

void rts_task_object_reset(rts_tcb_t *task);
rts_status_t rts_task_object_initialize(const rts_task_pool_t *pool,
                                        rts_tcb_t *task,
                                        const rts_task_config_t *config,
                                        rts_kernel_lifecycle_t lifecycle);
bool rts_task_handle_is_application_task(rts_task_handle_t handle);
bool rts_task_object_is_valid(const rts_tcb_t *task);

void rts_task_pool_initialize(rts_task_pool_t *pool);
bool rts_task_pool_reserve(rts_task_pool_t *pool, size_t *out_slot_index);
void rts_task_pool_commit(rts_task_pool_t *pool, size_t slot_index);
void rts_task_pool_rollback(rts_task_pool_t *pool, size_t slot_index);
rts_tcb_t *rts_task_pool_get(rts_task_pool_t *pool, size_t slot_index);
const rts_tcb_t *rts_task_pool_get_const(const rts_task_pool_t *pool,
                                        size_t slot_index);
size_t rts_task_pool_allocated_count(const rts_task_pool_t *pool);
size_t rts_task_pool_next_free_hint(const rts_task_pool_t *pool);

rts_status_t rts_task_config_validate(const rts_task_config_t *config,
                                      rts_kernel_lifecycle_t lifecycle);

#endif /* RTS_TASK_INTERNAL_H */
