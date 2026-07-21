#include "smoke_test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "target_device.h"
#include "rts/rts.h"
#include "rts/rts_task.h"
#include "rts/rts_semaphore.h"
#include "rts/rts_timer.h"
#include "target.h"
#include "target_config.h"
#include "target_led.h"
#include "target_diagnostics.h"
#include "target_tick.h"
#include "time_internal.h"
#include "diagnostics_internal.h"
#include "invariant_check_internal.h"
#include "stack_check_internal.h"
#include "scheduler_internal.h"
#include "power_internal.h"

#define RTS_SMOKE_STACK_SIZE_BYTES 1024u
#define RTS_SMOKE_GUARD_SIZE_BYTES RTS_STACK_GUARD_SIZE_BYTES
#define RTS_SMOKE_GUARD_VALUE      UINT8_C(0xa5)
#define RTS_SMOKE_TASK_A_ID        UINT32_C(0xa11a0001)
#define RTS_SMOKE_TASK_B_ID        UINT32_C(0xb22b0002)
#define RTS_SMOKE_TASK_C_ID        UINT32_C(0xc33c0003)
#define RTS_SMOKE_TASK_D_ID        UINT32_C(0xd44d0004)
#define RTS_SMOKE_TASK_E_ID        UINT32_C(0xe55e0005)
#define RTS_SMOKE_TASK_F_ID        UINT32_C(0xf66f0006)
#define RTS_SMOKE_TASK_G_ID        UINT32_C(0xa77a0007)
#define RTS_SMOKE_TASK_H_ID        UINT32_C(0xb88b0008)

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
    rts_tick_t work_delay_ticks;
} rts_smoke_task_argument_t;

typedef struct
{
    uint16_t adc[8];
    uint32_t can_id;
    float value;
    float confidence;
    uint8_t state;
    uint8_t fault;
} StarAutomotiveSample;

RTS_TASK_STACK_DECLARE(g_task_a_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_b_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_c_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_d_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_e_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_f_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_g_stack, RTS_SMOKE_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_task_h_stack, RTS_SMOKE_STACK_SIZE_BYTES);

rts_s32k148_smoke_record_t g_rts_s32k148_smoke_record;
static rts_semaphore_t g_smoke_semaphore;
static rts_semaphore_t g_timer_semaphore;
static rts_mutex_t g_smoke_mutex;
static bool g_low_mutex_locked;
static rts_tick_t g_low_mutex_lock_tick;
static rts_timer_handle_t g_periodic_timer;
static rts_timer_handle_t g_one_shot_timer;

void rts_power_before_sleep(rts_tick_t planned_ticks)
{
    (void)planned_ticks;
    ++g_rts_s32k148_smoke_record.tickless_before_sleep_count;
}

void rts_power_after_sleep(rts_tick_t elapsed_ticks,
                           rts_port_wake_source_t source)
{
    (void)source;
    ++g_rts_s32k148_smoke_record.tickless_after_sleep_count;
    g_rts_s32k148_smoke_record.tickless_elapsed_ticks += elapsed_ticks;
}

static void rts_smoke_timer_context_record(void)
{
    rts_tcb_t *current = rts_scheduler_current_get();

    g_rts_s32k148_smoke_record.timer_callback_psp = __get_PSP();
    g_rts_s32k148_smoke_record.timer_callback_ipsr = __get_IPSR();
    g_rts_s32k148_smoke_record.timer_service_identity_valid =
        rts_scheduler_task_is_timer_service(current) ? 1u : 0u;
    if (__get_IPSR() != 0u ||
        !rts_scheduler_task_is_timer_service(current))
    {
        g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_TIMER;
    }
}

static void rts_smoke_periodic_timer_callback(void *argument)
{
    (void)argument;
    rts_smoke_timer_context_record();
    ++g_rts_s32k148_smoke_record.timer_periodic_callback_count;
}

static void rts_smoke_one_shot_timer_callback(void *argument)
{
    (void)argument;
    rts_smoke_timer_context_record();
    ++g_rts_s32k148_smoke_record.timer_one_shot_callback_count;
    if (rts_semaphore_give(&g_timer_semaphore) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_TIMER;
    }
}

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
static const uint32_t g_task_d_patterns[8] = {
    UINT32_C(0xd4040404), UINT32_C(0xd5050505), UINT32_C(0xd6060606), UINT32_C(0xd7070707),
    UINT32_C(0xd8080808), UINT32_C(0xd9090909), UINT32_C(0xda101010), UINT32_C(0xdb111111)
};
static const uint32_t g_task_e_patterns[8] = {
    UINT32_C(0xe4040404), UINT32_C(0xe5050505), UINT32_C(0xe6060606), UINT32_C(0xe7070707),
    UINT32_C(0xe8080808), UINT32_C(0xe9090909), UINT32_C(0xea101010), UINT32_C(0xeb111111)
};
static const uint32_t g_task_f_patterns[8] = {
    UINT32_C(0xf4040404), UINT32_C(0xf5050505), UINT32_C(0xf6060606), UINT32_C(0xf7070707),
    UINT32_C(0xf8080808), UINT32_C(0xf9090909), UINT32_C(0xfa101010), UINT32_C(0xfb111111)
};
static const uint32_t g_task_g_patterns[8] = {
    UINT32_C(0x74040404), UINT32_C(0x75050505), UINT32_C(0x76060606), UINT32_C(0x77070707),
    UINT32_C(0x78080808), UINT32_C(0x79090909), UINT32_C(0x7a101010), UINT32_C(0x7b111111)
};
static const uint32_t g_task_h_patterns[8] = {
    UINT32_C(0x84040404), UINT32_C(0x85050505), UINT32_C(0x86060606), UINT32_C(0x87070707),
    UINT32_C(0x88080808), UINT32_C(0x89090909), UINT32_C(0x8a101010), UINT32_C(0x8b111111)
};

static rts_smoke_task_argument_t g_task_a_argument = {
    RTS_SMOKE_TASK_A_ID,
    &g_rts_s32k148_smoke_record.task_a_count,
    &g_rts_s32k148_smoke_record.task_a_argument_seen,
    &g_rts_s32k148_smoke_record.task_a_psp,
    &g_rts_s32k148_smoke_record.task_a_msp,
    &g_rts_s32k148_smoke_record.task_a_control,
    g_task_a_stack,
    g_task_a_patterns, 5u
};
static rts_smoke_task_argument_t g_task_b_argument = {
    RTS_SMOKE_TASK_B_ID,
    &g_rts_s32k148_smoke_record.task_b_count,
    &g_rts_s32k148_smoke_record.task_b_argument_seen,
    &g_rts_s32k148_smoke_record.task_b_psp,
    &g_rts_s32k148_smoke_record.task_b_msp,
    &g_rts_s32k148_smoke_record.task_b_control,
    g_task_b_stack,
    g_task_b_patterns, 10u
};
static rts_smoke_task_argument_t g_task_c_argument = {
    RTS_SMOKE_TASK_C_ID,
    &g_rts_s32k148_smoke_record.task_c_count,
    &g_rts_s32k148_smoke_record.task_c_argument_seen,
    &g_rts_s32k148_smoke_record.task_c_psp,
    &g_rts_s32k148_smoke_record.task_c_msp,
    &g_rts_s32k148_smoke_record.task_c_control,
    g_task_c_stack,
    g_task_c_patterns, 25u
};
static rts_smoke_task_argument_t g_task_d_argument = {
    RTS_SMOKE_TASK_D_ID, &g_rts_s32k148_smoke_record.task_d_count,
    &g_rts_s32k148_smoke_record.task_d_argument_seen, &g_rts_s32k148_smoke_record.task_d_psp,
    &g_rts_s32k148_smoke_record.task_d_msp, &g_rts_s32k148_smoke_record.task_d_control,
    g_task_d_stack, g_task_d_patterns, 50u
};
static rts_smoke_task_argument_t g_task_e_argument = {
    RTS_SMOKE_TASK_E_ID, &g_rts_s32k148_smoke_record.task_e_count,
    &g_rts_s32k148_smoke_record.task_e_argument_seen, &g_rts_s32k148_smoke_record.task_e_psp,
    &g_rts_s32k148_smoke_record.task_e_msp, &g_rts_s32k148_smoke_record.task_e_control,
    g_task_e_stack, g_task_e_patterns, 100u
};
static rts_smoke_task_argument_t g_task_f_argument = {
    RTS_SMOKE_TASK_F_ID, &g_rts_s32k148_smoke_record.task_f_count,
    &g_rts_s32k148_smoke_record.task_f_argument_seen, &g_rts_s32k148_smoke_record.task_f_psp,
    &g_rts_s32k148_smoke_record.task_f_msp, &g_rts_s32k148_smoke_record.task_f_control,
    g_task_f_stack, g_task_f_patterns, 200u
};
static rts_smoke_task_argument_t g_task_g_argument = {
    RTS_SMOKE_TASK_G_ID, &g_rts_s32k148_smoke_record.task_g_count,
    &g_rts_s32k148_smoke_record.task_g_argument_seen, &g_rts_s32k148_smoke_record.task_g_psp,
    &g_rts_s32k148_smoke_record.task_g_msp, &g_rts_s32k148_smoke_record.task_g_control,
    g_task_g_stack, g_task_g_patterns, 500u
};
static rts_smoke_task_argument_t g_task_h_argument = {
    RTS_SMOKE_TASK_H_ID, &g_rts_s32k148_smoke_record.task_h_count,
    &g_rts_s32k148_smoke_record.task_h_argument_seen, &g_rts_s32k148_smoke_record.task_h_psp,
    &g_rts_s32k148_smoke_record.task_h_msp, &g_rts_s32k148_smoke_record.task_h_control,
    g_task_h_stack, g_task_h_patterns, 1000u
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

/* Representative application work used by the board smoke test.  These are
 * deliberately portable computations; a product can replace them with its
 * sensor, control, CAN, or GPIO driver calls. */
static volatile uint32_t g_star_benchmark_sink;

static uint16_t star_adc_read(uint8_t channel)
{
    uint32_t seed = rts_s32k148_cycle_now() ^ ((uint32_t)channel * 7919u);
    return (uint16_t)((seed ^ (seed >> 7u)) & UINT32_C(0x0fff));
}

static void star_can_publish(uint32_t id, const void *payload, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)payload;
    size_t index;
    g_star_benchmark_sink ^= id + (uint32_t)length;
    for (index = 0u; index < length; ++index)
        g_star_benchmark_sink = (g_star_benchmark_sink << 3u) ^ bytes[index];
}

static uint32_t brake_sample_inputs(StarAutomotiveSample *sample)
{
    uint32_t checksum = 0u;
    uint8_t channel;
    for (channel = 0u; channel < 8u; ++channel)
    {
        sample->adc[channel] = star_adc_read(channel);
        checksum += sample->adc[channel] * (uint32_t)(channel + 1u);
        if (sample->adc[channel] > 3800u) sample->fault++;
        else if (sample->adc[channel] < 120u) sample->fault += 2u;
        else sample->confidence += ((sample->adc[channel] & 0x10u) != 0u) ? 0.25f : 0.10f;
    }
    return checksum;
}

static float brake_estimate_slip(StarAutomotiveSample *sample, uint32_t checksum)
{
    float slip = 0.0f;
    uint8_t wheel;
    for (wheel = 0u; wheel < 4u; ++wheel)
    {
        float delta = (float)sample->adc[wheel] - (float)sample->adc[wheel + 4u];
        if (delta > 900.0f) slip += 3.5f;
        else if (delta > 450.0f) slip += 2.0f;
        else if (delta < -450.0f) slip -= 1.5f;
        else slip += delta * 0.001f;
    }
    return slip + (((checksum & 3u) == 0u) ? 1.0f : 0.0f);
}

static void StarTask_5ms_BrakeControl(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t checksum = brake_sample_inputs(&sample);
    float slip = brake_estimate_slip(&sample, checksum);
    uint8_t command = (sample.fault > 3u) ? 0u : ((slip > 4.0f) ? 3u : 1u);
    (void)context;
    rts_s32k148_red_led_set(command == 3u);
    if (command > 1u) star_can_publish(UINT32_C(0x180), &sample, sizeof(sample));
    else g_star_benchmark_sink += checksum;
}

static void StarTask_10ms_SteeringAssist(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t effort = 0u;
    uint8_t index;
    (void)context;
    for (index = 0u; index < 6u; ++index)
    {
        sample.adc[index] = star_adc_read((uint8_t)(index + 8u));
        effort += (sample.adc[index] > 3000u) ? 9u : ((sample.adc[index] < 300u) ? 11u : 3u);
    }
    sample.value = (float)effort * 0.125f;
    rts_s32k148_red_led_set(sample.value > 12.0f);
    star_can_publish(UINT32_C(0x220), &sample.value, sizeof(sample.value));
}

static void StarTask_25ms_CrashSafety(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t energy = 0u;
    uint8_t axis;
    (void)context;
    for (axis = 0u; axis < 6u; ++axis)
    {
        sample.adc[axis] = star_adc_read((uint8_t)(axis + 16u));
        energy += (uint32_t)sample.adc[axis] * sample.adc[axis];
        if (sample.adc[axis] > 3500u) sample.fault += 3u;
    }
    sample.state = (energy > 9000000u || sample.fault > 2u) ? 3u : 0u;
    rts_s32k148_red_led_set(sample.state >= 3u);
    if (sample.state >= 3u) star_can_publish(UINT32_C(0x300), &sample, sizeof(sample));
}

static void StarTask_50ms_BatteryThermal(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t hot_cells = 0u;
    uint8_t cell;
    (void)context;
    for (cell = 0u; cell < 8u; ++cell)
    {
        sample.adc[cell] = star_adc_read((uint8_t)(cell + 24u));
        if (sample.adc[cell] > 3300u) { hot_cells += 2u; sample.fault++; }
        else if (sample.adc[cell] > 2600u) hot_cells++;
    }
    sample.value = (float)hot_cells * 0.05f;
    star_can_publish(UINT32_C(0x410), &sample.value, sizeof(sample.value));
}

static void StarTask_100ms_PowertrainManager(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t torque = 0u;
    uint8_t sensor;
    (void)context;
    for (sensor = 0u; sensor < 7u; ++sensor)
    {
        sample.adc[sensor] = star_adc_read((uint8_t)(sensor + 32u));
        torque += sample.adc[sensor] / (uint32_t)(sensor + 1u);
        if (sample.adc[sensor] > 3600u) sample.fault++;
    }
    sample.value = (float)torque * 0.01f;
    star_can_publish(UINT32_C(0x510), &sample.value, sizeof(sample.value));
}

static void StarTask_200ms_Diagnostics(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t dtc_mask = 0u;
    uint8_t module;
    (void)context;
    for (module = 0u; module < 8u; ++module)
    {
        sample.adc[module] = star_adc_read((uint8_t)(module + 40u));
        if (sample.adc[module] > 3400u || sample.adc[module] < 100u)
            dtc_mask |= UINT32_C(1) << module;
    }
    sample.state = (dtc_mask == 0u) ? 0u : 1u;
    star_can_publish(UINT32_C(0x620), &dtc_mask, sizeof(dtc_mask));
}

static void StarTask_500ms_NetworkGateway(void *context)
{
    StarAutomotiveSample sample = {0};
    uint8_t bus;
    uint32_t score = 0u;
    (void)context;
    for (bus = 0u; bus < 6u; ++bus) score += star_adc_read((uint8_t)(bus + 48u)) > 1500u ? 3u : 1u;
    sample.can_id = (score > 12u) ? UINT32_C(0x712) : UINT32_C(0x710);
    star_can_publish(sample.can_id, &score, sizeof(score));
}

static void StarTask_1000ms_VehicleHealth(void *context)
{
    StarAutomotiveSample sample = {0};
    uint32_t aging = 0u;
    uint8_t index;
    (void)context;
    for (index = 0u; index < 8u; ++index) aging += star_adc_read((uint8_t)(index + 56u));
    sample.value = (float)aging * 0.0001f;
    sample.state = (sample.value > 0.8f) ? 3u : ((sample.value > 0.5f) ? 2u : 0u);
    star_can_publish(UINT32_C(0x810), &sample.value, sizeof(sample.value));
}

static void rts_automotive_step_measure(const rts_smoke_task_argument_t *task)
{
    uint32_t start = rts_s32k148_cycle_now();
    uint32_t elapsed;
    uint32_t elapsed_us;

    if (task == &g_task_a_argument)
    {
        StarTask_5ms_BrakeControl(NULL);
    }
    else if (task == &g_task_b_argument)
    {
        StarTask_10ms_SteeringAssist(NULL);
    }
    else if (task == &g_task_c_argument)
    {
        StarTask_25ms_CrashSafety(NULL);
    }
    else if (task == &g_task_d_argument)
    {
        StarTask_50ms_BatteryThermal(NULL);
    }
    else if (task == &g_task_e_argument)
    {
        StarTask_100ms_PowertrainManager(NULL);
    }
    else if (task == &g_task_f_argument)
    {
        StarTask_200ms_Diagnostics(NULL);
    }
    else if (task == &g_task_g_argument)
    {
        StarTask_500ms_NetworkGateway(NULL);
    }
    else
    {
        StarTask_1000ms_VehicleHealth(NULL);
    }

    elapsed = rts_s32k148_cycle_now() - start;
    /* The target clock is 48 MHz: one microsecond is 48 DWT cycles. */
    elapsed_us = elapsed / (RTS_S32K148_CORE_CLOCK_HZ / UINT32_C(1000000));
    if (task == &g_task_a_argument)
    {
        g_rts_s32k148_smoke_record.task_a_last_execution_cycles = elapsed;
        g_rts_s32k148_smoke_record.task_a_last_execution_us = elapsed_us;
        if (elapsed > g_rts_s32k148_smoke_record.task_a_max_execution_cycles)
        {
            g_rts_s32k148_smoke_record.task_a_max_execution_cycles = elapsed;
        }
        if (elapsed_us > g_rts_s32k148_smoke_record.task_a_max_execution_us)
        {
            g_rts_s32k148_smoke_record.task_a_max_execution_us = elapsed_us;
        }
    }
    else if (task == &g_task_b_argument)
    {
        g_rts_s32k148_smoke_record.task_b_last_execution_cycles = elapsed;
        g_rts_s32k148_smoke_record.task_b_last_execution_us = elapsed_us;
        if (elapsed > g_rts_s32k148_smoke_record.task_b_max_execution_cycles)
        {
            g_rts_s32k148_smoke_record.task_b_max_execution_cycles = elapsed;
        }
        if (elapsed_us > g_rts_s32k148_smoke_record.task_b_max_execution_us)
        {
            g_rts_s32k148_smoke_record.task_b_max_execution_us = elapsed_us;
        }
    }
    else
    {
        volatile uint32_t *last_cycles = &g_rts_s32k148_smoke_record.task_c_last_execution_cycles;
        volatile uint32_t *last_us = &g_rts_s32k148_smoke_record.task_c_last_execution_us;
        volatile uint32_t *max_cycles = &g_rts_s32k148_smoke_record.task_c_max_execution_cycles;
        volatile uint32_t *max_us = &g_rts_s32k148_smoke_record.task_c_max_execution_us;
        if (task == &g_task_d_argument) { last_cycles = &g_rts_s32k148_smoke_record.task_d_last_execution_cycles; last_us = &g_rts_s32k148_smoke_record.task_d_last_execution_us; max_cycles = &g_rts_s32k148_smoke_record.task_d_max_execution_cycles; max_us = &g_rts_s32k148_smoke_record.task_d_max_execution_us; }
        else if (task == &g_task_e_argument) { last_cycles = &g_rts_s32k148_smoke_record.task_e_last_execution_cycles; last_us = &g_rts_s32k148_smoke_record.task_e_last_execution_us; max_cycles = &g_rts_s32k148_smoke_record.task_e_max_execution_cycles; max_us = &g_rts_s32k148_smoke_record.task_e_max_execution_us; }
        else if (task == &g_task_f_argument) { last_cycles = &g_rts_s32k148_smoke_record.task_f_last_execution_cycles; last_us = &g_rts_s32k148_smoke_record.task_f_last_execution_us; max_cycles = &g_rts_s32k148_smoke_record.task_f_max_execution_cycles; max_us = &g_rts_s32k148_smoke_record.task_f_max_execution_us; }
        else if (task == &g_task_g_argument) { last_cycles = &g_rts_s32k148_smoke_record.task_g_last_execution_cycles; last_us = &g_rts_s32k148_smoke_record.task_g_last_execution_us; max_cycles = &g_rts_s32k148_smoke_record.task_g_max_execution_cycles; max_us = &g_rts_s32k148_smoke_record.task_g_max_execution_us; }
        else if (task == &g_task_h_argument) { last_cycles = &g_rts_s32k148_smoke_record.task_h_last_execution_cycles; last_us = &g_rts_s32k148_smoke_record.task_h_last_execution_us; max_cycles = &g_rts_s32k148_smoke_record.task_h_max_execution_cycles; max_us = &g_rts_s32k148_smoke_record.task_h_max_execution_us; }
        *last_cycles = elapsed;
        *last_us = elapsed_us;
        if (elapsed > *max_cycles)
        {
            *max_cycles = elapsed;
        }
        if (elapsed_us > *max_us)
        {
            *max_us = elapsed_us;
        }
    }
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
    else if (task == &g_task_d_argument) { expected_id = RTS_SMOKE_TASK_D_ID; }
    else if (task == &g_task_e_argument) { expected_id = RTS_SMOKE_TASK_E_ID; }
    else if (task == &g_task_f_argument) { expected_id = RTS_SMOKE_TASK_F_ID; }
    else if (task == &g_task_g_argument) { expected_id = RTS_SMOKE_TASK_G_ID; }
    else if (task == &g_task_h_argument) { expected_id = RTS_SMOKE_TASK_H_ID; }
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
        (*task->psp_record & 7u) != 0u || *task->psp_record == 0u)
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

    if (task == &g_task_c_argument &&
        (rts_task_delay(10u) != RTS_STATUS_OK ||
         rts_semaphore_take(&g_timer_semaphore, RTS_WAIT_FOREVER) !=
             RTS_STATUS_OK))
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
    }

    for (;;)
    {
        rts_automotive_step_measure(task);

        /* Visible hardware heartbeat from Task A.  The divider keeps the
         * active-low red LED slow enough to observe on the EVB. */
        if (task == &g_task_a_argument &&
            ((*task->counter & UINT32_C(0xff)) == 0u))
        {
            rts_s32k148_red_led_toggle();
        }
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
        if (((*task->counter) & UINT32_C(0xff)) == 0u)
        {
            rts_diagnostics_snapshot_t snapshot;
            rts_tcb_t *current = rts_scheduler_current_get();

            if (!rts_diagnostics_snapshot_read(&snapshot) ||
                !rts_kernel_validate_all())
            {
                g_rts_s32k148_smoke_record.diagnostic_invariant_failure = 1u;
            }
            else
            {
                g_rts_s32k148_smoke_record.diagnostic_context_switches =
                    snapshot.context_switches;
                g_rts_s32k148_smoke_record.diagnostic_idle_ticks =
                    snapshot.idle_ticks;
                g_rts_s32k148_smoke_record.diagnostic_non_idle_ticks =
                    snapshot.non_idle_ticks;
                g_rts_s32k148_smoke_record.diagnostic_fatal_reason =
                    snapshot.fatal_reason;
                g_rts_s32k148_smoke_record.
                    diagnostic_cycle_counter_available =
                    g_rts_s32k148_timing_record.cycle_counter_available;
                g_rts_s32k148_smoke_record.
                    diagnostic_maximum_critical_cycles =
                    g_rts_s32k148_timing_record.maximum_critical_cycles;
            }
            if (current != NULL)
            {
                uint32_t used = (uint32_t)rts_stack_watermark_update(current);
                uint32_t running_ticks = current->diagnostic_running_ticks;
#if RTS_ENABLE_RUNTIME_STATS
                running_ticks += rts_kernel_tick_now() -
                                 current->diagnostic_last_start_tick;
#endif
#if RTS_ENABLE_RUNTIME_STATS
                uint32_t dispatch = current->diagnostic_dispatch_count;
#else
                uint32_t dispatch = 0u;
#endif
                if (task == &g_task_a_argument)
                {
                    g_rts_s32k148_smoke_record.task_a_dispatch_count = dispatch;
                    g_rts_s32k148_smoke_record.task_a_running_ticks =
                        running_ticks;
                    g_rts_s32k148_smoke_record.task_a_max_stack_used = used;
                }
                else if (task == &g_task_b_argument)
                {
                    g_rts_s32k148_smoke_record.task_b_dispatch_count = dispatch;
                    g_rts_s32k148_smoke_record.task_b_running_ticks =
                        running_ticks;
                    g_rts_s32k148_smoke_record.task_b_max_stack_used = used;
                }
                else
                {
                    g_rts_s32k148_smoke_record.task_c_dispatch_count = dispatch;
                    g_rts_s32k148_smoke_record.task_c_running_ticks =
                        running_ticks;
                    g_rts_s32k148_smoke_record.task_c_max_stack_used = used;
                }
            }
        }
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
        if (task == &g_task_c_argument &&
            ((*task->counter) & UINT32_C(0x3fff)) == 0u)
        {
            if (rts_timer_stop(g_periodic_timer) != RTS_STATUS_OK ||
                rts_timer_restart(g_periodic_timer) != RTS_STATUS_OK)
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_TIMER;
            }
            else
            {
                ++g_rts_s32k148_smoke_record.timer_stop_restart_count;
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
        {
            bool guard_valid = rts_smoke_guard_is_valid(task->stack);
            if (task == &g_task_a_argument)
            {
                g_rts_s32k148_smoke_record.task_a_stack_guard_ok =
                    guard_valid ? 1u : 0u;
            }
            else if (task == &g_task_b_argument)
            {
                g_rts_s32k148_smoke_record.task_b_stack_guard_ok =
                    guard_valid ? 1u : 0u;
            }
            else
            {
                g_rts_s32k148_smoke_record.task_c_stack_guard_ok =
                    guard_valid ? 1u : 0u;
                if (task == &g_task_d_argument) g_rts_s32k148_smoke_record.task_d_stack_guard_ok = guard_valid ? 1u : 0u;
                else if (task == &g_task_e_argument) g_rts_s32k148_smoke_record.task_e_stack_guard_ok = guard_valid ? 1u : 0u;
                else if (task == &g_task_f_argument) g_rts_s32k148_smoke_record.task_f_stack_guard_ok = guard_valid ? 1u : 0u;
                else if (task == &g_task_g_argument) g_rts_s32k148_smoke_record.task_g_stack_guard_ok = guard_valid ? 1u : 0u;
                else if (task == &g_task_h_argument) g_rts_s32k148_smoke_record.task_h_stack_guard_ok = guard_valid ? 1u : 0u;
            }
            if (!guard_valid)
            {
                g_rts_s32k148_smoke_record.failure_flags |=
                    RTS_SMOKE_FAILURE_STACK_GUARD;
            }
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
        else if (rts_task_delay(task->work_delay_ticks) !=
                 RTS_STATUS_OK)
        {
            g_rts_s32k148_smoke_record.failure_flags |=
                RTS_SMOKE_FAILURE_YIELD;
        }
    }
}

int main(void)
{
    rts_task_handle_t task_a = NULL;
    rts_task_handle_t task_b = NULL;
    rts_task_handle_t task_c = NULL;
    rts_task_handle_t task_d = NULL, task_e = NULL, task_f = NULL;
    rts_task_handle_t task_g = NULL, task_h = NULL;
    const rts_task_config_t config_a = {
        .entry = rts_smoke_task,
        .argument = &g_task_a_argument,
        .stack_buffer = g_task_a_stack,
        .stack_size_bytes = sizeof(g_task_a_stack),
        .priority = 5u,
        .period = 5u,
        .relative_deadline = 5u,
        .execution_budget = 0u
    };
    const rts_task_config_t config_b = {
        .entry = rts_smoke_task,
        .argument = &g_task_b_argument,
        .stack_buffer = g_task_b_stack,
        .stack_size_bytes = sizeof(g_task_b_stack),
        .priority = 5u,
        .period = 10u,
        .relative_deadline = 10u,
        .execution_budget = 0u
    };
    const rts_task_config_t config_c = {
        .entry = rts_smoke_task,
        .argument = &g_task_c_argument,
        .stack_buffer = g_task_c_stack,
        .stack_size_bytes = sizeof(g_task_c_stack),
        .priority = 4u,
        .period = 25u,
        .relative_deadline = 25u,
        .execution_budget = 0u
    };
    const rts_task_config_t config_d = { .entry = rts_smoke_task, .argument = &g_task_d_argument, .stack_buffer = g_task_d_stack, .stack_size_bytes = sizeof(g_task_d_stack), .priority = 4u, .period = 50u, .relative_deadline = 50u, .execution_budget = 0u };
    const rts_task_config_t config_e = { .entry = rts_smoke_task, .argument = &g_task_e_argument, .stack_buffer = g_task_e_stack, .stack_size_bytes = sizeof(g_task_e_stack), .priority = 3u, .period = 100u, .relative_deadline = 100u, .execution_budget = 0u };
    const rts_task_config_t config_f = { .entry = rts_smoke_task, .argument = &g_task_f_argument, .stack_buffer = g_task_f_stack, .stack_size_bytes = sizeof(g_task_f_stack), .priority = 3u, .period = 200u, .relative_deadline = 200u, .execution_budget = 0u };
    const rts_task_config_t config_g = { .entry = rts_smoke_task, .argument = &g_task_g_argument, .stack_buffer = g_task_g_stack, .stack_size_bytes = sizeof(g_task_g_stack), .priority = 2u, .period = 500u, .relative_deadline = 500u, .execution_budget = 0u };
    const rts_task_config_t config_h = { .entry = rts_smoke_task, .argument = &g_task_h_argument, .stack_buffer = g_task_h_stack, .stack_size_bytes = sizeof(g_task_h_stack), .priority = 1u, .period = 1000u, .relative_deadline = 1000u, .execution_budget = 0u };
    const rts_timer_config_t periodic_timer_config = {
        .period = 25u,
        .callback = rts_smoke_periodic_timer_callback,
        .argument = NULL,
        .mode = RTS_TIMER_PERIODIC
    };
    const rts_timer_config_t one_shot_timer_config = {
        .period = 40u,
        .callback = rts_smoke_one_shot_timer_callback,
        .argument = NULL,
        .mode = RTS_TIMER_ONE_SHOT
    };

    rts_s32k148_timing_initialize();
    rts_smoke_guard_initialize(g_task_a_stack);
    rts_smoke_guard_initialize(g_task_b_stack);
    rts_smoke_guard_initialize(g_task_c_stack);
    rts_smoke_guard_initialize(g_task_d_stack);
    rts_smoke_guard_initialize(g_task_e_stack);
    rts_smoke_guard_initialize(g_task_f_stack);
    rts_smoke_guard_initialize(g_task_g_stack);
    rts_smoke_guard_initialize(g_task_h_stack);
    if (rts_semaphore_init(&g_smoke_semaphore, 0u, 1u) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_SEMAPHORE;
        return 1;
    }
    if (rts_semaphore_init(&g_timer_semaphore, 0u, 1u) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_TIMER;
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
    if (rts_task_create(&config_d, &task_d) != RTS_STATUS_OK) { g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_CREATE_D; return 7; }
    if (rts_task_create(&config_e, &task_e) != RTS_STATUS_OK) { g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_CREATE_E; return 8; }
    if (rts_task_create(&config_f, &task_f) != RTS_STATUS_OK) { g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_CREATE_F; return 9; }
    if (rts_task_create(&config_g, &task_g) != RTS_STATUS_OK) { g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_CREATE_G; return 10; }
    if (rts_task_create(&config_h, &task_h) != RTS_STATUS_OK) { g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_CREATE_H; return 11; }
    if (rts_timer_init(&periodic_timer_config, &g_periodic_timer) !=
            RTS_STATUS_OK ||
        rts_timer_init(&one_shot_timer_config, &g_one_shot_timer) !=
            RTS_STATUS_OK ||
        rts_timer_start(g_periodic_timer) != RTS_STATUS_OK ||
        rts_timer_start(g_one_shot_timer) != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |= RTS_SMOKE_FAILURE_TIMER;
        return 5;
    }
    if (rts_start() != RTS_STATUS_OK)
    {
        g_rts_s32k148_smoke_record.failure_flags |=
            RTS_SMOKE_FAILURE_START_RETURNED;
        return 6;
    }
    g_rts_s32k148_smoke_record.failure_flags |=
        RTS_SMOKE_FAILURE_START_RETURNED;
    return 7;
}
