#ifndef RTS_TIMER_CALLBACK_QUEUE_H
#define RTS_TIMER_CALLBACK_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rts/rts_types.h"

struct rts_timer;

typedef struct
{
    struct rts_timer *timer;
    rts_tick_t expiration_tick;
    uint32_t generation;
    uint32_t sequence;
} rts_timer_callback_work_t;

typedef struct
{
    rts_timer_callback_work_t items[RTS_TIMER_CALLBACK_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    size_t maximum_depth;
} rts_timer_callback_queue_t;

void rts_timer_callback_queue_initialize(rts_timer_callback_queue_t *queue);
bool rts_timer_callback_queue_enqueue(
    rts_timer_callback_queue_t *queue,
    const rts_timer_callback_work_t *work);
bool rts_timer_callback_queue_dequeue(rts_timer_callback_queue_t *queue,
                                      rts_timer_callback_work_t *work);
size_t rts_timer_callback_queue_remove_timer(
    rts_timer_callback_queue_t *queue,
    const struct rts_timer *timer);
size_t rts_timer_callback_queue_count_for(
    const rts_timer_callback_queue_t *queue,
    const struct rts_timer *timer);
bool rts_timer_callback_queue_validate(
    const rts_timer_callback_queue_t *queue);

#endif /* RTS_TIMER_CALLBACK_QUEUE_H */
