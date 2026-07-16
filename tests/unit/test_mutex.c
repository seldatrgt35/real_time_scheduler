#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mutex_internal.h"
#include "port_internal.h"
#include "priority_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "time_internal.h"
#include "wait_object_internal.h"

#define TEST_STACK_BYTES 256u
#define TEST_TASK_LIMIT  8u

static _Alignas(RTS_TASK_STACK_ALIGNMENT)
    unsigned char test_stacks[TEST_TASK_LIMIT][TEST_STACK_BYTES];
static int test_failures;
static int first_failure_line;
static unsigned int assertion_count;

#define CHECK(condition)                                                   \
    do                                                                     \
    {                                                                      \
        if (!(condition))                                                  \
        {                                                                  \
            ++test_failures;                                               \
            if (first_failure_line == 0)                                   \
            {                                                              \
                first_failure_line = __LINE__;                             \
            }                                                              \
        }                                                                  \
    } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

static void task_entry(void *argument)
{
    (void)argument;
}

static void reset_environment(void)
{
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
}

static rts_task_handle_t create_task(size_t index, rts_priority_t priority)
{
    rts_task_config_t config = {
        task_entry, NULL, test_stacks[index], sizeof test_stacks[index],
        priority, 0u, 0u, 0u
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
}

static void force_current(rts_tcb_t *task)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
    if (kernel->current_task != NULL &&
        kernel->current_task->state == RTS_TASK_STATE_RUNNING)
    {
        kernel->current_task->state = RTS_TASK_STATE_READY;
    }
    CHECK(task->state == RTS_TASK_STATE_READY);
    task->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = task;
    CHECK(rts_scheduler_current_is_valid());
}

static void complete_pending_switch(void)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_kernel_state_get()->switch_plan.pending);
    if (rts_host_port_test_switch_request_pending())
    {
        rts_host_port_test_consume_switch_request();
    }
    CHECK(rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_switch_complete(&snapshot);
}

static bool advance_from_isr(rts_tick_t elapsed)
{
    bool notify;

    rts_host_port_test_set_isr(true);
    notify = rts_kernel_tick_advance(elapsed);
    if (notify)
    {
        rts_port_request_context_switch();
    }
    rts_host_port_test_set_isr(false);
    return notify;
}

static void test_initialization_uncontended_and_misuse(void)
{
    rts_mutex_t mutex = {0};
    rts_task_handle_t owner;
    rts_task_handle_t other;

    reset_environment();
    CHECK(rts_mutex_init(NULL) == RTS_STATUS_INVALID_ARGUMENT);
    rts_host_port_test_set_isr(true);
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_INVALID_CONTEXT);
    rts_host_port_test_set_isr(false);
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_OK);
    CHECK(rts_mutex_is_valid(&mutex));
    CHECK(mutex.owner == NULL && mutex.waiters.count == 0u);
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_ALREADY_INITIALIZED);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_INVALID_STATE);

    CHECK(rts_init() == RTS_STATUS_OK);
    owner = create_task(0u, 4u);
    other = create_task(1u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_OK);
    CHECK(mutex.owner == owner && owner->owned_mutex_head == &mutex);
    CHECK(owner->priority == owner->base_priority);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_INVALID_STATE);
    force_current(other);
    CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_INVALID_STATE);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_TIMEOUT);
    CHECK(mutex.owner == owner && mutex.waiters.count == 0u);
    force_current(owner);
    CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_OK);
    CHECK(mutex.owner == NULL && owner->owned_mutex_head == NULL);

    rts_host_port_test_set_isr(true);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_INVALID_CONTEXT);
    CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_INVALID_CONTEXT);
    rts_host_port_test_set_isr(false);
}

static void test_basic_inheritance_and_handoff(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_mutex_t mutex = {0};
    rts_task_handle_t high;
    rts_task_handle_t medium;
    rts_task_handle_t low;

    reset_environment();
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 5u);
    medium = create_task(1u, 3u);
    low = create_task(2u, 1u);
    CHECK(rts_start() == RTS_STATUS_OK);
    force_current(low);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_OK);
    force_current(high);

    CHECK(rts_mutex_lock(&mutex, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    CHECK(high->state == RTS_TASK_STATE_BLOCKED);
    CHECK(high->wait.reason == RTS_WAIT_MUTEX);
    CHECK(low->base_priority == 1u && low->priority == 5u);
    CHECK(rts_ready_contains(&kernel->ready_set, low));
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == low);
    CHECK(medium->priority == 3u);
    complete_pending_switch();
    CHECK(kernel->current_task == low);

    CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_OK);
    CHECK(mutex.owner == high);
    CHECK(high->wait.result == RTS_WAIT_RESULT_ACQUIRED);
    CHECK(high->wait_node.owner == NULL && high->delay_node.owner == NULL);
    CHECK(low->priority == low->base_priority && low->priority == 1u);
    CHECK(kernel->switch_plan.to == high);
    complete_pending_switch();
    CHECK(kernel->current_task == high);
    CHECK(rts_mutex_wait_result_consume(high) == RTS_STATUS_OK);
}

static void test_multiple_owned_mutex_restoration(void)
{
    rts_mutex_t first = {0};
    rts_mutex_t second = {0};
    rts_task_handle_t high5;
    rts_task_handle_t high4;
    rts_task_handle_t low;

    reset_environment();
    CHECK(rts_mutex_init(&first) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&second) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    high5 = create_task(0u, 5u);
    high4 = create_task(1u, 4u);
    low = create_task(2u, 1u);
    CHECK(rts_start() == RTS_STATUS_OK);
    force_current(low);
    CHECK(rts_mutex_lock(&first, 0u) == RTS_STATUS_OK);
    CHECK(rts_mutex_lock(&second, 0u) == RTS_STATUS_OK);
    force_current(high5);
    CHECK(rts_mutex_lock(&first, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    force_current(high4);
    CHECK(rts_mutex_lock(&second, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(low->priority == 5u);

    CHECK(rts_mutex_unlock(&first) == RTS_STATUS_OK);
    CHECK(first.owner == high5 && low->priority == 4u);
    complete_pending_switch();
    force_current(low);
    CHECK(rts_mutex_unlock(&second) == RTS_STATUS_OK);
    CHECK(second.owner == high4 && low->priority == low->base_priority);
}

static void test_timeout_restores_owner(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_mutex_t mutex = {0};
    rts_task_handle_t high;
    rts_task_handle_t low;

    reset_environment();
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 6u);
    low = create_task(1u, 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    force_current(low);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_OK);
    force_current(high);
    kernel->current_tick = UINT32_C(0xfffffffe);
    CHECK(rts_mutex_lock(&mutex, 3u) == RTS_STATUS_OK);
    CHECK(low->priority == 6u);
    complete_pending_switch();
    CHECK(kernel->current_task == low);
    CHECK(advance_from_isr(3u));
    CHECK(kernel->current_tick == 1u);
    CHECK(high->wait.result == RTS_WAIT_RESULT_TIMEOUT);
    CHECK(high->wait_node.owner == NULL && high->delay_node.owner == NULL);
    CHECK(mutex.owner == low && mutex.waiters.count == 0u);
    CHECK(low->priority == low->base_priority);
    complete_pending_switch();
    CHECK(kernel->current_task == high);
    CHECK(rts_mutex_wait_result_consume(high) == RTS_STATUS_TIMEOUT);
}

static void test_transitive_inheritance(void)
{
    rts_mutex_t first = {0};
    rts_mutex_t second = {0};
    rts_task_handle_t high;
    rts_task_handle_t medium;
    rts_task_handle_t low;

    reset_environment();
    CHECK(rts_mutex_init(&first) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&second) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    high = create_task(0u, 6u);
    medium = create_task(1u, 3u);
    low = create_task(2u, 1u);
    CHECK(rts_start() == RTS_STATUS_OK);
    force_current(low);
    CHECK(rts_mutex_lock(&second, 0u) == RTS_STATUS_OK);
    force_current(medium);
    CHECK(rts_mutex_lock(&first, 0u) == RTS_STATUS_OK);
    CHECK(rts_mutex_lock(&second, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    complete_pending_switch();
    force_current(high);
    CHECK(rts_mutex_lock(&first, RTS_WAIT_FOREVER) == RTS_STATUS_OK);
    CHECK(medium->priority == 6u && low->priority == 6u);
    CHECK(second.waiters.head == medium);
    CHECK(rts_kernel_state_get()->switch_plan.to == low);
    complete_pending_switch();

    CHECK(rts_mutex_unlock(&second) == RTS_STATUS_OK);
    CHECK(second.owner == medium && low->priority == 1u);
    CHECK(medium->priority == 6u);
    complete_pending_switch();
    CHECK(rts_kernel_state_get()->current_task == medium);
    CHECK(rts_mutex_wait_result_consume(medium) == RTS_STATUS_OK);
    CHECK(rts_mutex_unlock(&first) == RTS_STATUS_OK);
    CHECK(first.owner == high && medium->priority == medium->base_priority);
}

static void test_cycle_detection_is_bounded(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_mutex_t first = {0};
    rts_mutex_t second = {0};
    rts_task_handle_t a;
    rts_task_handle_t b;
    unsigned int before;

    CHECK(assertion_count == 0u);
    reset_environment();
    CHECK(rts_mutex_init(&first) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&second) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(0u, 4u);
    b = create_task(1u, 3u);
    CHECK(rts_start() == RTS_STATUS_OK);
    rts_ready_remove(&kernel->ready_set, a);
    rts_ready_remove(&kernel->ready_set, b);
    a->state = RTS_TASK_STATE_BLOCKED;
    b->state = RTS_TASK_STATE_BLOCKED;
    first.owner = a;
    a->owned_mutex_head = &first;
    a->owned_mutex_tail = &first;
    second.owner = b;
    b->owned_mutex_head = &second;
    b->owned_mutex_tail = &second;
    a->wait.reason = RTS_WAIT_MUTEX;
    a->wait.object = &second;
    b->wait.reason = RTS_WAIT_MUTEX;
    b->wait.object = &first;
    rts_wait_object_insert(&second.waiters, a);
    rts_wait_object_insert(&first.waiters, b);
    before = assertion_count;
    CHECK(!rts_priority_recompute_chain(a));
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count > before);
#else
    (void)before;
#endif
}

static void test_mutex_stress(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_mutex_t first = {0};
    rts_mutex_t second = {0};
    rts_task_handle_t task;
    size_t index;

    reset_environment();
    CHECK(rts_mutex_init(&first) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&second) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    task = create_task(0u, 4u);
    CHECK(rts_start() == RTS_STATUS_OK);
    for (index = 0u; index < 1000u; ++index)
    {
        CHECK(rts_mutex_lock(&first, 0u) == RTS_STATUS_OK);
        CHECK(rts_mutex_lock(&second, 0u) == RTS_STATUS_OK);
        CHECK(task->owned_mutex_count == 2u);
        CHECK(rts_mutex_unlock(&first) == RTS_STATUS_OK);
        CHECK(task->owned_mutex_count == 1u && second.owner == task);
        CHECK(rts_mutex_unlock(&second) == RTS_STATUS_OK);
        CHECK(task->owned_mutex_count == 0u);
        CHECK(task->priority == task->base_priority);
        CHECK(rts_scheduler_current_is_valid());
        CHECK(!kernel->switch_plan.pending && !kernel->switch_plan.active);
        CHECK(first.owner == NULL && second.owner == NULL);
    }
}

int main(void)
{
    test_initialization_uncontended_and_misuse();
    test_basic_inheritance_and_handoff();
    test_multiple_owned_mutex_restoration();
    test_timeout_restores_owner();
    test_transitive_inheritance();
    test_cycle_detection_is_bounded();
    test_mutex_stress();
    return test_failures == 0 ? 0 : first_failure_line;
}
