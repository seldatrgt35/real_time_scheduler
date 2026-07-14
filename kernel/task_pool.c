#include "task_internal.h"

#include "assert_internal.h"

static bool rts_task_pool_index_is_valid(size_t slot_index)
{
    return slot_index < (size_t)RTS_MAX_TASKS;
}

void rts_task_pool_initialize(rts_task_pool_t *pool)
{
    size_t slot_index;

    RTS_ASSERT(pool != NULL);
    if (pool == NULL)
    {
        return;
    }

    for (slot_index = 0u; slot_index < (size_t)RTS_MAX_TASKS; ++slot_index)
    {
        pool->slots[slot_index].slot_state = RTS_TASK_SLOT_FREE;
    }

    pool->allocated_count = 0u;
    pool->next_free_hint = 0u;
}

bool rts_task_pool_reserve(rts_task_pool_t *pool, size_t *out_slot_index)
{
    size_t examined;

    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(out_slot_index != NULL);
    if (pool == NULL || out_slot_index == NULL)
    {
        return false;
    }

    *out_slot_index = RTS_TASK_POOL_INVALID_INDEX;
    RTS_ASSERT(pool->next_free_hint < (size_t)RTS_MAX_TASKS);
    if (pool->next_free_hint >= (size_t)RTS_MAX_TASKS)
    {
        return false;
    }

    for (examined = 0u; examined < (size_t)RTS_MAX_TASKS; ++examined)
    {
        size_t slot_index = pool->next_free_hint + examined;

        if (slot_index >= (size_t)RTS_MAX_TASKS)
        {
            slot_index -= (size_t)RTS_MAX_TASKS;
        }

        if (pool->slots[slot_index].slot_state == RTS_TASK_SLOT_FREE)
        {
            size_t next_hint = slot_index + 1u;

            if (next_hint == (size_t)RTS_MAX_TASKS)
            {
                next_hint = 0u;
            }

            pool->slots[slot_index].slot_state = RTS_TASK_SLOT_RESERVED;
            pool->next_free_hint = next_hint;
            *out_slot_index = slot_index;
            return true;
        }
    }

    return false;
}

void rts_task_pool_commit(rts_task_pool_t *pool, size_t slot_index)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(rts_task_pool_index_is_valid(slot_index));
    if (pool == NULL || !rts_task_pool_index_is_valid(slot_index))
    {
        return;
    }

    RTS_ASSERT(pool->slots[slot_index].slot_state == RTS_TASK_SLOT_RESERVED);
    RTS_ASSERT(pool->allocated_count < (size_t)RTS_MAX_TASKS);
    if (pool->slots[slot_index].slot_state != RTS_TASK_SLOT_RESERVED ||
        pool->allocated_count >= (size_t)RTS_MAX_TASKS)
    {
        return;
    }

    pool->slots[slot_index].slot_state = RTS_TASK_SLOT_ALLOCATED;
    ++pool->allocated_count;
}

void rts_task_pool_rollback(rts_task_pool_t *pool, size_t slot_index)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(rts_task_pool_index_is_valid(slot_index));
    if (pool == NULL || !rts_task_pool_index_is_valid(slot_index))
    {
        return;
    }

    RTS_ASSERT(pool->slots[slot_index].slot_state == RTS_TASK_SLOT_RESERVED);
    if (pool->slots[slot_index].slot_state != RTS_TASK_SLOT_RESERVED)
    {
        return;
    }

    pool->slots[slot_index].slot_state = RTS_TASK_SLOT_FREE;
    pool->next_free_hint = slot_index;
}

rts_tcb_t *rts_task_pool_get(rts_task_pool_t *pool, size_t slot_index)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(rts_task_pool_index_is_valid(slot_index));
    if (pool == NULL || !rts_task_pool_index_is_valid(slot_index))
    {
        return NULL;
    }

    return &pool->slots[slot_index];
}

const rts_tcb_t *rts_task_pool_get_const(const rts_task_pool_t *pool,
                                        size_t slot_index)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(rts_task_pool_index_is_valid(slot_index));
    if (pool == NULL || !rts_task_pool_index_is_valid(slot_index))
    {
        return NULL;
    }

    return &pool->slots[slot_index];
}

size_t rts_task_pool_allocated_count(const rts_task_pool_t *pool)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(pool == NULL || pool->allocated_count <= (size_t)RTS_MAX_TASKS);
    if (pool == NULL || pool->allocated_count > (size_t)RTS_MAX_TASKS)
    {
        return 0u;
    }

    return pool->allocated_count;
}

size_t rts_task_pool_next_free_hint(const rts_task_pool_t *pool)
{
    RTS_ASSERT(pool != NULL);
    RTS_ASSERT(pool == NULL || pool->next_free_hint < (size_t)RTS_MAX_TASKS);
    if (pool == NULL || pool->next_free_hint >= (size_t)RTS_MAX_TASKS)
    {
        return RTS_TASK_POOL_INVALID_INDEX;
    }

    return pool->next_free_hint;
}
