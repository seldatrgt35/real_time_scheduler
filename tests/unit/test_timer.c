#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fatal_internal.h"
#include "invariant_check_internal.h"
#include "port_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "timer_internal.h"
#include "trace_internal.h"

static int test_failures;
static int first_failure_line;
static unsigned int assertion_count;
static unsigned int callback_count;

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

static void timer_callback(void *argument)
{
    (void)argument;
    ++callback_count;
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
}

static rts_timer_handle_t create_timer(rts_tick_t period,
                                       rts_timer_mode_t mode)
{
    rts_timer_config_t config = {
        period, timer_callback, NULL, mode
    };
    rts_timer_handle_t timer = NULL;

    CHECK(rts_timer_init(&config, &timer) == RTS_STATUS_OK);
    CHECK(timer != NULL);
    return timer;
}

static bool advance_tick(rts_tick_t elapsed)
{
    bool notify;

    rts_host_port_test_set_isr(true);
    notify = rts_kernel_tick_advance(elapsed);
    rts_host_port_test_set_isr(false);
    return notify;
}

static void test_validation_registration_and_capacity(void)
{
    rts_timer_handle_t handles[RTS_MAX_TIMERS];
    rts_timer_handle_t extra = (rts_timer_handle_t)(uintptr_t)1u;
    rts_timer_config_t valid = {
        10u, timer_callback, NULL, RTS_TIMER_ONE_SHOT
    };
    rts_timer_config_t invalid = valid;
    size_t index;

    reset_environment();
    CHECK(rts_timer_init(&valid, &extra) == RTS_STATUS_INVALID_STATE);
    CHECK(extra == NULL);
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_timer_start((rts_timer_handle_t)(uintptr_t)1u) ==
          RTS_STATUS_INVALID_ARGUMENT);
    CHECK(!rts_timer_is_running((rts_timer_handle_t)(uintptr_t)1u));
    CHECK(rts_timer_init(NULL, &extra) == RTS_STATUS_INVALID_ARGUMENT);
    CHECK(rts_timer_init(&valid, NULL) == RTS_STATUS_INVALID_ARGUMENT);
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
                                      (index & 1u) == 0u
                                          ? RTS_TIMER_ONE_SHOT
                                          : RTS_TIMER_PERIODIC);
        CHECK(rts_timer_handle_is_valid(handles[index]));
    }
    CHECK(rts_timer_allocated_count() == (size_t)RTS_MAX_TIMERS);
    extra = (rts_timer_handle_t)(uintptr_t)1u;
    CHECK(rts_timer_init(&valid, &extra) ==
          RTS_STATUS_CAPACITY_EXHAUSTED);
    CHECK(extra == NULL);
    CHECK(rts_timer_manager_validate(rts_timer_manager_get()));
}

static void test_state_machine_ordering_and_expiration(void)
{
    rts_timer_handle_t slow;
    rts_timer_handle_t periodic;
    rts_timer_handle_t equal;
    rts_timer_handle_t cancelled;
    const rts_list_node_t *node;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    slow = create_timer(5u, RTS_TIMER_ONE_SHOT);
    periodic = create_timer(2u, RTS_TIMER_PERIODIC);
    equal = create_timer(5u, RTS_TIMER_ONE_SHOT);
    cancelled = create_timer(1u, RTS_TIMER_ONE_SHOT);
    CHECK(slow->state == RTS_TIMER_STOPPED);
    CHECK(rts_timer_start(slow) == RTS_STATUS_OK);
    CHECK(rts_timer_start(periodic) == RTS_STATUS_OK);
    CHECK(rts_timer_start(equal) == RTS_STATUS_OK);
    CHECK(rts_timer_start(cancelled) == RTS_STATUS_OK);
    CHECK(rts_timer_stop(cancelled) == RTS_STATUS_OK);
    CHECK(rts_timer_start(slow) == RTS_STATUS_INVALID_STATE);
    CHECK(rts_timer_is_running(slow));

    node = rts_timer_manager_get()->running_queue.ordered_timers.head;
    CHECK(node != NULL && node->object == periodic);
    node = node == NULL ? NULL : node->next;
    CHECK(node != NULL && node->object == slow);
    node = node == NULL ? NULL : node->next;
    CHECK(node != NULL && node->object == equal);

    CHECK(rts_timer_stop(equal) == RTS_STATUS_OK);
    CHECK(!rts_timer_is_running(equal));
    CHECK(equal->state == RTS_TIMER_STOPPED);
    CHECK(rts_timer_stop(equal) == RTS_STATUS_INVALID_STATE);
    CHECK(rts_timer_restart(equal) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);

    CHECK(!advance_tick(2u));
    CHECK(periodic->state == RTS_TIMER_EXPIRED);
    CHECK(periodic->last_expiration_tick == 2u);
    CHECK(periodic->expiration_tick == 4u);
    CHECK(!rts_timer_is_running(periodic));
    CHECK(callback_count == 0u);
    CHECK(rts_timer_restart(periodic) == RTS_STATUS_OK);
    CHECK(periodic->expiration_tick == 4u);

    CHECK(!advance_tick(2u));
    CHECK(periodic->state == RTS_TIMER_EXPIRED);
    CHECK(slow->state == RTS_TIMER_RUNNING);
    CHECK(equal->state == RTS_TIMER_RUNNING);
    CHECK(!advance_tick(1u));
    CHECK(slow->state == RTS_TIMER_EXPIRED);
    CHECK(equal->state == RTS_TIMER_EXPIRED);
    CHECK(cancelled->state == RTS_TIMER_STOPPED);
    CHECK(callback_count == 0u);
    CHECK(rts_timer_manager_get()->running_queue.ordered_timers.count == 0u);
    CHECK(rts_kernel_validate_all());
#if RTS_ENABLE_RUNTIME_STATS
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    CHECK(periodic->diagnostic_expiration_count == 2u);
    CHECK(periodic->diagnostic_restart_count == 1u);
    CHECK(kernel->runtime_counters.timer_expirations == 4u);
#endif
#if RTS_ENABLE_TRACE
    CHECK(rts_trace_count() > 0u);
#else
    CHECK(rts_trace_count() == 0u);
#endif
}

static void test_restart_cancellation_wrap_and_context(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_timer_handle_t timer;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    kernel->current_tick = UINT32_MAX - 2u;
    timer = create_timer(5u, RTS_TIMER_ONE_SHOT);
    CHECK(rts_timer_start(timer) == RTS_STATUS_OK);
    CHECK(timer->expiration_tick == 2u);
    CHECK(rts_timer_restart(timer) == RTS_STATUS_OK);
    CHECK(rts_timer_stop(timer) == RTS_STATUS_OK);
    CHECK(rts_timer_restart(timer) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(!advance_tick(4u));
    CHECK(timer->state == RTS_TIMER_RUNNING);
    CHECK(!advance_tick(1u));
    CHECK(timer->state == RTS_TIMER_EXPIRED);
    CHECK(callback_count == 0u);

    rts_host_port_test_set_isr(true);
    CHECK(rts_timer_start(timer) == RTS_STATUS_INVALID_CONTEXT);
    CHECK(rts_timer_stop(timer) == RTS_STATUS_INVALID_CONTEXT);
    CHECK(rts_timer_restart(timer) == RTS_STATUS_INVALID_CONTEXT);
    CHECK(!rts_timer_is_running(timer));
    rts_host_port_test_set_isr(false);
    CHECK(rts_timer_init(&(rts_timer_config_t){
              1u, timer_callback, NULL, RTS_TIMER_ONE_SHOT
          }, &timer) == RTS_STATUS_INVALID_STATE);
}

static void test_invariant_fault_injection(void)
{
    rts_timer_manager_t *manager;
    rts_timer_handle_t first;
    rts_timer_handle_t second;
    rts_tick_t saved;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    manager = rts_timer_manager_get();
    first = create_timer(2u, RTS_TIMER_ONE_SHOT);
    second = create_timer(4u, RTS_TIMER_ONE_SHOT);
    CHECK(rts_timer_start(first) == RTS_STATUS_OK);
    CHECK(rts_timer_start(second) == RTS_STATUS_OK);
    saved = first->expiration_tick;
    first->expiration_tick = second->expiration_tick + 1u;
#if RTS_ENABLE_INVARIANT_CHECKS
    CHECK(!rts_timer_manager_validate(manager));
#endif
    first->expiration_tick = saved;
    CHECK(rts_timer_manager_validate(manager));
}

static void test_deterministic_stress(void)
{
    rts_timer_manager_t *manager;
    rts_timer_handle_t timers[RTS_MAX_TIMERS];
    uint32_t random = UINT32_C(0x10a5c0de);
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
                                         : RTS_TIMER_PERIODIC);
    }
    CHECK(rts_start() == RTS_STATUS_OK);

    for (event = 0u; event < 20000u; ++event)
    {
        struct rts_timer *timer;
        uint32_t operation;

        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        index = (size_t)(random % (uint32_t)RTS_MAX_TIMERS);
        operation = (random >> 16u) & UINT32_C(3);
        timer = timers[index];
        if (operation == 0u)
        {
            (void)advance_tick((rts_tick_t)((random & 3u) + 1u));
        }
        else if (operation == 1u)
        {
            if (timer->state == RTS_TIMER_RUNNING)
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
                  (timer->state == RTS_TIMER_RUNNING));
        }
        CHECK(rts_timer_manager_validate(manager));
        CHECK(rts_kernel_validate_all());
        CHECK(callback_count == 0u);
    }
    CHECK(assertion_count == 0u);
}

int main(void)
{
    test_validation_registration_and_capacity();
    test_state_machine_ordering_and_expiration();
    test_restart_cancellation_wrap_and_context();
    test_invariant_fault_injection();
    test_deterministic_stress();
    return test_failures == 0 ? 0 : first_failure_line;
}
