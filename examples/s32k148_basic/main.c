#include "smoke_test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "S32K148.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "rts/rts_semaphore.h"
#include "target.h"
#include "target_tick.h"
#include "time_internal.h"

#define RTS_SMOKE_STACK_SIZE_BYTES 1024u
#define RTS_SMOKE_GUARD_SIZE_BYTES 32u
#define RTS_SMOKE_GUARD_VALUE      UINT8_C(0xa5)
#define RTS_SMOKE_TASK_A_ID        UINT32_C(0xa11a0001)
#define RTS_SMOKE_TASK_B_ID        UINT32_C(0xb22b0002)
#define RTS_SMOKE_TASK_C_ID        UINT32_C(0xc33c0003)

typedef struct
{
    uint32_t identifier;
    volatile uint32_t *counter;
    volatile uint32_t *argument_seen;
    volatile uint32_t *psp_record;
    volatile uint32_t *msp_record;
    volatile uint32_t *control_record;
    unsigned char *stack;
    const uint32_t *register_patterns;
} rts_smoke_task_argument_t;

RTS_TASK_STACK_DECLARE(g_task_a_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_b_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_c_stack, RTS_SMOKE_STACK_SIZE_BYTES);

rts_s32k148_smoke_record_t g_rts_s32k148_smoke_record;
static rts_semaphore_t g_smoke_semaphore;
static rts_mutex_t g_smoke_mutex;
static bool g_low_mutex_locked;
static rts_tick_t g_low_mutex_lock_tick;

bool rts_s32k148_tick_isr_hook(void)
{
    static rts_tick_t last_give_tick;
    rts_tick_t now = rts_kernel_tick_now();
    bool higher_woken = false;

    if ((rts_tick_t)(now - last_give_tick) < (rts_tick_t)20u)
    {
        return false;
    }
    last_give_tick = now;
    if (rts_semaphore_give_from_isr(&g_smoke_semaphore, &higher_woken) !=
        RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
        return false;
    }
    ++g_rts_s32k148_smoke_record.semaphore_isr_give_count;
    return higher_woken;
}

static const uint32_t g_task_a_patterns[8] = {
    UINT32_C(0xa4040404), UINT32_C(0xa5050505),
    UINT32_C(0xa6060606), UINT32_C(0xa7070707),
    UINT32_C(0xa8080808), UINT32_C(0xa9090909),
    UINT32_C(0xaa101010), UINT32_C(0xab111111)
};
static const uint32_t g_task_b_patterns[8] = {
    UINT32_C(0xb4040404), UINT32_C(0xb5050505),
    UINT32_C(0xb6060606), UINT32_C(0xb7070707),
    UINT32_C(0xb8080808), UINT32_C(0xb9090909),
    UINT32_C(0xba101010), UINT32_C(0xbb111111)
};
static const uint32_t g_task_c_patterns[8] = {
    UINT32_C(0xc4040404), UINT32_C(0xc5050505),
    UINT32_C(0xc6060606), UINT32_C(0xc7070707),
    UINT32_C(0xc8080808), UINT32_C(0xc9090909),
    UINT32_C(0xca101010), UINT32_C(0xcb111111)
};

static rts_smoke_task_argument_t g_task_a_argument = {
    RTS_SMOKE_TASK_A_ID,
    &g_rts_s32k148_smoke_record.task_a_count,
    &g_rts_s32k148_smoke_record.task_a_argument_seen,
    &g_rts_s32k148_smoke_record.task_a_psp,
    &g_rts_s32k148_smoke_record.task_a_msp,
    &g_rts_s32k148_smoke_record.task_a_control,
    g_task_a_stack,
    g_task_a_patterns
};
static rts_smoke_task_argument_t g_task_b_argument = {
    RTS_SMOKE_TASK_B_ID,
    &g_rts_s32k148_smoke_record.task_b_count,
    &g_rts_s32k148_smoke_record.task_b_argument_seen,
    &g_rts_s32k148_smoke_record.task_b_psp,
    &g_rts_s32k148_smoke_record.task_b_msp,
    &g_rts_s32k148_smoke_record.task_b_control,
    g_task_b_stack,
    g_task_b_patterns
};
static rts_smoke_task_argument_t g_task_c_argument = {
    RTS_SMOKE_TASK_C_ID,
    &g_rts_s32k148_smoke_record.task_c_count,
    &g_rts_s32k148_smoke_record.task_c_argument_seen,
    &g_rts_s32k148_smoke_record.task_c_psp,
    &g_rts_s32k148_smoke_record.task_c_msp,
    &g_rts_s32k148_smoke_record.task_c_control,
    g_task_c_stack,
    g_task_c_patterns
};

static void rts_smoke_guard_initialize(unsigned char *stack)
{
    size_t index;
    for (index = 0u; index < RTS_SMOKE_GUARD_SIZE_BYTES; ++index)
    {
        stack[index] = RTS_SMOKE_GUARD_VALUE;
    }
}

static bool rts_smoke_guard_is_valid(const unsigned char *stack)
{
    size_t index;
    for (index = 0u; index < RTS_SMOKE_GUARD_SIZE_BYTES; ++index)
    {
        if (stack[index] != RTS_SMOKE_GUARD_VALUE)
        {
            return false;
        }
    }
    return true;
}

static void rts_smoke_task(void *argument)
{
    rts_smoke_task_argument_t *task = (rts_smoke_task_argument_t *)argument;
    uint32_t expected_id;
    bool high_mutex_exercised = false;

    if (task == &g_task_a_argument)
    {
        expected_id = RTS_SMOKE_TASK_A_ID;
    }
    else if (task == &g_task_b_argument)
    {
        expected_id = RTS_SMOKE_TASK_B_ID;
    }
    else if (task == &g_task_c_argument)
    {
        expected_id = RTS_SMOKE_TASK_C_ID;
    }
    else
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_ARGUMENT;
        for (;;)
        {
        }
    }

    *task->argument_seen = task->identifier;
    if (task->identifier != expected_id)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_ARGUMENT;
    }

    *task->psp_record = __get_PSP();
    *task->msp_record = __get_MSP();
    *task->control_record = __get_CONTROL();
    if ((*task->control_record & CONTROL_SPSEL_Msk) == 0u ||
        (*task->control_record & CONTROL_FPCA_Msk) != 0u ||
        (*task->psp_record & 15u) != 0u || *task->psp_record == 0u)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_THREAD_STACK;
    }

    rts_s32k148_request_handler_probe();
    if (!rts_s32k148_handler_probe_passed())
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_HANDLER_STACK;
    }

    if (task == &g_task_c_argument && rts_task_delay(10u) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
    }

    for (;;)
    {
        ++(*task->counter);
        g_rts_s32k148_smoke_record.current_task_identifier = task->identifier;
        if (task != &g_task_a_argument)
        {
            uint32_t previous =
                g_rts_s32k148_smoke_record.last_low_priority_identifier;
            if (previous != 0u && previous != task->identifier)
            {
                ++g_rts_s32k148_smoke_record.time_slice_rotation_count;
            }
            g_rts_s32k148_smoke_record.last_low_priority_identifier =
                task->identifier;
        }
        g_rts_s32k148_smoke_record.observed_tick = rts_kernel_tick_now();
        if (task == &g_task_b_argument && !g_low_mutex_locked)
        {
            if (rts_mutex_lock(&g_smoke_mutex, 0u) == RTS_STATUS_OK)
            {
                g_low_mutex_locked = true;
                g_low_mutex_lock_tick = rts_kernel_tick_now();
                ++g_rts_s32k148_smoke_record.mutex_low_lock_count;
            }
            else
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_SEMAPHORE;
            }
        }
        if (task == &g_task_b_argument && g_low_mutex_locked &&
            (rts_tick_t)(rts_kernel_tick_now() - g_low_mutex_lock_tick) >= 12u)
        {
            if (rts_mutex_unlock(&g_smoke_mutex) == RTS_STATUS_OK)
            {
                g_low_mutex_locked = false;
                ++g_rts_s32k148_smoke_record.mutex_low_unlock_count;
            }
            else
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_SEMAPHORE;
            }
        }
        if (!rts_smoke_guard_is_valid(task->stack))
        {
            g_rts_s32k148_smoke_record.failure_flags |=
                RTS_SMOKE_FAILURE_STACK_GUARD;
        }
        {
            uint32_t register_result =
                rts_smoke_verify_registers(task->register_patterns);
            if (register_result == 1u)
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_REGISTER;
            }
            else if (register_result != 0u)
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_YIELD;
            }
        }
        if (task == &g_task_a_argument)
        {
            rts_status_t wait_status;

            if (!high_mutex_exercised)
            {
                if (rts_task_delay(1u) != RTS_STATUS_OK ||
                    rts_mutex_lock(&g_smoke_mutex, RTS_WAIT_FOREVER) !=
                        RTS_STATUS_OK)
                {
                    g_rts_s32k148_smoke_record.failure_flags |=
                        RTS_SMOKE_FAILURE_SEMAPHORE;
                }
                else
                {
                    ++g_rts_s32k148_smoke_record.mutex_high_handoff_count;
                    if (rts_mutex_unlock(&g_smoke_mutex) != RTS_STATUS_OK)
                    {
                        g_rts_s32k148_smoke_record.failure_flags |=
                            RTS_SMOKE_FAILURE_SEMAPHORE;
                    }
                }
                high_mutex_exercised = true;
            }

            if ((g_rts_s32k148_smoke_record.task_a_wakeup_count & 1u) == 0u)
            {
                wait_status = rts_semaphore_take(&g_smoke_semaphore,
                                                 RTS_WAIT_FOREVER);
            }
            else
            {
                wait_status = rts_semaphore_take(&g_smoke_semaphore, 3u);
            }
            if (wait_status == RTS_STATUS_OK)
            {
                ++g_rts_s32k148_smoke_record.semaphore_acquired_count;
            }
            else if (wait_status == RTS_STATUS_TIMEOUT)
            {
                ++g_rts_s32k148_smoke_record.semaphore_timeout_count;
            }
            else
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_SEMAPHORE;
            }
            ++g_rts_s32k148_smoke_record.task_a_wakeup_count;
        }
    }
}

int main(void)
{
    rts_task_handle_t task_a = NULL;
    rts_task_handle_t task_b = NULL;
    rts_task_handle_t task_c = NULL;
    const rts_task_config_t config_a = {
        .entry = rts_smoke_task,
        .argument = &g_task_a_argument,
        .stack_buffer = g_task_a_stack,
        .stack_size_bytes = sizeof(g_task_a_stack),
        .priority = 3u
    };
    const rts_task_config_t config_b = {
        .entry = rts_smoke_task,
        .argument = &g_task_b_argument,
        .stack_buffer = g_task_b_stack,
        .stack_size_bytes = sizeof(g_task_b_stack),
        .priority = 1u
    };
    const rts_task_config_t config_c = {
        .entry = rts_smoke_task,
        .argument = &g_task_c_argument,
        .stack_buffer = g_task_c_stack,
        .stack_size_bytes = sizeof(g_task_c_stack),
        .priority = 2u
    };

    rts_smoke_guard_initialize(g_task_a_stack);
    rts_smoke_guard_initialize(g_task_b_stack);
    rts_smoke_guard_initialize(g_task_c_stack);
    if (rts_semaphore_init(&g_smoke_semaphore, 0u, 1u) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
        return 1;
    }
    if (rts_mutex_init(&g_smoke_mutex) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
        return 1;
    }
    if (rts_init() != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_INIT;
        return 1;
    }
    if (rts_task_create(&config_a, &task_a) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_CREATE_A;
        return 2;
    }
    if (rts_task_create(&config_b, &task_b) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_CREATE_B;
        return 3;
    }
    if (rts_task_create(&config_c, &task_c) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_CREATE_C;
        return 4;
    }
    if (rts_start() != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_START_RETURNED;
        return 5;
    }
    g_rts_s32k148_smoke_record.failure_flags |=
        RTS_SMOKE_FAILURE_START_RETURNED;
    return 6;
}
