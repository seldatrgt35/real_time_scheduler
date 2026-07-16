#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fatal_internal.h"
#include "invariant_check_internal.h"
#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_mutex.h"
#include "rts/rts_semaphore.h"
#include "scheduler_internal.h"
#include "timer_internal.h"
#include "trace_internal.h"

static int test_failures;
static int first_failure_line;
static unsigned int assertion_count;
static unsigned int callback_count;
static unsigned int callback_order[64];
static unsigned int callback_identity_values[64];
static size_t callback_order_count;
static rts_timer_handle_t self_timer;
static rts_timer_handle_t controlled_timer;
static rts_status_t callback_status;
static rts_semaphore_t callback_semaphore;
static rts_mutex_t callback_mutex;

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

static void record_callback(void *argument)
{
    const unsigned int *identity = argument;
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    CHECK(!rts_port_is_in_isr());
    CHECK(rts_host_port_test_critical_depth() == 0u);
    CHECK(kernel->current_task == kernel->timer_service_task);
    if (callback_order_count < 64u)
    {
        callback_order[callback_order_count] = *identity;
        ++callback_order_count;
    }
    ++callback_count;
}

static void dummy_task_entry(void *argument)
{
    (void)argument;
}

static void self_stop_callback(void *argument)
{
    (void)argument;
    ++callback_count;
    callback_status = rts_timer_stop(self_timer);
}

static void self_restart_callback(void *argument)
{
    (void)argument;
    ++callback_count;
    callback_status = rts_timer_restart(self_timer);
}

static void control_other_callback(void *argument)
{
    (void)argument;
    ++callback_count;
    callback_status = rts_timer_restart(controlled_timer);
}

static void synchronization_callback(void *argument)
{
    (void)argument;
    ++callback_count;
    CHECK(rts_semaphore_give(&callback_semaphore) == RTS_STATUS_OK);
    callback_status = rts_task_delay(1u);
}

static void mutex_callback(void *argument)
{
    (void)argument;
    ++callback_count;
    callback_status = rts_mutex_lock(&callback_mutex, 0u);
    if (callback_status == RTS_STATUS_OK)
    {
        callback_status = rts_mutex_unlock(&callback_mutex);
    }
}

static void reset_environment(void)
{
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    rts_fatal_record_reset_for_test();
    rts_trace_reset_for_test();
    assertion_count = 0u;
    callback_count = 0u;
    callback_order_count = 0u;
    self_timer = NULL;
    controlled_timer = NULL;
    callback_status = RTS_STATUS_OK;
    callback_semaphore = (rts_semaphore_t){0};
    callback_mutex = (rts_mutex_t){0};
}

static rts_timer_handle_t create_timer_with_callback(
    rts_tick_t period,
    rts_timer_mode_t mode,
    rts_timer_callback_t callback,
    void *argument)
{
    rts_timer_config_t config = {period, callback, argument, mode};
    rts_timer_handle_t timer = NULL;

    CHECK(rts_timer_init(&config, &timer) == RTS_STATUS_OK);
    CHECK(timer != NULL);
    return timer;
}

static rts_timer_handle_t create_timer(rts_tick_t period,
                                       rts_timer_mode_t mode,
                                       unsigned int identity)
{
    CHECK(identity < 64u);
    callback_identity_values[identity] = identity;
    return create_timer_with_callback(period, mode, record_callback,
                                      &callback_identity_values[identity]);
}

static bool advance_tick(rts_tick_t elapsed)
{
    bool notify;

    rts_host_port_test_set_isr(true);
    notify = rts_kernel_tick_advance(elapsed);
    rts_host_port_test_set_isr(false);
    return notify;
}

static void complete_pending_switch(void)
{
    rts_switch_snapshot_t snapshot;

    CHECK(rts_scheduler_switch_acquire(&snapshot));
    rts_scheduler_switch_complete(&snapshot);
}

static size_t run_timer_service(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t processed;

    if (kernel->current_task != kernel->timer_service_task)
    {
        CHECK(kernel->switch_plan.pending);
        complete_pending_switch();
    }
    CHECK(kernel->current_task == kernel->timer_service_task);
    processed = rts_timer_service_drain();
    if (rts_timer_manager_get()->callback_queue.count == 0u)
    {
        rts_critical_token_t token = rts_port_critical_enter();

        (void)rts_scheduler_timer_service_block();
        rts_port_critical_exit(token);
        CHECK(kernel->switch_plan.pending);
        complete_pending_switch();
    }
    return processed;
}

static void test_bootstrap_validation_and_capacity(void)
{
    rts_timer_handle_t handles[RTS_MAX_TIMERS];
    rts_timer_handle_t extra = (rts_timer_handle_t)(uintptr_t)1u;
    rts_timer_config_t valid = {
        10u, record_callback, NULL, RTS_TIMER_ONE_SHOT
    };
    rts_timer_config_t invalid = valid;
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t index;

    reset_environment();
    CHECK(rts_timer_init(&valid, &extra) == RTS_STATUS_INVALID_STATE);
    CHECK(extra == NULL);
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(kernel->timer_service_task == &kernel->timer_service_task_storage);
    CHECK(kernel->timer_service_task->state == RTS_TASK_STATE_BLOCKED);
    CHECK(kernel->timer_service_task->wait.reason == RTS_WAIT_TIMER_SERVICE);
    CHECK(!rts_ready_contains(&kernel->ready_set,
                              kernel->timer_service_task));
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 0u);
    CHECK(rts_timer_manager_get()->callback_queue.count == 0u);
    CHECK(rts_timer_init(NULL, &extra) == RTS_STATUS_INVALID_ARGUMENT);
    invalid.callback = NULL;
    CHECK(rts_timer_init(&invalid, &extra) == RTS_STATUS_INVALID_ARGUMENT);
    invalid = valid;
    invalid.period = 0u;
    CHECK(rts_timer_init(&invalid, &extra) == RTS_STATUS_INVALID_ARGUMENT);
    invalid.period = RTS_DELAY_MAX + 1u;
    CHECK(rts_timer_init(&invalid, &extra) == RTS_STATUS_INVALID_ARGUMENT);
    invalid = valid;
    invalid.mode = UINT8_MAX;
    CHECK(rts_timer_init(&invalid, &extra) == RTS_STATUS_INVALID_ARGUMENT);

    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        handles[index] = create_timer((rts_tick_t)index + 1u,
                                      RTS_TIMER_ONE_SHOT,
                                      (unsigned int)index);
        CHECK(rts_timer_handle_is_valid(handles[index]));
    }
    CHECK(rts_timer_allocated_count() == (size_t)RTS_MAX_TIMERS);
    CHECK(rts_timer_init(&valid, &extra) ==
          RTS_STATUS_CAPACITY_EXHAUSTED);
    CHECK(extra == NULL);
    CHECK(rts_timer_manager_validate(rts_timer_manager_get()));
}

static void test_one_shot_ordering_and_context(void)
{
    rts_timer_handle_t early;
    rts_timer_handle_t equal_a;
    rts_timer_handle_t equal_b;
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    early = create_timer(2u, RTS_TIMER_ONE_SHOT, 1u);
    equal_a = create_timer(5u, RTS_TIMER_ONE_SHOT, 2u);
    equal_b = create_timer(5u, RTS_TIMER_ONE_SHOT, 3u);
    CHECK(rts_timer_start(equal_a) == RTS_STATUS_OK);
    CHECK(rts_timer_start(early) == RTS_STATUS_OK);
    CHECK(rts_timer_start(equal_b) == RTS_STATUS_OK);
    CHECK(rts_timer_start(early) == RTS_STATUS_INVALID_STATE);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(advance_tick(2u));
    CHECK(callback_count == 0u);
    CHECK(early->state == RTS_TIMER_STOPPED);
    CHECK(early->callback_state == RTS_TIMER_CALLBACK_PENDING);
    CHECK(kernel->timer_service_task->state == RTS_TASK_STATE_READY);
    CHECK(run_timer_service() == 1u);
    CHECK(callback_count == 1u && callback_order[0] == 1u);
    CHECK(early->callback_state == RTS_TIMER_CALLBACK_IDLE);
    CHECK(advance_tick(3u));
    CHECK(run_timer_service() == 2u);
    CHECK(callback_order_count == 3u);
    CHECK(callback_order[1] == 2u && callback_order[2] == 3u);
    CHECK(equal_a->state == RTS_TIMER_STOPPED);
    CHECK(equal_b->state == RTS_TIMER_STOPPED);
    CHECK(rts_kernel_validate_all());
}

static void test_periodic_coalescing_and_overrun(void)
{
    rts_timer_handle_t periodic;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    periodic = create_timer(10u, RTS_TIMER_PERIODIC, 7u);
    CHECK(rts_timer_start(periodic) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(advance_tick(35u));
    CHECK(periodic->state == RTS_TIMER_ACTIVE);
    CHECK(periodic->expiration_tick == 40u);
    CHECK(periodic->callback_state == RTS_TIMER_CALLBACK_PENDING);
    CHECK(rts_timer_manager_get()->callback_queue.count == 1u);
#if RTS_ENABLE_RUNTIME_STATS
    CHECK(periodic->diagnostic_missed_period_count == 2u);
#endif
    CHECK(!advance_tick(5u));
    CHECK(periodic->expiration_tick == 50u);
    CHECK(rts_timer_manager_get()->callback_queue.count == 1u);
#if RTS_ENABLE_RUNTIME_STATS
    CHECK(periodic->diagnostic_overrun_count == 1u);
#endif
    CHECK(run_timer_service() == 1u);
    CHECK(callback_count == 1u);
    CHECK(periodic->state == RTS_TIMER_ACTIVE);
    CHECK(rts_timer_stop(periodic) == RTS_STATUS_OK);
    CHECK(!advance_tick(20u));
    CHECK(callback_count == 1u);
}

static void test_generation_stop_restart_and_wrap(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_handle_t timer;
    uint32_t generation;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_MAX - 2u;
    timer = create_timer(5u, RTS_TIMER_ONE_SHOT, 9u);
    CHECK(rts_timer_start(timer) == RTS_STATUS_OK);
    CHECK(timer->expiration_tick == 2u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(advance_tick(5u));
    generation = timer->generation;
    CHECK(rts_timer_stop(timer) == RTS_STATUS_OK);
    CHECK(timer->generation == generation + 1u);
    CHECK(timer->callback_state == RTS_TIMER_CALLBACK_IDLE);
    CHECK(rts_timer_manager_get()->callback_queue.count == 0u);
    CHECK(kernel->timer_service_task->state == RTS_TASK_STATE_BLOCKED);
    CHECK(!kernel->switch_plan.pending);
    CHECK(callback_count == 0u);
    CHECK(rts_timer_restart(timer) == RTS_STATUS_OK);
    CHECK(timer->expiration_tick == 7u);
    CHECK(advance_tick(5u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_count == 1u);
}

static void test_wrap_boundary_order(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_handle_t timers[4];

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_C(0xfffffff0);
    timers[0] = create_timer(14u, RTS_TIMER_ONE_SHOT, 30u);
    timers[1] = create_timer(15u, RTS_TIMER_ONE_SHOT, 31u);
    timers[2] = create_timer(16u, RTS_TIMER_ONE_SHOT, 32u);
    timers[3] = create_timer(32u, RTS_TIMER_ONE_SHOT, 33u);
    CHECK(rts_timer_start(timers[0]) == RTS_STATUS_OK);
    CHECK(rts_timer_start(timers[1]) == RTS_STATUS_OK);
    CHECK(rts_timer_start(timers[2]) == RTS_STATUS_OK);
    CHECK(rts_timer_start(timers[3]) == RTS_STATUS_OK);
    CHECK(timers[0]->expiration_tick == UINT32_C(0xfffffffe));
    CHECK(timers[1]->expiration_tick == UINT32_C(0xffffffff));
    CHECK(timers[2]->expiration_tick == UINT32_C(0));
    CHECK(timers[3]->expiration_tick == UINT32_C(0x10));
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(advance_tick(32u));
    CHECK(run_timer_service() == 4u);
    CHECK(callback_order_count == 4u);
    CHECK(callback_order[0] == 30u && callback_order[1] == 31u &&
          callback_order[2] == 32u && callback_order[3] == 33u);
}

static void test_self_control_and_synchronization(void)
{
    rts_timer_handle_t self_stop;
    rts_timer_handle_t self_restart;
    rts_timer_handle_t control_trigger;
    rts_timer_handle_t synchronization_trigger;
    rts_timer_handle_t mutex_trigger;

    reset_environment();
    CHECK(rts_semaphore_init(&callback_semaphore, 0u, 1u) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&callback_mutex) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    self_stop = create_timer_with_callback(
        3u, RTS_TIMER_PERIODIC, self_stop_callback, NULL);
    self_restart = create_timer_with_callback(
        3u, RTS_TIMER_PERIODIC, self_restart_callback, NULL);
    controlled_timer = create_timer(20u, RTS_TIMER_ONE_SHOT, 12u);
    control_trigger = create_timer_with_callback(
        1u, RTS_TIMER_ONE_SHOT, control_other_callback, NULL);
    synchronization_trigger = create_timer_with_callback(
        1u, RTS_TIMER_ONE_SHOT, synchronization_callback, NULL);
    mutex_trigger = create_timer_with_callback(
        1u, RTS_TIMER_ONE_SHOT, mutex_callback, NULL);
    self_timer = self_stop;
    CHECK(rts_timer_start(self_stop) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(advance_tick(3u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_status == RTS_STATUS_OK);
    CHECK(self_stop->state == RTS_TIMER_STOPPED);
    CHECK(!advance_tick(9u));
    CHECK(callback_count == 1u);

    self_timer = self_restart;
    CHECK(rts_timer_start(self_restart) == RTS_STATUS_OK);
    CHECK(advance_tick(3u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_status == RTS_STATUS_OK);
    CHECK(self_restart->state == RTS_TIMER_ACTIVE);
    CHECK(self_restart->expiration_tick == rts_kernel_tick_now() + 3u);
    CHECK(rts_timer_stop(self_restart) == RTS_STATUS_OK);

    CHECK(rts_timer_start(control_trigger) == RTS_STATUS_OK);
    CHECK(advance_tick(1u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_status == RTS_STATUS_OK);
    CHECK(controlled_timer->state == RTS_TIMER_ACTIVE);
    CHECK(rts_timer_stop(controlled_timer) == RTS_STATUS_OK);

    CHECK(rts_timer_start(synchronization_trigger) == RTS_STATUS_OK);
    CHECK(advance_tick(1u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_status == RTS_STATUS_INVALID_CONTEXT);
    CHECK(callback_semaphore.count == 1u);
    CHECK(rts_timer_start(mutex_trigger) == RTS_STATUS_OK);
    CHECK(advance_tick(1u));
    CHECK(run_timer_service() == 1u);
    CHECK(callback_status == RTS_STATUS_OK);
    CHECK(callback_mutex.owner == NULL);
    CHECK(rts_kernel_validate_all());
}

static void test_callback_queue_capacity(void)
{
    rts_timer_callback_queue_t queue;
    struct rts_timer fake[RTS_TIMER_CALLBACK_QUEUE_CAPACITY + 1u];
    size_t index;

    rts_timer_callback_queue_initialize(&queue);
    for (index = 0u;
         index < (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY; ++index)
    {
        rts_timer_callback_work_t work = {
            &fake[index], (rts_tick_t)index, 1u, (uint32_t)index
        };
        CHECK(rts_timer_callback_queue_enqueue(&queue, &work));
    }
    CHECK(queue.count == (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY);
    {
        rts_timer_callback_work_t overflow = {
            &fake[RTS_TIMER_CALLBACK_QUEUE_CAPACITY], 0u, 1u, 0u
        };
        CHECK(!rts_timer_callback_queue_enqueue(&queue, &overflow));
    }
    CHECK(queue.count == (size_t)RTS_TIMER_CALLBACK_QUEUE_CAPACITY);
    CHECK(rts_timer_callback_queue_validate(&queue));
}

static void test_simultaneous_delay_timer_preemption(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char high_stack[512u];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char low_stack[512u];
    const rts_task_config_t high_config = {
        dummy_task_entry, NULL, high_stack, sizeof(high_stack),
        (rts_priority_t)(RTS_TIMER_SERVICE_PRIORITY + 1u), 0u, 0u, 0u
    };
    const rts_task_config_t low_config = {
        dummy_task_entry, NULL, low_stack, sizeof(low_stack), 1u,
        0u, 0u, 0u
    };
    rts_task_handle_t high = NULL;
    rts_task_handle_t low = NULL;
    rts_timer_handle_t timer;
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    timer = create_timer(5u, RTS_TIMER_ONE_SHOT, 21u);
    CHECK(rts_timer_start(timer) == RTS_STATUS_OK);
    CHECK(rts_task_create(&high_config, &high) == RTS_STATUS_OK);
    CHECK(rts_task_create(&low_config, &low) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == high);
    CHECK(rts_task_delay(5u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == low);

    CHECK(advance_tick(5u));
    CHECK(kernel->switch_plan.pending);
    CHECK(kernel->switch_plan.to == high);
    CHECK(kernel->timer_service_task->state == RTS_TASK_STATE_READY);
    CHECK(rts_timer_manager_get()->callback_queue.count == 1u);
    CHECK(callback_count == 0u);
    complete_pending_switch();
    CHECK(kernel->current_task == high);

    CHECK(rts_task_delay(1u) == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == kernel->timer_service_task);
    CHECK(run_timer_service() == 1u);
    CHECK(callback_count == 1u);
    CHECK(kernel->current_task == low);
    CHECK(rts_kernel_validate_all());
}

static void test_deterministic_stress(void)
{
    rts_timer_manager_t *manager;
    rts_timer_handle_t timers[RTS_MAX_TIMERS];
    uint32_t random = UINT32_C(0x10b5c0de);
    size_t index;
    size_t event;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    manager = rts_timer_manager_get();
    for (index = 0u; index < (size_t)RTS_MAX_TIMERS; ++index)
    {
        timers[index] = create_timer((rts_tick_t)(index + 1u),
                                     (index & 1u) == 0u
                                         ? RTS_TIMER_ONE_SHOT
                                         : RTS_TIMER_PERIODIC,
                                     (unsigned int)index);
    }
    CHECK(rts_start() == RTS_STATUS_OK);

    for (event = 0u; event < 50000u; ++event)
    {
        struct rts_timer *timer;
        uint32_t operation;

        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        index = (size_t)(random % (uint32_t)RTS_MAX_TIMERS);
        operation = (random >> 16u) & UINT32_C(3);
        timer = timers[index];
        if (operation == 0u)
        {
            (void)advance_tick((rts_tick_t)((random & 7u) + 1u));
            if (manager->callback_queue.count != 0u)
            {
                (void)run_timer_service();
            }
        }
        else if (operation == 1u)
        {
            if (timer->state == RTS_TIMER_ACTIVE ||
                timer->callback_state != RTS_TIMER_CALLBACK_IDLE)
            {
                CHECK(rts_timer_stop(timer) == RTS_STATUS_OK);
            }
            else
            {
                CHECK(rts_timer_start(timer) == RTS_STATUS_OK);
            }
        }
        else if (operation == 2u)
        {
            CHECK(rts_timer_restart(timer) == RTS_STATUS_OK);
        }
        else
        {
            CHECK(rts_timer_is_running(timer) ==
                  (timer->state == RTS_TIMER_ACTIVE));
        }
        CHECK(rts_timer_manager_validate(manager));
        CHECK(rts_kernel_validate_all());
    }
    if (manager->callback_queue.count != 0u)
    {
        (void)run_timer_service();
    }
    CHECK(assertion_count == 0u);
}

int main(void)
{
    test_bootstrap_validation_and_capacity();
    test_one_shot_ordering_and_context();
    test_periodic_coalescing_and_overrun();
    test_generation_stop_restart_and_wrap();
    test_wrap_boundary_order();
    test_self_control_and_synchronization();
    test_callback_queue_capacity();
    test_simultaneous_delay_timer_preemption();
    test_deterministic_stress();
    return test_failures == 0 ? 0 : first_failure_line;
}
