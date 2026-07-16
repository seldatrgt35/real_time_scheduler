#ifndef RTS_READY_QUEUE_H
#define RTS_READY_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "task_internal.h"

#define RTS_READY_BITMAP_WORD_BITS 32u
#define RTS_READY_BITMAP_WORDS \
    ((RTS_PRIORITY_COUNT + RTS_READY_BITMAP_WORD_BITS - 1u) / RTS_READY_BITMAP_WORD_BITS)

typedef struct
{
    rts_list_t priority_queue[RTS_PRIORITY_COUNT];
    uint32_t ready_bitmap[RTS_READY_BITMAP_WORDS];
} rts_ready_set_t;

void rts_ready_initialize(rts_ready_set_t *ready_set);
void rts_ready_insert(rts_ready_set_t *ready_set, rts_tcb_t *task);
void rts_ready_remove(rts_ready_set_t *ready_set, rts_tcb_t *task);
rts_tcb_t *rts_ready_peek_highest(const rts_ready_set_t *ready_set);
bool rts_ready_has_peer(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task);
bool rts_ready_contains(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task);
bool rts_ready_is_front(const rts_ready_set_t *ready_set,
                        const rts_tcb_t *task);
bool rts_ready_only_contains(const rts_ready_set_t *ready_set,
                             const rts_tcb_t *task);
void rts_ready_rotate(rts_ready_set_t *ready_set,
                      rts_priority_t priority);

#endif /* RTS_READY_QUEUE_H */
