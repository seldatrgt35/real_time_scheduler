#include <stddef.h>
#include <stdint.h>

#include "assert_internal.h"
#include "port_internal.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"
#include "timer_internal.h"

#define TEST_STACK_BYTES 256u

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

static void application_entry(void *argument)
{
    (void)argument;
}

static void reset_environment(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();

    *kernel = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
}

static void check_reset_kernel(void)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();

    CHECK(kernel->lifecycle == RTS_KERNEL_RESET);
    CHECK(kernel->application_task_pool.allocated_count == 0u);
    CHECK(kernel->application_task_pool.next_free_hint == 0u);
    CHECK(kernel->idle_task == NULL);
    CHECK(kernel->timer_service_task == NULL);
    CHECK(kernel->current_task == NULL);
    CHECK(kernel->current_tick == 0u);
    CHECK(kernel->ready_set.priority_queue[0].count == 0u);
    CHECK(kernel->delay_queue.ordered_tasks.count == 0u);
    CHECK(kernel->switch_plan.from == NULL);
    CHECK(kernel->switch_plan.to == NULL);
    CHECK(!kernel->switch_plan.pending);
}

static void test_successful_bootstrap(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *idle;
    rts_tcb_t *service;
    rts_host_initial_frame_t frame;
    size_t index;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(kernel->lifecycle == RTS_KERNEL_INITIALIZED);
    CHECK(rts_host_port_test_is_initialized());
    CHECK(rts_host_port_test_critical_depth() == 0u);
    CHECK(!rts_host_port_test_tick_running());
    CHECK(kernel->current_task == NULL);
    CHECK(kernel->current_tick == 0u);
    CHECK(kernel->switch_plan.from == NULL && kernel->switch_plan.to == NULL &&
          !kernel->switch_plan.pending);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 0u);
    CHECK(rts_task_pool_next_free_hint(&kernel->application_task_pool) == 0u);
    CHECK(rts_timer_allocated_count() == 0u);
    CHECK(rts_timer_manager_get()->callback_queue.count == 0u);
    for (index = 0u; index < (size_t)RTS_MAX_TASKS; ++index)
    {
        CHECK(kernel->application_task_pool.slots[index].slot_state ==
              RTS_TASK_SLOT_FREE);
    }

    idle = kernel->idle_task;
    CHECK(idle == &kernel->idle_task_storage);
    CHECK(idle->slot_state == RTS_TASK_SLOT_ALLOCATED);
    CHECK(idle->state == RTS_TASK_STATE_READY);
    CHECK(idle->priority == RTS_IDLE_PRIORITY);
    CHECK(idle->saved_stack_pointer != NULL);
    CHECK(idle->stack_low == kernel->idle_stack);
    CHECK(idle->stack_high == kernel->idle_stack + sizeof kernel->idle_stack);
    CHECK(idle->entry != NULL);
    CHECK(idle->argument == NULL);
    CHECK(idle->wait.reason == RTS_WAIT_NONE && idle->wait.wake_tick == 0u);
    CHECK(idle->slice_remaining == (rts_tick_t)RTS_TIME_SLICE_TICKS);
    CHECK(rts_ready_contains(&kernel->ready_set, idle));
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == idle);
    CHECK(kernel->ready_set.priority_queue[RTS_IDLE_PRIORITY].count == 1u);
    CHECK(!rts_delay_contains(&kernel->delay_queue, idle));
    CHECK(rts_host_port_initial_frame_read(idle->saved_stack_pointer, &frame));
    CHECK(frame.entry == idle->entry);
    CHECK(frame.argument == NULL);
#if RTS_ENABLE_ASSERTIONS
    CHECK(idle->validation_magic == RTS_TASK_VALIDATION_MAGIC);
#endif

    service = kernel->timer_service_task;
    CHECK(service == &kernel->timer_service_task_storage);
    CHECK(service->slot_state == RTS_TASK_SLOT_ALLOCATED);
    CHECK(service->state == RTS_TASK_STATE_BLOCKED);
    CHECK(service->priority == (rts_priority_t)RTS_TIMER_SERVICE_PRIORITY);
    CHECK(service->saved_stack_pointer != NULL);
    CHECK(service->stack_low == kernel->timer_service_stack);
    CHECK(service->stack_high == kernel->timer_service_stack +
                                 sizeof kernel->timer_service_stack);
    CHECK(service->entry == rts_timer_service_entry);
    CHECK(service->wait.reason == RTS_WAIT_TIMER_SERVICE);
    CHECK(!rts_ready_contains(&kernel->ready_set, service));
    CHECK(!rts_delay_contains(&kernel->delay_queue, service));
    CHECK(rts_host_port_initial_frame_read(service->saved_stack_pointer,
                                           &frame));
    CHECK(frame.entry == rts_timer_service_entry);
}

static void test_repeated_and_running_status(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t *idle;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    idle = kernel->idle_task;
    CHECK(rts_init() == RTS_STATUS_ALREADY_INITIALIZED);
    CHECK(kernel->idle_task == idle);
    CHECK(kernel->ready_set.priority_queue[0].count == 1u);

    kernel->lifecycle = RTS_KERNEL_RUNNING;
    CHECK(rts_init() == RTS_STATUS_ALREADY_STARTED);
    CHECK(kernel->idle_task == idle);
    CHECK(kernel->ready_set.priority_queue[0].count == 1u);
}

static void test_isr_rejection(void)
{
    unsigned int before;

    reset_environment();
    rts_host_port_test_set_isr(true);
    before = assertion_count;
    CHECK(rts_init() == RTS_STATUS_INVALID_CONTEXT);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == before + 1u);
#else
    (void)before;
#endif
    check_reset_kernel();
    CHECK(rts_host_port_test_critical_depth() == 0u);
}

static void test_port_initialize_failure_and_retry(void)
{
    reset_environment();
    rts_host_port_test_fail_next_initialize(true);
    CHECK(rts_init() == RTS_STATUS_PORT_ERROR);
    check_reset_kernel();
    CHECK(!rts_host_port_test_is_initialized());
    CHECK(rts_host_port_test_critical_depth() == 0u);
    CHECK(rts_init() == RTS_STATUS_OK);
}

static void test_idle_stack_failure_and_retry(void)
{
    reset_environment();
    rts_host_port_test_fail_next_stack_initialize(true);
    CHECK(rts_init() == RTS_STATUS_PORT_ERROR);
    check_reset_kernel();
    CHECK(rts_host_port_test_is_initialized());
    CHECK(rts_host_port_test_critical_depth() == 0u);
    CHECK(rts_init() == RTS_STATUS_OK);
}

static void test_creation_after_bootstrap(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_config_t config = {
        .entry = application_entry,
        .argument = NULL,
        .stack_buffer = stack,
        .stack_size_bytes = sizeof stack,
        .priority = 1u
    };
    rts_task_handle_t handle = NULL;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
    CHECK(handle != NULL);
    CHECK(rts_task_pool_allocated_count(&kernel->application_task_pool) == 1u);
    CHECK(kernel->idle_task == &kernel->idle_task_storage);
    CHECK(rts_ready_contains(&kernel->ready_set, kernel->idle_task));
    CHECK(rts_ready_peek_highest(&kernel->ready_set) == handle);
}

int rts_test_kernel_bootstrap_run(void)
{
    test_successful_bootstrap();
    test_repeated_and_running_status();
    test_isr_rejection();
    test_port_initialize_failure_and_retry();
    test_idle_stack_failure_and_retry();
    test_creation_after_bootstrap();
    return test_failures;
}

int main(void)
{
    return rts_test_kernel_bootstrap_run();
}
