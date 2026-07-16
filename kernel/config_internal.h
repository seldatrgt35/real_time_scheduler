#ifndef RTS_CONFIG_INTERNAL_H
#define RTS_CONFIG_INTERNAL_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "rts/rts_types.h"

#if !defined(RTS_IDLE_STACK_SIZE_BYTES)
#error "RTS_IDLE_STACK_SIZE_BYTES must be defined by the selected rts_config.h"
#endif

#define RTS_APPLICATION_TASK_CAPACITY ((size_t)RTS_MAX_TASKS)
#define RTS_PRIVATE_TASK_COUNT ((size_t)2u)
#define RTS_SCHEDULABLE_TASK_CAPACITY \
    (RTS_APPLICATION_TASK_CAPACITY + RTS_PRIVATE_TASK_COUNT)

_Static_assert(CHAR_BIT == 8, "RTS requires 8-bit bytes");
_Static_assert(sizeof(rts_tick_t) == 4, "Version 1 requires 32-bit ticks");
_Static_assert(RTS_PRIORITY_COUNT <= 256, "priority type cannot represent range");
_Static_assert((RTS_TASK_STACK_ALIGNMENT & (RTS_TASK_STACK_ALIGNMENT - 1u)) == 0,
               "stack alignment must be a power of two");
_Static_assert(RTS_IDLE_STACK_SIZE_BYTES > 0,
               "idle stack size must be nonzero");
_Static_assert((RTS_TIMER_SERVICE_STACK_SIZE_BYTES %
                RTS_TASK_STACK_ALIGNMENT) == 0u,
               "timer-service stack size must preserve alignment");
_Static_assert(RTS_TIMER_CALLBACK_QUEUE_CAPACITY >= RTS_MAX_TIMERS,
               "callback queue must hold one item per timer");
_Static_assert(RTS_TIME_SLICE_TICKS <= UINT32_MAX,
               "time-slice quantum must fit in rts_tick_t");
_Static_assert(RTS_TICKLESS_MAX_SLEEP_TICKS <= UINT32_C(0x7fffffff),
               "tickless maintenance interval must be wrap safe");
#if RTS_POLICY_RMS
_Static_assert((RTS_MAX_TASKS + 1u) < RTS_PRIORITY_COUNT,
               "RMS requires one priority rank per task plus idle");
_Static_assert(RTS_TIMER_SERVICE_PRIORITY > RTS_MAX_TASKS,
               "timer service must remain above RMS application ranks");
#endif

#endif /* RTS_CONFIG_INTERNAL_H */
