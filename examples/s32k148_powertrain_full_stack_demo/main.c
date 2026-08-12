#include <stdint.h>

#include "clock_config.h"
#include "clock_manager.h"
#include "flexcan_driver.h"
#include "flexcan_irq.h"
#include "peripherals_flexcan_config_1.h"
#include "powertrain_full_stack_demo.h"

#define STAR_TASK_COUNT          (5U)
#define STAR_SAMPLE_COUNT        (40U)
#define STAR_WARMUP_COUNT        (8U)
#define STAR_CAN_INSTANCE        (0U)
#define STAR_CAN_TX_MB           (0U)
#define STAR_CAN_TIMEOUT_CYCLES  (480000U)
#define STAR_DEMCR               (*(volatile uint32_t *)0xE000EDFCUL)
#define STAR_DWT_CTRL            (*(volatile uint32_t *)0xE0001000UL)
#define STAR_DWT_CYCCNT          (*(volatile uint32_t *)0xE0001004UL)
#define STAR_TRCENA_MASK         (1UL << 24U)
#define STAR_CYCCNT_MASK         (1UL << 0U)

typedef void (*StarTaskFunction)(void);

typedef struct
{
    uint32_t task_id;
    uint32_t period_ms;
    StarTaskFunction function;
} StarTaskDescriptor;

typedef struct
{
    uint32_t task_id;
    uint32_t period_ms;
    uint32_t completed_samples;
    uint32_t mean_execution_us_x1000;
    uint32_t max_execution_us_x1000;
} StarTaskPoint;

typedef struct
{
    uint32_t task_count;
    uint32_t sample_count;
    uint32_t core_frequency_hz;
    uint32_t measurement_overhead_cycles;
    uint32_t error_flags;
    uint32_t can_completion_mean_us_x1000;
    uint32_t can_completion_max_us_x1000;
    StarTaskPoint points[STAR_TASK_COUNT];
} StarFullStackResults;

static const StarTaskDescriptor g_star_tasks[STAR_TASK_COUNT] =
{
    { 0U, 5U, StarTask_PedalAndTorqueControl_5ms },
    { 1U, 10U, StarTask_SensorFusionAndSpeed_10ms },
    { 2U, 20U, StarTask_TorqueLimiterAndTraction_20ms },
    { 3U, 50U, StarTask_DiagnosticsAndPlausibility_50ms },
    { 4U, 100U, StarTask_CanObdAndNetworkManagement_100ms }
};

static const flexcan_data_info_t g_star_tx_info =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = 8U,
    .fd_enable = false,
    .fd_padding = 0U,
    .enable_brs = false,
    .is_remote = false
};

volatile uint32_t g_star_full_stack_complete;
volatile uint32_t g_star_full_stack_current;
volatile StarFullStackResults g_star_full_stack_results;

static uint32_t g_star_adc_sequence;
static uint32_t g_star_can_pending;
static uint32_t g_star_can_start_cycles;
static uint64_t g_star_can_total_cycles;
static uint32_t g_star_can_max_cycles;
static uint32_t g_star_can_count;

uint16_t ADC_Read(uint8_t channel)
{
    g_star_adc_sequence += 37U;
    return (uint16_t)((512U + ((uint32_t)channel * 173U) +
                       g_star_adc_sequence) & 4095U);
}

void CAN_Send(uint32_t identifier, const void *payload, uint8_t length)
{
    if ((length != 8U) || (g_star_can_pending != 0U) ||
        (FLEXCAN_DRV_Send(STAR_CAN_INSTANCE, STAR_CAN_TX_MB, &g_star_tx_info,
                          identifier, (const uint8_t *)payload) != STATUS_SUCCESS))
    {
        g_star_full_stack_results.error_flags |= 2U;
    }
    else
    {
        g_star_can_start_cycles = STAR_DWT_CYCCNT;
        g_star_can_pending = 1U;
    }
}

static uint32_t StarCyclesToUsX1000(uint64_t cycles)
{
    return (uint32_t)((cycles * UINT64_C(1000000000)) /
                      g_star_full_stack_results.core_frequency_hz);
}

static uint32_t StarCalibrateOverhead(void)
{
    uint32_t index;
    uint32_t minimum = UINT32_MAX;
    for (index = 0U; index < 64U; ++index)
    {
        const uint32_t start = STAR_DWT_CYCCNT;
        const uint32_t elapsed = STAR_DWT_CYCCNT - start;
        if (elapsed < minimum)
        {
            minimum = elapsed;
        }
    }
    return minimum;
}

static void StarCompleteCanTransmission(void)
{
    uint32_t timeout_start;
    uint32_t elapsed;

    if (g_star_can_pending == 0U)
    {
        return;
    }
    timeout_start = STAR_DWT_CYCCNT;
    while ((CAN0->IFLAG1 & (1UL << STAR_CAN_TX_MB)) == 0U)
    {
        if ((STAR_DWT_CYCCNT - timeout_start) > STAR_CAN_TIMEOUT_CYCLES)
        {
            g_star_full_stack_results.error_flags |= 4U;
            return;
        }
    }
    elapsed = STAR_DWT_CYCCNT - g_star_can_start_cycles;
    g_star_can_total_cycles += elapsed;
    if (elapsed > g_star_can_max_cycles)
    {
        g_star_can_max_cycles = elapsed;
    }
    ++g_star_can_count;
    FLEXCAN_IRQHandler(STAR_CAN_INSTANCE);
    g_star_can_pending = 0U;
}

static void StarMeasureTask(uint32_t task_index)
{
    volatile StarTaskPoint *point =
        &g_star_full_stack_results.points[task_index];
    uint64_t total_cycles = 0U;
    uint32_t maximum_cycles = 0U;
    uint32_t sample;

    point->task_id = g_star_tasks[task_index].task_id;
    point->period_ms = g_star_tasks[task_index].period_ms;
    g_star_full_stack_current = task_index;

    for (sample = 0U; sample < STAR_WARMUP_COUNT; ++sample)
    {
        StarPowertrainDemo_Initialize();
        StarIsr_AdcComplete();
        g_star_tasks[task_index].function();
        StarCompleteCanTransmission();
    }
    for (sample = 0U; sample < STAR_SAMPLE_COUNT; ++sample)
    {
        uint32_t start;
        uint32_t elapsed;
        StarPowertrainDemo_Initialize();
        StarIsr_AdcComplete();
        start = STAR_DWT_CYCCNT;
        g_star_tasks[task_index].function();
        elapsed = STAR_DWT_CYCCNT - start;
        if (elapsed >= g_star_full_stack_results.measurement_overhead_cycles)
        {
            elapsed -= g_star_full_stack_results.measurement_overhead_cycles;
        }
        else
        {
            g_star_full_stack_results.error_flags |= 8U;
        }
        total_cycles += elapsed;
        if (elapsed > maximum_cycles)
        {
            maximum_cycles = elapsed;
        }
        StarCompleteCanTransmission();
    }
    point->completed_samples = STAR_SAMPLE_COUNT;
    point->mean_execution_us_x1000 =
        StarCyclesToUsX1000(total_cycles / STAR_SAMPLE_COUNT);
    point->max_execution_us_x1000 = StarCyclesToUsX1000(maximum_cycles);
}

int main(void)
{
    uint32_t task_index;

    (void)CLOCK_DRV_Init(&clockMan1_InitConfig0);
    flexcanInitConfig0.flexcanMode = FLEXCAN_LOOPBACK_MODE;
    flexcanInitConfig0.fd_enable = false;
    flexcanInitConfig0.payload = FLEXCAN_PAYLOAD_SIZE_8;
    STAR_DEMCR |= STAR_TRCENA_MASK;
    STAR_DWT_CYCCNT = 0U;
    STAR_DWT_CTRL |= STAR_CYCCNT_MASK;
    (void)CLOCK_SYS_GetFreq(
        CORE_CLK, (uint32_t *)&g_star_full_stack_results.core_frequency_hz);
    g_star_full_stack_results.task_count = STAR_TASK_COUNT;
    g_star_full_stack_results.sample_count = STAR_SAMPLE_COUNT;
    g_star_full_stack_results.measurement_overhead_cycles =
        StarCalibrateOverhead();

    if ((g_star_full_stack_results.core_frequency_hz == 0U) ||
        (FLEXCAN_DRV_Init(STAR_CAN_INSTANCE, &flexcanState0,
                          &flexcanInitConfig0) != STATUS_SUCCESS) ||
        (FLEXCAN_DRV_ConfigTxMb(STAR_CAN_INSTANCE, STAR_CAN_TX_MB,
                                &g_star_tx_info, 0x510U) != STATUS_SUCCESS))
    {
        g_star_full_stack_results.error_flags = 1U;
        g_star_full_stack_complete = 2U;
        for (;;) { __asm__ volatile ("nop"); }
    }
    __asm__ volatile ("cpsid i" ::: "memory");
    for (task_index = 0U; task_index < STAR_TASK_COUNT; ++task_index)
    {
        StarMeasureTask(task_index);
    }
    if (g_star_can_count != 0U)
    {
        g_star_full_stack_results.can_completion_mean_us_x1000 =
            StarCyclesToUsX1000(g_star_can_total_cycles / g_star_can_count);
        g_star_full_stack_results.can_completion_max_us_x1000 =
            StarCyclesToUsX1000(g_star_can_max_cycles);
    }
    g_star_full_stack_complete =
        (g_star_full_stack_results.error_flags == 0U) ? 1U : 2U;
    for (;;) { __asm__ volatile ("nop"); }
}
