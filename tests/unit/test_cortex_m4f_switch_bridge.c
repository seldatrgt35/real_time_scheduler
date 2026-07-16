#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "port_offsets.h"
#include "port_switch.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "scheduler_internal.h"

#define TEST_STACK_BYTES 256u

static int test_failures;
static unsigned int assertion_count;

#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

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

static rts_task_handle_t create_task(void *stack)
{
    rts_task_config_t config = {
        task_entry, NULL, stack, TEST_STACK_BYTES, 5u
    };
    rts_task_handle_t task = NULL;

    CHECK(rts_task_create(&config, &task) == RTS_STATUS_OK);
    task->saved_stack_pointer =
        task->stack_high - RTS_CM4F_INITIAL_FRAME_SIZE_BYTES;
    return task;
}

int main(void)
{
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char a_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char b_stack[TEST_STACK_BYTES];
    static _Alignas(RTS_TASK_STACK_ALIGNMENT)
        unsigned char c_stack[TEST_STACK_BYTES];
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_task_handle_t a;
    rts_task_handle_t b;
    rts_task_handle_t c;
    const rts_cm4f_switch_handoff_t *handoff;
    rts_cm4f_switch_handoff_t stale;
    void *outgoing_saved_sp;
    rts_list_node_t *a_next;
    rts_list_node_t *b_next;
    rts_priority_t a_priority;
    rts_priority_t b_priority;
    unsigned int before;

    *kernel = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    CHECK(rts_init() == RTS_STATUS_OK);
    a = create_task(a_stack);
    b = create_task(b_stack);
    c = create_task(c_stack);
    CHECK(rts_scheduler_current_establish(a));
    kernel->lifecycle = RTS_KERNEL_RUNNING;

    CHECK(rts_cm4f_switch_bridge_acquire(a->saved_stack_pointer) == NULL);
    CHECK(rts_cm4f_switch_bridge_no_plan());
    CHECK(kernel->current_task == a);

    a_next = a->ready_node.next;
    b_next = b->ready_node.next;
    a_priority = a->priority;
    b_priority = b->priority;
    outgoing_saved_sp = a->stack_high - 128u;
    rts_scheduler_request_switch_if_needed(b);
    CHECK(rts_host_port_test_switch_request_pending());
    rts_host_port_test_consume_switch_request();

    handoff = rts_cm4f_switch_bridge_acquire(outgoing_saved_sp);
    CHECK(handoff != NULL);
    CHECK(handoff->from == a && handoff->to == b);
    CHECK(handoff->snapshot.from == a && handoff->snapshot.to == b);
    CHECK(handoff->outgoing_saved_stack_pointer == outgoing_saved_sp);
    CHECK(handoff->incoming_saved_stack_pointer == b->saved_stack_pointer);
    CHECK(kernel->switch_plan.active && !kernel->switch_plan.pending);
    CHECK(kernel->current_task == a);
    CHECK(a->state == RTS_TASK_STATE_RUNNING);
    CHECK(b->state == RTS_TASK_STATE_READY);

    stale = *handoff;
    before = assertion_count;
    CHECK(!rts_cm4f_switch_bridge_complete(&stale));
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == a && kernel->switch_plan.active);

    /* Emulate only the two symbolic-offset loads/stores performed by assembly. */
    a->saved_stack_pointer = outgoing_saved_sp;
    CHECK(b->saved_stack_pointer == handoff->incoming_saved_stack_pointer);
    rts_ready_remove(&kernel->ready_set, c);
    c->priority = 7u;
    rts_ready_insert(&kernel->ready_set, c);
    b_next = b->ready_node.next;
    CHECK(!rts_scheduler_prepare_switch(c));
    CHECK(rts_scheduler_switch_reselection_required());
    CHECK(rts_cm4f_switch_bridge_complete(handoff));

    CHECK(kernel->current_task == b);
    CHECK(a->state == RTS_TASK_STATE_READY);
    CHECK(b->state == RTS_TASK_STATE_RUNNING);
    CHECK(!kernel->switch_plan.active && kernel->switch_plan.pending);
    CHECK(kernel->switch_plan.from == b && kernel->switch_plan.to == c);
    CHECK(rts_host_port_test_switch_request_count() == 2u);
    CHECK(!rts_scheduler_switch_reselection_required());
    CHECK(a->ready_node.next == a_next);
    CHECK(b->ready_node.next == b_next);
    CHECK(a->priority == a_priority && b->priority == b_priority);
    CHECK(kernel->lifecycle == RTS_KERNEL_RUNNING);

    before = assertion_count;
    CHECK(!rts_cm4f_switch_bridge_complete(handoff));
    CHECK(assertion_count == before + 1u);
    CHECK(kernel->current_task == b);
    return test_failures;
}
