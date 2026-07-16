#ifndef RTS_TIMER_QUEUE_H
#define RTS_TIMER_QUEUE_H

#include <stdbool.h>

#include "intrusive_list.h"

struct rts_timer;

typedef struct
{
    rts_list_t ordered_timers;
} rts_timer_queue_t;

void rts_timer_queue_initialize(rts_timer_queue_t *queue);
void rts_timer_queue_insert(rts_timer_queue_t *queue,
                            struct rts_timer *timer);
void rts_timer_queue_remove(rts_timer_queue_t *queue,
                            struct rts_timer *timer);
struct rts_timer *rts_timer_queue_peek_expired(
    const rts_timer_queue_t *queue,
    rts_tick_t now);
bool rts_timer_queue_contains(const rts_timer_queue_t *queue,
                              const struct rts_timer *timer);
bool rts_timer_queue_validate(const rts_timer_queue_t *queue);

#endif /* RTS_TIMER_QUEUE_H */
