#ifndef RTS_DELAY_QUEUE_H
#define RTS_DELAY_QUEUE_H

#include <stdbool.h>

#include "task_internal.h"

typedef struct
{
    rts_list_t ordered_tasks;
} rts_delay_queue_t;

void rts_delay_initialize(rts_delay_queue_t *delay_queue);
void rts_delay_insert(rts_delay_queue_t *delay_queue, rts_tcb_t *task);
void rts_delay_remove(rts_delay_queue_t *delay_queue, rts_tcb_t *task);
rts_tcb_t *rts_delay_peek_expired(const rts_delay_queue_t *delay_queue,
                                  rts_tick_t now);
bool rts_delay_contains(const rts_delay_queue_t *delay_queue,
                        const rts_tcb_t *task);
bool rts_tick_deadline_reached(rts_tick_t now, rts_tick_t deadline);

#endif /* RTS_DELAY_QUEUE_H */
