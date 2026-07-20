#include "stack_check_internal.h"

#include <stdint.h>

void rts_stack_diagnostics_prepare(unsigned char *stack_low,
                                   unsigned char *stack_high)
{
#if RTS_ENABLE_STACK_GUARDS || RTS_ENABLE_STACK_WATERMARK
    unsigned char *cursor;

    if (stack_low == NULL || stack_high == NULL || stack_low >= stack_high)
    {
        return;
    }
#if RTS_ENABLE_STACK_WATERMARK
    for (cursor = stack_low; cursor < stack_high; ++cursor)
    {
        *cursor = RTS_STACK_FILL_PATTERN;
    }
#endif
#if RTS_ENABLE_STACK_GUARDS
    for (cursor = stack_low;
         cursor < stack_low + (size_t)RTS_STACK_GUARD_SIZE_BYTES; ++cursor)
    {
        *cursor = RTS_STACK_GUARD_PATTERN;
    }
#endif
#else
    (void)stack_low;
    (void)stack_high;
#endif
}

bool rts_stack_guard_is_valid(const rts_tcb_t *task)
{
#if RTS_ENABLE_STACK_GUARDS
    size_t index;

    if (task == NULL || task->stack_low == NULL || task->stack_high == NULL ||
        (size_t)(task->stack_high - task->stack_low) <
            (size_t)RTS_STACK_GUARD_SIZE_BYTES)
    {
        return false;
    }
    for (index = 0u; index < (size_t)RTS_STACK_GUARD_SIZE_BYTES; ++index)
    {
        if (task->stack_low[index] != RTS_STACK_GUARD_PATTERN)
        {
            return false;
        }
    }
#else
    (void)task;
#endif
    return true;
}

bool rts_stack_saved_sp_is_valid(const rts_tcb_t *task)
{
    uintptr_t low;
    uintptr_t high;
    uintptr_t saved;

    if (task == NULL || task->stack_low == NULL || task->stack_high == NULL ||
        task->saved_stack_pointer == NULL)
    {
        return false;
    }
    low = (uintptr_t)task->stack_low;
    high = (uintptr_t)task->stack_high;
    saved = (uintptr_t)task->saved_stack_pointer;
#if RTS_ENABLE_STACK_GUARDS
    low += (uintptr_t)RTS_STACK_GUARD_SIZE_BYTES;
#endif
    /* The caller-owned region and initial frame use the public 16-byte
     * contract.  A running Cortex-M task may subsequently have an 8-byte
     * aligned PSP under AAPCS; diagnostics must accept that legal saved SP. */
    return low <= saved && saved < high &&
           (saved % (uintptr_t)sizeof(uint64_t)) == 0u;
}

size_t rts_stack_high_water_used_bytes(const rts_tcb_t *task)
{
#if RTS_ENABLE_STACK_WATERMARK
    const unsigned char *cursor;
    const unsigned char *usable_low;
    size_t usable;
    size_t untouched = 0u;

    if (task == NULL || task->stack_low == NULL || task->stack_high == NULL ||
        task->stack_low >= task->stack_high)
    {
        return 0u;
    }
    usable_low = task->stack_low;
#if RTS_ENABLE_STACK_GUARDS
    usable_low += (size_t)RTS_STACK_GUARD_SIZE_BYTES;
#endif
    usable = (size_t)(task->stack_high - usable_low);
    for (cursor = usable_low; cursor < task->stack_high; ++cursor)
    {
        if (*cursor != RTS_STACK_FILL_PATTERN)
        {
            break;
        }
        ++untouched;
    }
    return usable - untouched;
#else
    (void)task;
    return 0u;
#endif
}

size_t rts_stack_watermark_update(rts_tcb_t *task)
{
    size_t used = rts_stack_high_water_used_bytes(task);

#if RTS_ENABLE_STACK_WATERMARK
    if (task != NULL && used > (size_t)task->diagnostic_max_stack_used)
    {
        task->diagnostic_max_stack_used =
            used > UINT32_MAX ? UINT32_MAX : (uint32_t)used;
    }
#else
    (void)task;
#endif
    return used;
}
