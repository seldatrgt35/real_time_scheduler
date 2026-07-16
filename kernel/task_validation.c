#include "task_internal.h"

#include <stdint.h>

#include "assert_internal.h"
#include "port.h"

static bool rts_task_stack_is_valid(const rts_task_config_t *config)
{
    size_t minimum_size = rts_port_task_stack_minimum_size_bytes();
    const size_t size_granularity =
        rts_port_task_stack_size_granularity_bytes();
    const uintptr_t stack_start = (uintptr_t)config->stack_buffer;
    uintptr_t stack_end;

    RTS_ASSERT(minimum_size > 0u);
    RTS_ASSERT(size_granularity > 0u);
    if (minimum_size == 0u || size_granularity == 0u)
    {
        return false;
    }
#if RTS_ENABLE_STACK_GUARDS
    if (minimum_size > SIZE_MAX - (size_t)RTS_STACK_GUARD_SIZE_BYTES)
    {
        return false;
    }
    minimum_size += (size_t)RTS_STACK_GUARD_SIZE_BYTES;
#endif

    if (config->stack_buffer == NULL || config->stack_size_bytes == 0u ||
        (stack_start % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) != 0u ||
        config->stack_size_bytes < minimum_size ||
        (config->stack_size_bytes % size_granularity) != 0u ||
        config->stack_size_bytes > UINTPTR_MAX)
    {
        return false;
    }

    if (stack_start > UINTPTR_MAX - (uintptr_t)config->stack_size_bytes)
    {
        return false;
    }

    stack_end = stack_start + (uintptr_t)config->stack_size_bytes;
    return (stack_end % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) == 0u;
}

rts_status_t rts_task_config_validate(const rts_task_config_t *config,
                                      rts_kernel_lifecycle_t lifecycle)
{
    if (config == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }

    if (lifecycle != RTS_KERNEL_INITIALIZED)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    if (config->entry == NULL)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }

    if (config->priority < (rts_priority_t)1u ||
        (size_t)config->priority >= (size_t)RTS_PRIORITY_COUNT)
    {
        return RTS_STATUS_INVALID_PRIORITY;
    }

#if RTS_POLICY_RMS
    if (config->period == 0u || config->period > RTS_DELAY_MAX ||
        config->relative_deadline == 0u ||
        config->relative_deadline > config->period ||
        config->execution_budget > config->relative_deadline)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }
#elif RTS_POLICY_EDF
    if (config->relative_deadline == 0u ||
        config->relative_deadline > RTS_DELAY_MAX ||
        config->period > RTS_DELAY_MAX ||
        config->execution_budget > config->relative_deadline)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }
#else
    if (config->period > RTS_DELAY_MAX ||
        config->relative_deadline > RTS_DELAY_MAX)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }
#endif

    if (!rts_task_stack_is_valid(config))
    {
        return RTS_STATUS_INVALID_STACK;
    }

    return RTS_STATUS_OK;
}
