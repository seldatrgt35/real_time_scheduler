#ifndef RTS_WAIT_OBJECT_INTERNAL_H
#define RTS_WAIT_OBJECT_INTERNAL_H

#include <stdbool.h>

#include "rts/rts_semaphore.h"
#include "task_internal.h"

void rts_wait_object_initialize(rts_wait_object_storage_t *object);
bool rts_wait_object_is_empty(const rts_wait_object_storage_t *object);
bool rts_wait_object_contains(const rts_wait_object_storage_t *object,
                              const rts_tcb_t *task);
void rts_wait_object_insert(rts_wait_object_storage_t *object, rts_tcb_t *task);
void rts_wait_object_remove(rts_wait_object_storage_t *object, rts_tcb_t *task);
rts_tcb_t *rts_wait_object_pop_highest(rts_wait_object_storage_t *object);
void rts_wait_object_reprioritize(rts_wait_object_storage_t *object,
                                  rts_tcb_t *task);
bool rts_wait_object_validate(const rts_wait_object_storage_t *object);

#endif /* RTS_WAIT_OBJECT_INTERNAL_H */
