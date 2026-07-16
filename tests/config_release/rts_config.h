#ifndef RTS_CONFIG_H
#define RTS_CONFIG_H

#define RTS_MAX_TASKS                 8u
#define RTS_PRIORITY_COUNT            65u
#define RTS_TICK_RATE_HZ              1000u
#define RTS_ENABLE_TIME_SLICING       1
#define RTS_TIME_SLICE_TICKS          10u
#define RTS_ENABLE_ASSERTIONS         0
#define RTS_ENABLE_DIAGNOSTICS        0
#define RTS_ENABLE_TRACE              0
#define RTS_ENABLE_STACK_GUARDS       0
#define RTS_ENABLE_STACK_WATERMARK    0
#define RTS_ENABLE_RUNTIME_STATS      0
#define RTS_ENABLE_INVARIANT_CHECKS   0
#define RTS_TRACE_CAPACITY            1u
#define RTS_STACK_GUARD_SIZE_BYTES    0u
#define RTS_IDLE_STACK_SIZE_BYTES     256u

#endif /* RTS_CONFIG_H */
