#include "task_internal.h"

#include "assert_internal.h"
#include "config_internal.h"

static bool rts_task_pool_contains(const rts_task_pool_t *pool,
                                   const rts_tcb_t *task)
{
    size_t slot_index;

    if (pool == NULL || task == NULL)
    {
        return false;
    }

    for (slot_index = 0u; slot_index < (size_t)RTS_MAX_TASKS; ++slot_index)
    {
        if (task == &pool->slots[slot_index])
        {
            return true;
        }
    }

    return false;
}

static bool rts_task_node_is_canonical_unlinked(const rts_list_node_t *node)
{
    return node->previous == NULL && node->next == NULL &&
           node->owner == NULL && node->object == NULL;
}

static bool rts_task_descriptor_preconditions_hold(
    const rts_task_config_t *config)
{
    const uintptr_t stack_start = (uintptr_t)config->stack_buffer;

    return config->entry != NULL && config->stack_buffer != NULL &&
           config->stack_size_bytes != 0u &&
           (stack_start % (uintptr_t)RTS_TASK_STACK_ALIGNMENT) == 0u &&
           config->stack_size_bytes <= UINTPTR_MAX &&
           stack_start <= UINTPTR_MAX - (uintptr_t)config->stack_size_bytes &&
           config->priority > RTS_IDLE_PRIORITY &&
           (size_t)config->priority < (size_t)RTS_PRIORITY_COUNT;
}

void rts_task_object_reset(rts_tcb_t *task)
{
    RTS_ASSERT(task != NULL);
    if (task == NULL)
    {
        return;
    }

    RTS_ASSERT(task->slot_state == RTS_TASK_SLOT_RESERVED);
    RTS_ASSERT(task->ready_node.owner == NULL);
    RTS_ASSERT(task->delay_node.owner == NULL);
    RTS_ASSERT(task->wait_node.owner == NULL &&
               task->wait_node.previous == NULL &&
               task->wait_node.next == NULL);
    if (task->slot_state != RTS_TASK_SLOT_RESERVED ||
        task->ready_node.owner != NULL || task->delay_node.owner != NULL ||
        task->wait_node.owner != NULL || task->wait_node.previous != NULL ||
        task->wait_node.next != NULL)
    {
        return;
    }

    task->saved_stack_pointer = NULL;
    task->stack_low = NULL;
    task->stack_high = NULL;
    task->entry = NULL;
    task->argument = NULL;
    rts_list_node_initialize(&task->ready_node);
    rts_list_node_initialize(&task->delay_node);
    task->wait_node.previous = NULL;
    task->wait_node.next = NULL;
    task->wait_node.owner = NULL;
    task->wait.reason = RTS_WAIT_NONE;
    task->wait.result = RTS_WAIT_RESULT_NONE;
    task->wait.wake_tick = 0u;
    task->wait.object = NULL;
    task->wait.timeout_active = false;
    task->slice_remaining = 0u;
    task->owned_mutex_head = NULL;
    task->owned_mutex_tail = NULL;
    task->owned_mutex_count = 0u;
    task->base_priority = RTS_IDLE_PRIORITY;
    task->priority = RTS_IDLE_PRIORITY;
    task->state = RTS_TASK_STATE_DORMANT;
#if RTS_ENABLE_RUNTIME_STATS
    task->diagnostic_dispatch_count = 0u;
    task->diagnostic_block_count = 0u;
    task->diagnostic_wake_count = 0u;
    task->diagnostic_running_ticks = 0u;
    task->diagnostic_last_start_tick = 0u;
#endif
#if RTS_ENABLE_STACK_WATERMARK
    task->diagnostic_max_stack_used = 0u;
#endif
#if RTS_ENABLE_ASSERTIONS
    task->validation_magic = 0u;
#endif
}

rts_status_t rts_task_object_initialize(const rts_task_pool_t *pool,
                                        rts_tcb_t *task,
                                        const rts_task_config_t *config,
                                        rts_kernel_lifecycle_t lifecycle)
{
    bool contract_is_valid;

    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(task != NULL);
    RTS_ASSERT(config != NULL);
    if (pool == NULL || task == NULL || config == NULL)
    {
        return RTS_STATUS_INVALID_ARGUMENT;
    }

    contract_is_valid = rts_task_pool_contains(pool, task);
    RTS_ASSERT(contract_is_valid);
    if (!contract_is_valid)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }

    RTS_ASSERT(lifecycle == RTS_KERNEL_INITIALIZED);
    RTS_ASSERT(task->slot_state == RTS_TASK_SLOT_RESERVED);
    RTS_ASSERT(rts_task_node_is_canonical_unlinked(&task->ready_node));
    RTS_ASSERT(rts_task_node_is_canonical_unlinked(&task->delay_node));
    RTS_ASSERT(task->wait_node.owner == NULL &&
               task->wait_node.previous == NULL &&
               task->wait_node.next == NULL);
    contract_is_valid = lifecycle == RTS_KERNEL_INITIALIZED &&
                        task->slot_state == RTS_TASK_SLOT_RESERVED &&
                        rts_task_node_is_canonical_unlinked(&task->ready_node) &&
                        rts_task_node_is_canonical_unlinked(&task->delay_node) &&
                        task->wait_node.owner == NULL &&
                        task->wait_node.previous == NULL &&
                        task->wait_node.next == NULL;
    if (!contract_is_valid)
    {
        return RTS_STATUS_INVALID_STATE;
    }

    contract_is_valid = rts_task_descriptor_preconditions_hold(config);
    RTS_ASSERT(contract_is_valid);
    if (!contract_is_valid)
    {
        return RTS_STATUS_INVALID_TASK_CONFIG;
    }

    task->saved_stack_pointer = NULL;
    task->stack_low = (unsigned char *)config->stack_buffer;
    task->stack_high = task->stack_low + config->stack_size_bytes;
    task->entry = config->entry;
    task->argument = config->argument;

    rts_list_node_initialize(&task->ready_node);
    rts_list_node_initialize(&task->delay_node);
    task->wait_node.previous = NULL;
    task->wait_node.next = NULL;
    task->wait_node.owner = NULL;

    task->wait.reason = RTS_WAIT_NONE;
    task->wait.result = RTS_WAIT_RESULT_NONE;
    task->wait.wake_tick = 0u;
    task->wait.object = NULL;
    task->wait.timeout_active = false;
    task->slice_remaining = (rts_tick_t)RTS_TIME_SLICE_TICKS;
    task->owned_mutex_head = NULL;
    task->owned_mutex_tail = NULL;
    task->owned_mutex_count = 0u;
    task->base_priority = config->priority;
    task->priority = config->priority;
    task->state = RTS_TASK_STATE_DORMANT;
#if RTS_ENABLE_RUNTIME_STATS
    task->diagnostic_dispatch_count = 0u;
    task->diagnostic_block_count = 0u;
    task->diagnostic_wake_count = 0u;
    task->diagnostic_running_ticks = 0u;
    task->diagnostic_last_start_tick = 0u;
#endif
#if RTS_ENABLE_STACK_WATERMARK
    task->diagnostic_max_stack_used = 0u;
#endif

#if RTS_ENABLE_ASSERTIONS
    task->validation_magic = 0u;
#endif

    return RTS_STATUS_OK;
}
