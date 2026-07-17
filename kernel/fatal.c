#include "fatal_internal.h"

#include <stddef.h>

#include "port.h"
#include "scheduler_internal.h"

#if defined(__GNUC__) || defined(__clang__)
#define RTS_WEAK __attribute__((weak))
#else
#define RTS_WEAK
#endif

volatile rts_fatal_record_t g_rts_fatal_record;

bool rts_fatal_record_capture(rts_fatal_reason_t reason,
                              const void *context,
                              const char *file,
                              uint32_t line)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    if (g_rts_fatal_record.valid != 0u)
    {
        return false;
    }
    g_rts_fatal_record.reason = reason;
    g_rts_fatal_record.lifecycle = kernel->lifecycle;
    g_rts_fatal_record.tick = kernel->current_tick;
    g_rts_fatal_record.exception_number = rts_port_exception_number();
    g_rts_fatal_record.current_task = (uintptr_t)rts_scheduler_current_get();
    g_rts_fatal_record.context = (uintptr_t)context;
    g_rts_fatal_record.source_file = (uintptr_t)file;
    g_rts_fatal_record.source_line = line;
    g_rts_fatal_record.switch_generation = kernel->switch_plan.generation;
    g_rts_fatal_record.switch_flags =
        (kernel->switch_plan.pending ? UINT32_C(1) : UINT32_C(0)) |
        (kernel->switch_plan.active ? UINT32_C(2) : UINT32_C(0)) |
        (kernel->switch_plan.reselection_required ? UINT32_C(4) : UINT32_C(0));
    g_rts_fatal_record.fatal_count = 1u;
    g_rts_fatal_record.valid = 1u;
    return true;
}

_Noreturn void rts_kernel_fatal_at(rts_fatal_reason_t reason,
                                   const void *context,
                                   const char *file,
                                   uint32_t line)
{
    bool first;

    rts_port_fatal_disable();
    first = rts_fatal_record_capture(reason, context, file, line);
    if (first)
    {
        rts_target_fatal_hook((const rts_fatal_record_t *)&g_rts_fatal_record);
    }
    for (;;)
    {
    }
}

RTS_WEAK void rts_assert_fail(const char *expression,
                              const char *file,
                              int line)
{
    rts_kernel_fatal_at(RTS_FATAL_ASSERTION, expression, file,
                        line < 0 ? 0u : (uint32_t)line);
}

RTS_WEAK void rts_target_fatal_hook(const rts_fatal_record_t *record)
{
    (void)record;
}

void rts_fatal_record_reset_for_test(void)
{
    g_rts_fatal_record = (rts_fatal_record_t){0};
}
