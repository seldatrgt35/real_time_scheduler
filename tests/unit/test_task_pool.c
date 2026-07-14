#include <stdbool.h>
#include <stddef.h>

#include "task_internal.h"

static int test_failures;
static unsigned int assertion_count;

#define CHECK(condition)                    \
    do                                      \
    {                                       \
        if (!(condition))                   \
        {                                   \
            ++test_failures;                \
        }                                   \
    } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

static void reserve_and_commit(rts_task_pool_t *pool, size_t expected_index)
{
    size_t slot_index = RTS_TASK_POOL_INVALID_INDEX;

    CHECK(rts_task_pool_reserve(pool, &slot_index));
    CHECK(slot_index == expected_index);
    CHECK(rts_task_pool_get(pool, slot_index)->slot_state == RTS_TASK_SLOT_RESERVED);
    CHECK(rts_task_pool_allocated_count(pool) == expected_index);
    rts_task_pool_commit(pool, slot_index);
    CHECK(rts_task_pool_get_const(pool, slot_index)->slot_state ==
          RTS_TASK_SLOT_ALLOCATED);
    CHECK(rts_task_pool_allocated_count(pool) == expected_index + 1u);
}

static void test_initialization_and_first_slot(void)
{
    rts_task_pool_t pool;
    size_t slot_index;
    size_t index;

    pool.slots[0].priority = 37u;
    rts_task_pool_initialize(&pool);

    CHECK(pool.slots[0].priority == 37u);
    CHECK(rts_task_pool_allocated_count(&pool) == 0u);
    CHECK(rts_task_pool_next_free_hint(&pool) == 0u);
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        CHECK(pool.slots[index].slot_state == RTS_TASK_SLOT_FREE);
    }

    slot_index = RTS_TASK_POOL_INVALID_INDEX;
    CHECK(rts_task_pool_reserve(&pool, &slot_index));
    CHECK(slot_index == 0u);
    CHECK(pool.slots[0].slot_state == RTS_TASK_SLOT_RESERVED);
    CHECK(rts_task_pool_allocated_count(&pool) == 0u);
    CHECK(rts_task_pool_next_free_hint(&pool) == 1u);
    CHECK(rts_task_pool_get(&pool, 0u) == &pool.slots[0]);
    CHECK(rts_task_pool_get_const(&pool, 0u) == &pool.slots[0]);
}

static void test_capacity_and_commit(void)
{
    rts_task_pool_t pool;
    size_t slot_index = 0u;
    size_t index;

    rts_task_pool_initialize(&pool);
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        reserve_and_commit(&pool, index);
    }

    CHECK(rts_task_pool_allocated_count(&pool) == (size_t)RTS_MAX_TASKS);
    CHECK(rts_task_pool_next_free_hint(&pool) == 0u);
    CHECK(!rts_task_pool_reserve(&pool, &slot_index));
    CHECK(slot_index == RTS_TASK_POOL_INVALID_INDEX);
}

static void test_rollback_and_reserved_semantics(void)
{
    rts_task_pool_t pool;
    size_t first;
    size_t second;

    rts_task_pool_initialize(&pool);
    CHECK(rts_task_pool_reserve(&pool, &first));
    CHECK(rts_task_pool_reserve(&pool, &second));
    CHECK(first == 0u);
    CHECK(second == 1u);
    CHECK(rts_task_pool_allocated_count(&pool) == 0u);

    rts_task_pool_commit(&pool, first);
    CHECK(rts_task_pool_allocated_count(&pool) == 1u);
    rts_task_pool_rollback(&pool, second);
    CHECK(pool.slots[second].slot_state == RTS_TASK_SLOT_FREE);
    CHECK(rts_task_pool_allocated_count(&pool) == 1u);
    CHECK(rts_task_pool_next_free_hint(&pool) == second);

    first = RTS_TASK_POOL_INVALID_INDEX;
    CHECK(rts_task_pool_reserve(&pool, &first));
    CHECK(first == second);
    CHECK(pool.slots[first].slot_state == RTS_TASK_SLOT_RESERVED);
}

#if RTS_ENABLE_ASSERTIONS
static void test_contract_assertions(void)
{
    rts_task_pool_t pool;
    size_t slot_index;
    unsigned int before;

    rts_task_pool_initialize(&pool);
    CHECK(rts_task_pool_reserve(&pool, &slot_index));
    rts_task_pool_commit(&pool, slot_index);

    before = assertion_count;
    rts_task_pool_commit(&pool, slot_index);
    CHECK(assertion_count == before + 1u);
    CHECK(rts_task_pool_allocated_count(&pool) == 1u);

    before = assertion_count;
    rts_task_pool_rollback(&pool, slot_index);
    CHECK(assertion_count == before + 1u);
    CHECK(pool.slots[slot_index].slot_state == RTS_TASK_SLOT_ALLOCATED);

    rts_task_pool_initialize(&pool);
    before = assertion_count;
    rts_task_pool_rollback(&pool, 0u);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    rts_task_pool_commit(&pool, 0u);
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    rts_task_pool_commit(&pool, (size_t)RTS_MAX_TASKS);
    CHECK(assertion_count == before + 1u);

    pool.next_free_hint = (size_t)RTS_MAX_TASKS;
    before = assertion_count;
    CHECK(!rts_task_pool_reserve(&pool, &slot_index));
    CHECK(assertion_count == before + 1u);

    before = assertion_count;
    CHECK(rts_task_pool_next_free_hint(&pool) == RTS_TASK_POOL_INVALID_INDEX);
    CHECK(assertion_count == before + 1u);

    rts_task_pool_initialize(&pool);
    CHECK(rts_task_pool_reserve(&pool, &slot_index));
    pool.allocated_count = (size_t)RTS_MAX_TASKS;
    before = assertion_count;
    rts_task_pool_commit(&pool, slot_index);
    CHECK(assertion_count == before + 1u);
    CHECK(pool.slots[slot_index].slot_state == RTS_TASK_SLOT_RESERVED);

    before = assertion_count;
    rts_task_pool_initialize(NULL);
    CHECK(assertion_count == before + 1u);
    before = assertion_count;
    CHECK(!rts_task_pool_reserve(&pool, NULL));
    CHECK(assertion_count == before + 1u);
    before = assertion_count;
    CHECK(rts_task_pool_get(&pool, (size_t)RTS_MAX_TASKS) == NULL);
    CHECK(assertion_count == before + 1u);
}
#endif

int rts_test_task_pool_run(void)
{
    test_initialization_and_first_slot();
    test_capacity_and_commit();
    test_rollback_and_reserved_semantics();
#if RTS_ENABLE_ASSERTIONS
    test_contract_assertions();
#endif
    return test_failures;
}

int main(void)
{
    return rts_test_task_pool_run();
}
