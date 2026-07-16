#ifndef RTS_MUTEX_H
#define RTS_MUTEX_H

#include <stdint.h>

#include "rts/rts_semaphore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rts_mutex
{
    struct rts_task *owner;
    rts_wait_object_storage_t waiters;
    struct rts_mutex *owned_previous;
    struct rts_mutex *owned_next;
    const struct rts_mutex *identity;
    uint32_t signature;
} rts_mutex_t;

rts_status_t rts_mutex_init(rts_mutex_t *mutex);
rts_status_t rts_mutex_lock(rts_mutex_t *mutex, rts_tick_t timeout);
rts_status_t rts_mutex_unlock(rts_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* RTS_MUTEX_H */
