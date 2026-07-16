#ifndef RTS_TYPES_H
#define RTS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "rts_config.h"

#if !defined(RTS_MAX_TASKS)
#error "RTS_MAX_TASKS must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_PRIORITY_COUNT)
#error "RTS_PRIORITY_COUNT must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_TICK_RATE_HZ)
#error "RTS_TICK_RATE_HZ must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_ENABLE_TIME_SLICING)
#error "RTS_ENABLE_TIME_SLICING must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_TIME_SLICE_TICKS)
#error "RTS_TIME_SLICE_TICKS must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_ENABLE_ASSERTIONS)
#error "RTS_ENABLE_ASSERTIONS must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_MAX_MUTEXES_PER_TASK)
#define RTS_MAX_MUTEXES_PER_TASK RTS_MAX_TASKS
#endif

#if (RTS_MAX_MUTEXES_PER_TASK < 1)
#error "RTS_MAX_MUTEXES_PER_TASK must be at least 1"
#endif

#if (RTS_MAX_TASKS < 1)
#error "RTS_MAX_TASKS must be at least 1"
#endif

#if (RTS_PRIORITY_COUNT < 2) || (RTS_PRIORITY_COUNT > 256)
#error "RTS_PRIORITY_COUNT must be in the range 2..256"
#endif

#if (RTS_TICK_RATE_HZ < 1)
#error "RTS_TICK_RATE_HZ must be nonzero"
#endif

#if (RTS_ENABLE_TIME_SLICING != 0) && (RTS_ENABLE_TIME_SLICING != 1)
#error "RTS_ENABLE_TIME_SLICING must be 0 or 1"
#endif

#if (RTS_ENABLE_TIME_SLICING == 1) && (RTS_TIME_SLICE_TICKS < 1)
#error "RTS_TIME_SLICE_TICKS must be nonzero when time slicing is enabled"
#endif

#if (RTS_ENABLE_ASSERTIONS != 0) && (RTS_ENABLE_ASSERTIONS != 1)
#error "RTS_ENABLE_ASSERTIONS must be 0 or 1"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Portable minimum alignment required for an application-supplied task stack. */
#define RTS_TASK_STACK_ALIGNMENT    16u

/** Largest relative delay accepted by rts_task_delay(). */
#define RTS_DELAY_MAX               UINT32_C(0x7fffffff)

/** Explicit infinite timeout value for blocking synchronization operations. */
#define RTS_WAIT_FOREVER            UINT32_MAX

/** Priority zero is reserved for the kernel idle task. */
#define RTS_IDLE_PRIORITY           UINT8_C(0)

/** Declare a byte-counted task stack with static lifetime and required alignment. */
#define RTS_TASK_STACK_DECLARE(name, size_bytes) \
    static _Alignas(RTS_TASK_STACK_ALIGNMENT) unsigned char name[(size_bytes)]

typedef uint32_t rts_tick_t;
typedef uint32_t rts_count_t;
typedef uint8_t rts_priority_t;

typedef enum
{
    RTS_STATUS_OK = 0,
    RTS_STATUS_INVALID_ARGUMENT,
    RTS_STATUS_INVALID_CONTEXT,
    RTS_STATUS_INVALID_STATE,
    RTS_STATUS_INVALID_TASK_CONFIG,
    RTS_STATUS_INVALID_PRIORITY,
    RTS_STATUS_INVALID_STACK,
    RTS_STATUS_CAPACITY_EXHAUSTED,
    RTS_STATUS_ALREADY_INITIALIZED,
    RTS_STATUS_ALREADY_STARTED,
    RTS_STATUS_TIMEOUT,
    RTS_STATUS_FULL,
    RTS_STATUS_PORT_ERROR
} rts_status_t;

struct rts_task;
typedef struct rts_task *rts_task_handle_t;

typedef void (*rts_task_entry_t)(void *argument);

typedef struct
{
    rts_task_entry_t entry;
    void *argument;
    void *stack_buffer;
    size_t stack_size_bytes;
    rts_priority_t priority;
} rts_task_config_t;

#ifdef __cplusplus
}
#endif

#endif /* RTS_TYPES_H */
