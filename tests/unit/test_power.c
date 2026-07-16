#include <stdbool.h>
#include <stdint.h>

#include "intrusive_list.h"
#include "port_internal.h"
#include "power_internal.h"
#include "rts/rts.h"
#include "rts/rts_timer.h"
#include "scheduler_internal.h"
#include "timer_internal.h"

static int failures;
static uint32_t before_count;
static uint32_t after_count;
static rts_tick_t hook_planned;
static rts_tick_t hook_elapsed;
static rts_port_wake_source_t hook_source;
RTS_TASK_STACK_DECLARE(delayed_stack, 512u);

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

void rts_power_prepare_sleep(rts_tick_t planned_ticks)
{
    hook_planned = planned_ticks;
}

void rts_power_before_sleep(rts_tick_t planned_ticks)
{
    ++before_count;
    hook_planned = planned_ticks;
}

void rts_power_resume_from_sleep(rts_tick_t elapsed_ticks,
                                 rts_port_wake_source_t source)
{
    hook_elapsed = elapsed_ticks;
    hook_source = source;
}

void rts_power_after_sleep(rts_tick_t elapsed_ticks,
                           rts_port_wake_source_t source)
{
    ++after_count;
    hook_elapsed = elapsed_ticks;
    hook_source = source;
}

static void reset_running_idle(void)
{
    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    before_count = 0u;
    after_count = 0u;
    hook_planned = 0u;
    hook_elapsed = 0u;
    hook_source = RTS_PORT_WAKE_OTHER;
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_scheduler_current_get() == rts_kernel_state_get()->idle_task);
    CHECK(rts_power_sleep_is_allowed());
}

static void delayed_task_initialize(rts_tcb_t *task,
                                    rts_tick_t wake_tick)
{
    *task = (rts_tcb_t){0};
    rts_list_node_initialize(&task->ready_node);
    rts_list_node_initialize(&task->delay_node);
    task->priority = 1u;
    task->base_priority = 1u;
    task->state = RTS_TASK_STATE_BLOCKED;
    task->slot_state = RTS_TASK_SLOT_ALLOCATED;
    task->wait.reason = RTS_WAIT_DELAY;
    task->wait.wake_tick = wake_tick;
    rts_delay_insert(&rts_kernel_state_get()->delay_queue, task);
}

static void unused_task_entry(void *argument)
{
    (void)argument;
}

static rts_tcb_t *create_task(void)
{
    rts_task_handle_t handle = NULL;
    const rts_task_config_t config = {
        .entry = unused_task_entry,
        .argument = NULL,
        .stack_buffer = delayed_stack,
        .stack_size_bytes = sizeof(delayed_stack),
        .priority = 1u
    };
    rts_tcb_t *task;

    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
    task = handle;
    return task;
}

static void test_maintenance_and_external_wake(void)
{
    rts_power_plan_t plan;

    reset_running_idle();
    CHECK(rts_power_plan_compute(&plan));
    CHECK(plan.sleep_ticks == (rts_tick_t)RTS_TICKLESS_MAX_SLEEP_TICKS);
    CHECK(plan.source == RTS_POWER_DEADLINE_MAINTENANCE);
    rts_host_port_test_set_next_wake(37u, RTS_PORT_WAKE_GPIO);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() == 37u);
    CHECK(before_count == 1u && after_count == 1u);
    CHECK(hook_planned == (rts_tick_t)RTS_TICKLESS_MAX_SLEEP_TICKS);
    CHECK(hook_elapsed == 37u && hook_source == RTS_PORT_WAKE_GPIO);
}

static void test_delay_deadline_and_switch_request(void)
{
    rts_tcb_t *delayed;
    rts_power_plan_t plan;
    rts_kernel_state_t *kernel;

    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    CHECK(rts_init() == RTS_STATUS_OK);
    delayed = create_task();
    CHECK(rts_start() == RTS_STATUS_OK);
    kernel = rts_kernel_state_get();
    CHECK(kernel->current_task == delayed);
    rts_ready_remove(&kernel->ready_set, delayed);
    delayed->state = RTS_TASK_STATE_BLOCKED;
    delayed->wait.reason = RTS_WAIT_DELAY;
    delayed->wait.wake_tick = 11u;
    rts_delay_insert(&kernel->delay_queue, delayed);
    kernel->idle_task->state = RTS_TASK_STATE_RUNNING;
    kernel->current_task = kernel->idle_task;
    CHECK(rts_scheduler_current_is_valid());
    CHECK(rts_power_plan_compute(&plan));
    CHECK(plan.sleep_ticks == 11u);
    CHECK(plan.source == RTS_POWER_DEADLINE_DELAY);
    rts_host_port_test_set_next_wake(11u, RTS_PORT_WAKE_TIMER);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() == 11u);
    CHECK(delayed->state == RTS_TASK_STATE_READY);
    CHECK(rts_ready_contains(&rts_kernel_state_get()->ready_set, delayed));
    CHECK(rts_host_port_test_switch_request_count() == 1u);
}

static void timer_callback(void *argument)
{
    (void)argument;
}

static void test_software_timer_deadline(void)
{
    rts_timer_handle_t timer = NULL;
    const rts_timer_config_t config = {
        .period = 19u,
        .callback = timer_callback,
        .argument = NULL,
        .mode = RTS_TIMER_PERIODIC
    };
    rts_power_plan_t plan;

    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    *rts_kernel_state_get() = (rts_kernel_state_t){0};
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_timer_init(&config, &timer) == RTS_STATUS_OK);
    CHECK(rts_timer_start(timer) == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_power_plan_compute(&plan));
    CHECK(plan.sleep_ticks == 19u);
    CHECK(plan.source == RTS_POWER_DEADLINE_TIMER);
    rts_host_port_test_set_next_wake(19u, RTS_PORT_WAKE_TIMER);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() == 19u);
    CHECK(timer->expiration_tick == 38u);
    CHECK(timer->callback_state == RTS_TIMER_CALLBACK_PENDING);
}

static void test_zero_one_wrap_and_long_sleep(void)
{
    rts_tcb_t delayed;
    rts_power_plan_t plan;

    reset_running_idle();
    rts_host_port_test_set_next_wake(0u, RTS_PORT_WAKE_EXTERNAL);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() == 0u);
    rts_host_port_test_set_next_wake(1u, RTS_PORT_WAKE_TIMER);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() == 1u);

    rts_kernel_state_get()->current_tick = UINT32_MAX - 2u;
    delayed_task_initialize(&delayed, 1u);
    CHECK(rts_power_plan_compute(&plan));
    CHECK(plan.sleep_ticks == 4u);

    reset_running_idle();
    rts_host_port_test_set_next_wake(
        (rts_tick_t)RTS_TICKLESS_MAX_SLEEP_TICKS, RTS_PORT_WAKE_TIMER);
    rts_power_idle();
    CHECK(rts_kernel_tick_now() ==
          (rts_tick_t)RTS_TICKLESS_MAX_SLEEP_TICKS);
}

static void test_deterministic_random_intervals(void)
{
    uint32_t state = UINT32_C(0x12345678);
    rts_tick_t expected = 0u;
    unsigned int index;

    reset_running_idle();
    for (index = 0u; index < 512u; ++index)
    {
        rts_tick_t elapsed;

        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        elapsed = state % UINT32_C(997);
        rts_host_port_test_set_next_wake(elapsed,
                                         RTS_PORT_WAKE_EXTERNAL);
        rts_power_idle();
        expected += elapsed;
        CHECK(rts_kernel_tick_now() == expected);
    }
    CHECK(rts_host_port_test_sleep_count() == 512u);
}

static void test_not_idle_and_port_failure(void)
{
    rts_tcb_t ready;
    uint32_t attempts;

    reset_running_idle();
    ready = (rts_tcb_t){0};
    rts_list_node_initialize(&ready.ready_node);
    ready.priority = 1u;
    ready.state = RTS_TASK_STATE_READY;
    rts_ready_insert(&rts_kernel_state_get()->ready_set, &ready);
    CHECK(!rts_power_sleep_is_allowed());
    rts_power_idle();
    CHECK(rts_host_port_test_sleep_count() == 0u);
    rts_ready_remove(&rts_kernel_state_get()->ready_set, &ready);

    attempts = rts_kernel_state_get()->runtime_counters.tickless_sleep_attempts;
    rts_host_port_test_fail_next_sleep(true);
    rts_power_idle();
    CHECK(rts_kernel_state_get()->runtime_counters.tickless_sleep_attempts ==
          attempts + 1u);
    CHECK(rts_kernel_state_get()->runtime_counters.tickless_sleep_aborts == 1u);
}

int main(void)
{
    test_maintenance_and_external_wake();
    test_delay_deadline_and_switch_request();
    test_software_timer_deadline();
    test_zero_one_wrap_and_long_sleep();
    test_deterministic_random_intervals();
    test_not_idle_and_port_failure();
    return failures;
}
