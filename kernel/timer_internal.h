#ifndef RTS_TIMER_INTERNAL_H
#define RTS_TIMER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rts/rts_timer.h"
#include "timer_queue.h"

typedef uint8_t rts_timer_slot_state_t;
enum
{
    RTS_TIMER_SLOT_FREE = 0,
    RTS_TIMER_SLOT_RESERVED,
    RTS_TIMER_SLOT_ALLOCATED
};

typedef uint8_t rts_timer_state_t;
enum
{
    RTS_TIMER_UNINITIALIZED = 0,
    RTS_TIMER_STOPPED,
    RTS_TIMER_RUNNING,
    RTS_TIMER_EXPIRED
};

struct rts_timer
{
    rts_tick_t expiration_tick;
    rts_tick_t last_expiration_tick;
    rts_tick_t period;
    rts_timer_callback_t callback;
    void *argument;
    rts_list_node_t queue_node;
    size_t slot_index;
    rts_timer_mode_t mode;
    rts_timer_state_t state;
    rts_timer_slot_state_t slot_state;
#if RTS_ENABLE_RUNTIME_STATS
    uint32_t diagnostic_start_count;
    uint32_t diagnostic_stop_count;
    uint32_t diagnostic_restart_count;
    uint32_t diagnostic_expiration_count;
#endif
#if RTS_ENABLE_ASSERTIONS
    uint32_t validation_magic;
#endif
};

typedef struct
{
    struct rts_timer slots[RTS_MAX_TIMERS];
    rts_timer_queue_t running_queue;
    size_t allocated_count;
    size_t next_free_hint;
} rts_timer_manager_t;

#define RTS_TIMER_VALIDATION_MAGIC UINT32_C(0x5254544d)

void rts_timer_manager_initialize(void);
void rts_timer_manager_process_expired(rts_tick_t now);
rts_timer_manager_t *rts_timer_manager_get(void);
bool rts_timer_manager_validate(const rts_timer_manager_t *manager);
bool rts_timer_handle_is_valid(rts_timer_handle_t timer);
size_t rts_timer_allocated_count(void);

#endif /* RTS_TIMER_INTERNAL_H */
