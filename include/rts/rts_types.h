#ifndef RTS_TYPES_H
#define RTS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "rts_config.h"

#if !defined(RTS_MAX_TASKS)
#error "RTS_MAX_TASKS must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_CPU_COUNT)
#error "RTS_CPU_COUNT must be defined by the selected rts_config.h"
#endif

#if (RTS_CPU_COUNT != 1)
#error "Real-Time Scheduler v1.0 supports exactly one CPU"
#endif

#if !defined(RTS_PRIORITY_COUNT)
#error "RTS_PRIORITY_COUNT must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_MAX_TIMERS)
#error "RTS_MAX_TIMERS must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_TIMER_SERVICE_PRIORITY) ||                              \
    !defined(RTS_TIMER_SERVICE_STACK_SIZE_BYTES) ||                      \
    !defined(RTS_TIMER_CALLBACK_QUEUE_CAPACITY)
#error "all timer-service configuration options must be selected"
#endif

#if !defined(RTS_TICK_RATE_HZ)
#error "RTS_TICK_RATE_HZ must be defined by the selected rts_config.h"
#endif

#if !defined(RTS_ENABLE_TICKLESS_IDLE) ||                              \
    !defined(RTS_TICKLESS_MAX_SLEEP_TICKS)
#error "all Sprint 11 tickless-idle options must be selected"
#endif

#if !defined(RTS_POLICY_FIXED_PRIORITY) || !defined(RTS_POLICY_RMS) || \
    !defined(RTS_POLICY_EDF)
#error "all Sprint 12 scheduler-policy selectors must be defined"
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

#if !defined(RTS_ENABLE_DIAGNOSTICS) || !defined(RTS_ENABLE_TRACE) ||         \
    !defined(RTS_ENABLE_STACK_GUARDS) ||                                    \
    !defined(RTS_ENABLE_STACK_WATERMARK) ||                                 \
    !defined(RTS_ENABLE_RUNTIME_STATS) ||                                   \
    !defined(RTS_ENABLE_INVARIANT_CHECKS) ||                                \
    !defined(RTS_TRACE_CAPACITY) || !defined(RTS_STACK_GUARD_SIZE_BYTES)
#error "all Sprint 9 diagnostic configuration options must be selected"
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

#if (RTS_MAX_TIMERS < 1)
#error "RTS_MAX_TIMERS must be at least 1"
#endif

#if (RTS_TIMER_SERVICE_PRIORITY < 1) ||                                  \
    (RTS_TIMER_SERVICE_PRIORITY >= RTS_PRIORITY_COUNT)
#error "timer-service priority must be in the application-priority range"
#endif

#if (RTS_TIMER_SERVICE_STACK_SIZE_BYTES < 1)
#error "timer-service stack must be nonzero"
#endif

#if (RTS_TIMER_CALLBACK_QUEUE_CAPACITY < RTS_MAX_TIMERS)
#error "callback queue must hold one pending item per timer"
#endif

#if (RTS_PRIORITY_COUNT < 2) || (RTS_PRIORITY_COUNT > 256)
#error "RTS_PRIORITY_COUNT must be in the range 2..256"
#endif

#if (RTS_TICK_RATE_HZ < 1)
#error "RTS_TICK_RATE_HZ must be nonzero"
#endif

#if (RTS_ENABLE_TICKLESS_IDLE != 0) && (RTS_ENABLE_TICKLESS_IDLE != 1)
#error "RTS_ENABLE_TICKLESS_IDLE must be 0 or 1"
#endif

#if (RTS_TICKLESS_MAX_SLEEP_TICKS < 1) ||                         \
    (RTS_TICKLESS_MAX_SLEEP_TICKS > UINT32_C(0x7fffffff))
#error "RTS_TICKLESS_MAX_SLEEP_TICKS must be in the wrap-safe half range"
#endif

#if ((RTS_POLICY_FIXED_PRIORITY != 0) &&                           \
     (RTS_POLICY_FIXED_PRIORITY != 1)) ||                          \
    ((RTS_POLICY_RMS != 0) && (RTS_POLICY_RMS != 1)) ||            \
    ((RTS_POLICY_EDF != 0) && (RTS_POLICY_EDF != 1))
#error "scheduler-policy selectors must be 0 or 1"
#endif

#if (RTS_POLICY_FIXED_PRIORITY + RTS_POLICY_RMS + RTS_POLICY_EDF) != 1
#error "exactly one scheduler policy must be selected"
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

#if ((RTS_ENABLE_DIAGNOSTICS != 0) && (RTS_ENABLE_DIAGNOSTICS != 1)) ||       \
    ((RTS_ENABLE_TRACE != 0) && (RTS_ENABLE_TRACE != 1)) ||                  \
    ((RTS_ENABLE_STACK_GUARDS != 0) && (RTS_ENABLE_STACK_GUARDS != 1)) ||    \
    ((RTS_ENABLE_STACK_WATERMARK != 0) &&                                   \
     (RTS_ENABLE_STACK_WATERMARK != 1)) ||                                  \
    ((RTS_ENABLE_RUNTIME_STATS != 0) && (RTS_ENABLE_RUNTIME_STATS != 1)) ||  \
    ((RTS_ENABLE_INVARIANT_CHECKS != 0) &&                                  \
     (RTS_ENABLE_INVARIANT_CHECKS != 1))
#error "diagnostic feature options must be 0 or 1"
#endif

#if (RTS_ENABLE_TRACE == 1) && (RTS_TRACE_CAPACITY < 1)
#error "RTS_TRACE_CAPACITY must be nonzero when trace is enabled"
#endif

#if (RTS_ENABLE_STACK_GUARDS == 1) && (RTS_STACK_GUARD_SIZE_BYTES < 4)
#error "RTS_STACK_GUARD_SIZE_BYTES must be at least four when enabled"
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
    /** RMS period; optional policy metadata for FP and EDF. */
    rts_tick_t period;
    /** Relative deadline used by RMS validation and EDF ordering. */
    rts_tick_t relative_deadline;
    /** Reserved for analysis/admission work; not enforced in Sprint 12. */
    rts_tick_t execution_budget;
} rts_task_config_t;

#ifdef __cplusplus
}
#endif

#endif /* RTS_TYPES_H */
