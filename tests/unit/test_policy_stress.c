#include <stddef.h>
#include <stdint.h>

#include "port_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"
#include "scheduler_policy.h"

#define STRESS_TASK_COUNT 8u
#define STRESS_EVENT_COUNT 50000u
#define STRESS_STACK_BYTES 512u
#define STRESS_SEED UINT32_C(0x13a5c7e9)

static int failures;
static uint32_t random_state = STRESS_SEED;

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

RTS_TASK_STACK_DECLARE(stack_0, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_1, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_2, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_3, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_4, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_5, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_6, STRESS_STACK_BYTES);
RTS_TASK_STACK_DECLARE(stack_7, STRESS_STACK_BYTES);

static uint32_t next_random(void)
{
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return random_state;
}

static void task_entry(void *argument)
{
    (void)argument;
}

int main(void)
{
    void *stacks[STRESS_TASK_COUNT] = {
        stack_0, stack_1, stack_2, stack_3,
        stack_4, stack_5, stack_6, stack_7
    };
    rts_tcb_t *tasks[STRESS_TASK_COUNT] = {0};
    bool ready[STRESS_TASK_COUNT] = {0};
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    size_t index;
    uint32_t event;

    *kernel = (rts_kernel_state_t){0};
    rts_host_port_test_reset();
    CHECK(rts_init() == RTS_STATUS_OK);

    for (index = 0u; index < STRESS_TASK_COUNT; ++index)
    {
        rts_task_handle_t handle = NULL;
        const rts_task_config_t config = {
            .entry = task_entry,
            .argument = NULL,
            .stack_buffer = stacks[index],
            .stack_size_bytes = STRESS_STACK_BYTES,
            .priority = (rts_priority_t)(index + 1u),
            .period = (rts_tick_t)((index + 1u) * 10u),
            .relative_deadline = (rts_tick_t)((index + 1u) * 10u),
            .execution_budget = 1u
        };
        CHECK(rts_task_create(&config, &handle) == RTS_STATUS_OK);
        tasks[index] = handle;
        ready[index] = true;
    }

    kernel->policy_release_sequence = UINT32_MAX - UINT32_C(16);
    for (event = 0u; event < STRESS_EVENT_COUNT; ++event)
    {
        uint32_t value = next_random();
        index = (size_t)(value % STRESS_TASK_COUNT);
        kernel->current_tick += (rts_tick_t)((value >> 8u) & UINT32_C(7));

        if ((value & UINT32_C(3)) == 0u && ready[index])
        {
            CHECK(rts_policy_task_block(tasks[index]));
            tasks[index]->state = RTS_TASK_STATE_BLOCKED;
            ready[index] = false;
        }
        else if (!ready[index])
        {
            tasks[index]->state = RTS_TASK_STATE_READY;
            CHECK(rts_policy_task_unblock(tasks[index]));
            ready[index] = true;
        }
        else
        {
            rts_tcb_t *selected = rts_policy_pick_next();
            CHECK(selected != NULL);
            if (selected != NULL)
            {
                (void)rts_policy_yield(selected);
            }
        }

        CHECK(rts_policy_pick_next() != NULL);
        CHECK(rts_policy_validate(NULL, false));
        CHECK(rts_policy_validate(tasks[index], ready[index]));
        if (failures != 0)
        {
            break;
        }
    }

    CHECK(event == STRESS_EVENT_COUNT);
    return failures;
}
