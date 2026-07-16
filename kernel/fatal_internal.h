#ifndef RTS_FATAL_INTERNAL_H
#define RTS_FATAL_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t rts_fatal_reason_t;
enum
{
    RTS_FATAL_NONE = 0,
    RTS_FATAL_ASSERTION,
    RTS_FATAL_INVALID_STATE,
    RTS_FATAL_LIST_CORRUPTION,
    RTS_FATAL_STACK_CORRUPTION,
    RTS_FATAL_SWITCH_CORRUPTION,
    RTS_FATAL_WAIT_CORRUPTION,
    RTS_FATAL_CONTEXT_CORRUPTION,
    RTS_FATAL_TASK_RETURNED,
    RTS_FATAL_HARDFAULT,
    RTS_FATAL_TARGET_CONFIGURATION,
    RTS_FATAL_TIMER_CORRUPTION
};

typedef struct
{
    volatile uint32_t valid;
    volatile rts_fatal_reason_t reason;
    volatile uint8_t lifecycle;
    volatile uint16_t reserved;
    volatile uint32_t tick;
    volatile uint32_t exception_number;
    volatile uintptr_t current_task;
    volatile uintptr_t context;
    volatile uintptr_t source_file;
    volatile uint32_t source_line;
    volatile uint32_t switch_generation;
    volatile uint32_t switch_flags;
    volatile uint32_t fatal_count;
} rts_fatal_record_t;

extern volatile rts_fatal_record_t g_rts_fatal_record;

bool rts_fatal_record_capture(rts_fatal_reason_t reason,
                              const void *context,
                              const char *file,
                              uint32_t line);
_Noreturn void rts_kernel_fatal_at(rts_fatal_reason_t reason,
                                   const void *context,
                                   const char *file,
                                   uint32_t line);
void rts_target_fatal_hook(const rts_fatal_record_t *record);
void rts_fatal_record_reset_for_test(void);

#define RTS_KERNEL_FATAL(reason, context) \
    rts_kernel_fatal_at((reason), (context), __FILE__, (uint32_t)__LINE__)

#endif /* RTS_FATAL_INTERNAL_H */
