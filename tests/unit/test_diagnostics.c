#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "diagnostics_internal.h"
#include "fatal_internal.h"
#include "invariant_check_internal.h"
#include "port_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "stack_check_internal.h"
#include "trace_internal.h"

#define TEST_STACK_BYTES 512u

static _Alignas(RTS_TASK_STACK_ALIGNMENT)
    unsigned char test_stacks[2][TEST_STACK_BYTES];
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
    rts_fatal_record_reset_for_test();
    rts_trace_reset_for_test();
}

static rts_task_handle_t create_task(size_t index)
{
    rts_task_config_t config = {
        task_entry, NULL, test_stacks[index], sizeof test_stacks[index], 4u
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    return task;
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

static void test_fatal_record_first_failure(void)
{
    static const uint32_t first_context = UINT32_C(0x12345678);
    static const uint32_t second_context = UINT32_C(0xabcdef01);

    reset_environment();
    CHECK(rts_fatal_record_capture(RTS_FATAL_STACK_CORRUPTION,
                                   &first_context, "first", 17u));
    CHECK(!rts_fatal_record_capture(RTS_FATAL_HARDFAULT,
                                    &second_context, "second", 99u));
    CHECK(g_rts_fatal_record.valid == 1u);
    CHECK(g_rts_fatal_record.reason == RTS_FATAL_STACK_CORRUPTION);
    CHECK(g_rts_fatal_record.context == (uintptr_t)&first_context);
    CHECK(g_rts_fatal_record.source_line == 17u);
    CHECK(g_rts_fatal_record.fatal_count == 1u);
}

static void test_stack_trace_runtime_and_invariants(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t first;
    rts_task_handle_t second;
    rts_trace_entry_t entry;
    rts_diagnostics_snapshot_t snapshot;
    void *saved;
    size_t index;

    reset_environment();
    CHECK(rts_kernel_validate_all());
    CHECK(rts_init() == RTS_STATUS_OK);
    first = create_task(0u);
    second = create_task(1u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_kernel_validate_all());

#if RTS_ENABLE_STACK_GUARDS
    CHECK(rts_stack_guard_is_valid(first));
    first->stack_low[0] ^= UINT8_C(1);
    CHECK(!rts_stack_guard_is_valid(first));
    CHECK(!rts_kernel_validate_all());
    first->stack_low[0] = RTS_STACK_GUARD_PATTERN;
#endif
    saved = first->saved_stack_pointer;
    first->saved_stack_pointer = first->stack_low;
#if RTS_ENABLE_INVARIANT_CHECKS
    CHECK(!rts_task_validate_internal(first));
#endif
    first->saved_stack_pointer = saved;
    CHECK(rts_stack_saved_sp_is_valid(first));
#if RTS_ENABLE_STACK_WATERMARK
    CHECK(rts_stack_watermark_update(first) > 0u);
    CHECK(first->diagnostic_max_stack_used > 0u);
#endif

    rts_trace_reset_for_test();
    for (index = 0u; index < (size_t)RTS_TRACE_CAPACITY + 3u; ++index)
    {
        RTS_TRACE(RTS_TRACE_TICK, index, index + 1u);
    }
#if RTS_ENABLE_TRACE
    CHECK(rts_trace_count() == (size_t)RTS_TRACE_CAPACITY);
    CHECK(rts_trace_overwrite_count() == 3u);
    CHECK(rts_trace_read(0u, &entry));
    CHECK(entry.sequence == 4u);
#else
    CHECK(rts_trace_count() == 0u);
    CHECK(!rts_trace_read(0u, &entry));
#endif

    CHECK(!advance_from_isr(5u));
    CHECK(rts_task_yield() == RTS_STATUS_OK);
    complete_pending_switch();
    CHECK(kernel->current_task == second);
#if RTS_ENABLE_RUNTIME_STATS
    CHECK(first->diagnostic_running_ticks == 5u);
    CHECK(second->diagnostic_dispatch_count == 1u);
#endif
#if RTS_ENABLE_DIAGNOSTICS
    CHECK(rts_diagnostics_snapshot_read(&snapshot));
    CHECK(snapshot.tick == 5u && snapshot.task_count == 2u);
#else
    CHECK(!rts_diagnostics_snapshot_read(&snapshot));
#endif
    CHECK(rts_kernel_validate_all());
    CHECK(rts_diagnostic_counter_increment(UINT32_MAX) == UINT32_MAX);
    CHECK(rts_diagnostic_counter_add(UINT32_MAX - 1u, 2u) == UINT32_MAX);
}

static void test_fault_injection_and_membership_transitions(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t first;
    rts_task_handle_t second;
    rts_mutex_t mutex = {0};
    rts_switch_snapshot_t stale;
    rts_list_t *delay_list;
    void *saved_owner;
    uint32_t ready_word;
    unsigned int assertions_before;

    reset_environment();
    CHECK(rts_init() == RTS_STATUS_OK);
    first = create_task(0u);
    second = create_task(1u);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(kernel->current_task == first);
    CHECK(rts_stack_guard_is_valid(kernel->idle_task));

    ready_word = kernel->ready_set.ready_bitmap[0];
    kernel->ready_set.ready_bitmap[0] ^= UINT32_C(1) << 4u;
#if RTS_ENABLE_INVARIANT_CHECKS
    CHECK(!rts_kernel_validate_all());
#endif
    kernel->ready_set.ready_bitmap[0] = ready_word;

    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_OK);
    CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_OK);
    mutex.owner = second;
#if RTS_ENABLE_INVARIANT_CHECKS
    CHECK(!rts_kernel_validate_all());
#endif
    mutex.owner = first;
    CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_OK);
    CHECK(rts_kernel_validate_all());

    CHECK(rts_task_yield() == RTS_STATUS_OK);
    CHECK(rts_scheduler_switch_acquire(&stale));
    assertions_before = assertion_count;
    ++stale.generation;
    rts_scheduler_switch_complete(&stale);
#if RTS_ENABLE_ASSERTIONS
    CHECK(assertion_count == assertions_before + 1u);
#else
    CHECK(assertion_count == assertions_before);
#endif
    --stale.generation;
    rts_scheduler_switch_complete(&stale);
    CHECK(kernel->current_task == second);

    CHECK(rts_task_delay(2u) == RTS_STATUS_OK);
    CHECK(rts_kernel_validate_all());
    delay_list = &kernel->delay_queue.ordered_tasks;
    saved_owner = second->delay_node.owner;
    second->delay_node.owner = NULL;
#if RTS_ENABLE_INVARIANT_CHECKS
    CHECK(!rts_kernel_validate_all());
#endif
    second->delay_node.owner = saved_owner;
    CHECK(second->delay_node.owner == delay_list);
    CHECK(rts_kernel_validate_all());
    complete_pending_switch();
    (void)advance_from_isr(2u);
    if (kernel->switch_plan.pending)
    {
        complete_pending_switch();
    }
    CHECK(rts_kernel_validate_all());

    rts_fatal_record_reset_for_test();
    CHECK(rts_fatal_record_capture(RTS_FATAL_HARDFAULT,
                                   &kernel->current_task,
                                   "synthetic-hardfault", 1u));
    CHECK(g_rts_fatal_record.reason == RTS_FATAL_HARDFAULT);
    assertion_count = 0u;
}

static void test_twenty_thousand_event_stress(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_semaphore_t semaphore = {0};
    rts_mutex_t mutex = {0};
    size_t event;

    reset_environment();
    CHECK(rts_semaphore_init(&semaphore, 1u, 1u) == RTS_STATUS_OK);
    CHECK(rts_mutex_init(&mutex) == RTS_STATUS_OK);
    CHECK(rts_init() == RTS_STATUS_OK);
    (void)create_task(0u);
    (void)create_task(1u);
    CHECK(rts_start() == RTS_STATUS_OK);

    for (event = 0u; event < 20000u; ++event)
    {
        switch (event & 3u)
        {
            case 0u:
                (void)advance_from_isr(1u);
                break;
            case 1u:
                CHECK(rts_task_yield() == RTS_STATUS_OK);
                break;
            case 2u:
                CHECK(rts_semaphore_take(&semaphore, 0u) == RTS_STATUS_OK);
                CHECK(rts_semaphore_give(&semaphore) == RTS_STATUS_OK);
                break;
            default:
                CHECK(rts_mutex_lock(&mutex, 0u) == RTS_STATUS_OK);
                CHECK(rts_mutex_unlock(&mutex) == RTS_STATUS_OK);
                break;
        }
        if (kernel->switch_plan.pending)
        {
            complete_pending_switch();
        }
        CHECK(rts_kernel_validate_all());
        CHECK(kernel->current_task != NULL);
        CHECK(kernel->current_task->state == RTS_TASK_STATE_RUNNING);
    }
    CHECK(assertion_count == 0u);
}

int main(void)
{
    test_fatal_record_first_failure();
    test_stack_trace_runtime_and_invariants();
    test_fault_injection_and_membership_transitions();
    test_twenty_thousand_event_stress();
    return test_failures == 0 ? 0 : first_failure_line;
}
