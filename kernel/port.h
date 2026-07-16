#ifndef RTS_PORT_H
#define RTS_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rts/rts_types.h"
#include "rts/rts_power.h"

typedef uintptr_t rts_critical_token_t;

typedef struct
{
    rts_status_t status;
    void *saved_stack_pointer;
} rts_port_stack_result_t;

typedef rts_wake_source_t rts_port_wake_source_t;
enum
{
    RTS_PORT_WAKE_TIMER = RTS_WAKE_SOURCE_TIMER,
    RTS_PORT_WAKE_EXTERNAL = RTS_WAKE_SOURCE_EXTERNAL,
    RTS_PORT_WAKE_GPIO = RTS_WAKE_SOURCE_GPIO,
    RTS_PORT_WAKE_OTHER = RTS_WAKE_SOURCE_OTHER
};

typedef struct
{
    rts_status_t status;
    rts_tick_t elapsed_ticks;
    rts_port_wake_source_t wake_source;
} rts_port_sleep_result_t;

rts_status_t rts_port_initialize(void);
size_t rts_port_task_stack_minimum_size_bytes(void);
size_t rts_port_task_stack_size_granularity_bytes(void);
rts_port_stack_result_t rts_port_stack_initialize(void *stack_buffer,
                                                   size_t stack_size_bytes,
                                                   rts_task_entry_t entry,
                                                   void *argument);
rts_critical_token_t rts_port_critical_enter(void);
void rts_port_critical_exit(rts_critical_token_t token);
bool rts_port_is_in_isr(void);
uint32_t rts_port_exception_number(void);
void rts_port_fatal_disable(void);
void rts_port_request_context_switch(void);
rts_status_t rts_port_tick_start(void);
bool rts_port_tick_commit_start(void);
void rts_port_tick_stop(void);
rts_status_t rts_port_start_first_task(void);
rts_port_sleep_result_t rts_port_power_sleep(rts_tick_t maximum_ticks);

#endif /* RTS_PORT_H */
