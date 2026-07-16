#ifndef RTS_SEMAPHORE_H
#define RTS_SEMAPHORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rts/rts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Statically owned semaphore object.
 *
 * The layout is a deliberate Version 1 public ABI. Applications allocate the
 * object with static lifetime, initialize it once, and must not copy or move it
 * after initialization. Fields are observable for storage purposes only and
 * must not be modified by applications.
 */
typedef struct
{
    struct rts_task *head;
    struct rts_task *tail;
    size_t count;
} rts_wait_object_storage_t;

typedef struct rts_semaphore
{
    rts_count_t count;
    rts_count_t maximum_count;
    rts_wait_object_storage_t waiters;
    const struct rts_semaphore *identity;
    uint32_t signature;
} rts_semaphore_t;

rts_status_t rts_semaphore_init(rts_semaphore_t *semaphore,
                                rts_count_t initial_count,
                                rts_count_t maximum_count);
rts_status_t rts_semaphore_take(rts_semaphore_t *semaphore,
                                rts_tick_t timeout);
rts_status_t rts_semaphore_give(rts_semaphore_t *semaphore);
rts_status_t rts_semaphore_give_from_isr(
    rts_semaphore_t *semaphore,
    bool *higher_priority_task_woken);

#ifdef __cplusplus
}
#endif

#endif /* RTS_SEMAPHORE_H */
