#ifndef RTS_CORTEX_M4F_PORT_INTERNAL_H
#define RTS_CORTEX_M4F_PORT_INTERNAL_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "scheduler_internal.h"
#include "task_internal.h"
#include "port_config.h"
#include "port_offsets.h"
#include "port_switch.h"

#if !defined(__arm__) && !defined(__thumb__)
#error "Cortex-M4F port requires an ARM target compiler"
#endif

#if !defined(__thumb__)
#error "Cortex-M4F port must be compiled in Thumb mode"
#endif

#if defined(__ARM_PCS_VFP)
#error "Version 1 Cortex-M4F port forbids the hard-float procedure-call ABI"
#endif

#if defined(__ARM_FP) && (__ARM_FP != 0)
#error "Version 1 kernel/port build must disable FP instruction generation"
#endif

#define RTS_CM4F_INITIAL_XPSR                UINT32_C(0x01000000)

#define RTS_CM4F_TASK_STACK_MINIMUM_SIZE_BYTES \
    RTS_CM4F_INITIAL_FRAME_SIZE_BYTES
#define RTS_CM4F_TASK_STACK_GRANULARITY_BYTES RTS_TASK_STACK_ALIGNMENT
#define RTS_CM4F_INITIAL_FRAME_WORD_COUNT       16u

typedef struct
{
    rts_tcb_t *first_task;
    void *saved_stack_pointer;
    uint32_t cookie;
    uint32_t valid;
} rts_cm4f_start_handoff_t;

#define RTS_CM4F_START_HANDOFF_COOKIE UINT32_C(0x52545353)

_Static_assert(CHAR_BIT == 8, "Cortex-M port requires 8-bit bytes");
_Static_assert(sizeof(uint32_t) == 4u, "Cortex-M words must be 32 bits");
_Static_assert(sizeof(void *) == 4u, "Cortex-M pointers must be 32 bits");
_Static_assert(sizeof(rts_task_entry_t) == 4u,
               "Supported Cortex-M ABI requires 32-bit function pointers");
_Static_assert(offsetof(struct rts_task, saved_stack_pointer) ==
                   RTS_CM4F_TCB_SAVED_SP_OFFSET,
               "TCB saved-SP assembly offset mismatch");
_Static_assert(offsetof(rts_cm4f_switch_handoff_t, from) ==
                   RTS_CM4F_HANDOFF_FROM_OFFSET,
               "switch-handoff from offset mismatch");
_Static_assert(offsetof(rts_cm4f_switch_handoff_t, to) ==
                   RTS_CM4F_HANDOFF_TO_OFFSET,
               "switch-handoff to offset mismatch");
_Static_assert(offsetof(rts_cm4f_start_handoff_t, first_task) ==
                   RTS_CM4F_START_TASK_OFFSET,
               "startup-handoff task offset mismatch");
_Static_assert(offsetof(rts_cm4f_start_handoff_t, saved_stack_pointer) ==
                   RTS_CM4F_START_SAVED_SP_OFFSET,
               "startup-handoff saved-SP offset mismatch");
_Static_assert(offsetof(rts_cm4f_start_handoff_t, cookie) ==
                   RTS_CM4F_START_COOKIE_OFFSET,
               "startup-handoff cookie offset mismatch");
_Static_assert(offsetof(rts_cm4f_start_handoff_t, valid) ==
                   RTS_CM4F_START_VALID_OFFSET,
               "startup-handoff valid offset mismatch");
_Static_assert((RTS_CM4F_INITIAL_FRAME_SIZE_BYTES %
                RTS_TASK_STACK_ALIGNMENT) == 0u,
               "initial frame must preserve public stack alignment");
_Static_assert(RTS_CM4F_INITIAL_FRAME_SIZE_BYTES ==
                   (RTS_CM4F_INITIAL_FRAME_WORD_COUNT * sizeof(uint32_t)),
               "initial frame size/word count mismatch");
_Static_assert(RTS_CM4F_TASK_STACK_MINIMUM_SIZE_BYTES >=
                   RTS_CM4F_INITIAL_FRAME_SIZE_BYTES,
               "port minimum stack must contain the initial frame");
_Static_assert((RTS_TASK_STACK_ALIGNMENT %
                RTS_CM4F_ARCH_STACK_ALIGNMENT) == 0u,
               "public alignment must satisfy Cortex-M alignment");

const rts_cm4f_start_handoff_t *rts_cm4f_start_handoff_get(void);
rts_status_t rts_cm4f_start_handoff_prepare(void);
void *rts_cm4f_start_handoff_consume(void);
_Noreturn void rts_cm4f_start_trigger(void);
_Noreturn void rts_cm4f_start_fatal(void);

_Noreturn void rts_cm4f_task_return_trap(void);
void PendSV_Handler(void);
void SVC_Handler(void);

#endif /* RTS_CORTEX_M4F_PORT_INTERNAL_H */
