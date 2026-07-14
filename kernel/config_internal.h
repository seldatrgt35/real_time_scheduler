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

_Static_assert(CHAR_BIT == 8, "RTS requires 8-bit bytes");
_Static_assert(sizeof(rts_tick_t) == 4, "Version 1 requires 32-bit ticks");
_Static_assert(RTS_PRIORITY_COUNT <= 256, "priority type cannot represent range");
_Static_assert((RTS_TASK_STACK_ALIGNMENT & (RTS_TASK_STACK_ALIGNMENT - 1u)) == 0,
               "stack alignment must be a power of two");
_Static_assert(RTS_IDLE_STACK_SIZE_BYTES > 0,
               "idle stack size must be nonzero");

#endif /* RTS_CONFIG_INTERNAL_H */
