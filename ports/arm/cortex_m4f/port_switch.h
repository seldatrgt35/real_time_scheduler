#ifndef RTS_CORTEX_M4F_PORT_SWITCH_H
#define RTS_CORTEX_M4F_PORT_SWITCH_H

#include <stdbool.h>

#include "scheduler_internal.h"

typedef struct
{
    rts_tcb_t *from;
    rts_tcb_t *to;
    rts_switch_snapshot_t snapshot;
    void *outgoing_saved_stack_pointer;
    void *incoming_saved_stack_pointer;
} rts_cm4f_switch_handoff_t;

const rts_cm4f_switch_handoff_t *rts_cm4f_switch_bridge_acquire(
    void *outgoing_saved_stack_pointer);
bool rts_cm4f_switch_bridge_no_plan(void);
bool rts_cm4f_switch_bridge_complete(
    const rts_cm4f_switch_handoff_t *handoff);

#endif /* RTS_CORTEX_M4F_PORT_SWITCH_H */
