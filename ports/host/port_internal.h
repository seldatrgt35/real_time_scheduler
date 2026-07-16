#ifndef RTS_HOST_PORT_INTERNAL_H
#define RTS_HOST_PORT_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "port.h"

#define RTS_HOST_INITIAL_FRAME_MAGIC UINT32_C(0x48535446)
#define RTS_HOST_INITIAL_FRAME_VERSION UINT32_C(1)

typedef void (*rts_host_task_return_trap_t)(void);

typedef struct
{
    uint32_t magic;
    uint32_t version;
    rts_task_entry_t entry;
    void *argument;
    rts_host_task_return_trap_t return_trap;
    uintptr_t reserved;
} rts_host_initial_frame_t;

void rts_host_port_task_return_trap(void);
bool rts_host_port_initial_frame_read(const void *saved_stack_pointer,
                                      rts_host_initial_frame_t *out_frame);
void rts_host_port_test_reset(void);
void rts_host_port_test_set_isr(bool in_isr);
void rts_host_port_test_fail_next_stack_initialize(bool fail);
void rts_host_port_test_fail_next_initialize(bool fail);
void rts_host_port_test_fail_next_tick_start(bool fail);
bool rts_host_port_test_is_initialized(void);
bool rts_host_port_test_tick_running(void);
size_t rts_host_port_test_critical_depth(void);
size_t rts_host_port_test_switch_request_count(void);
size_t rts_host_port_test_last_switch_request_critical_depth(void);
bool rts_host_port_test_switch_request_pending(void);
void rts_host_port_test_consume_switch_request(void);
void rts_host_port_test_start_reset(void);
size_t rts_host_port_test_start_request_count(void);
rts_task_handle_t rts_host_port_test_start_task(void);
bool rts_host_port_test_start_consumed(void);
void *rts_host_port_test_start_saved_stack_pointer(void);
void rts_host_port_test_fail_next_start(bool fail);
void rts_host_port_test_set_next_wake(rts_tick_t elapsed_ticks,
                                      rts_port_wake_source_t source);
void rts_host_port_test_fail_next_sleep(bool fail);
size_t rts_host_port_test_sleep_count(void);
rts_tick_t rts_host_port_test_last_sleep_limit(void);

#endif /* RTS_HOST_PORT_INTERNAL_H */
