#include <stdbool.h>
#include <stdint.h>

#include "clock_config.h"
#include "clock_manager.h"
#include "flexcan_driver.h"
#include "flexcan_irq.h"
#include "interrupt_manager.h"
#include "peripherals_flexcan_config_1.h"
#include "port.h"
#include "rts/rts.h"
#include "rts/rts_semaphore.h"
#include "rts/rts_task.h"

#define STAR_SAMPLE_COUNT       (40U)
#define STAR_CAN_INSTANCE       (0U)
#define STAR_CAN_TX_MB          (0U)
#define STAR_STACK_SIZE_BYTES   (1024U)

#define STAR_DEMCR              (*(volatile uint32_t *)0xE000EDFCUL)
#define STAR_DWT_CTRL           (*(volatile uint32_t *)0xE0001000UL)
#define STAR_DWT_CYCCNT         (*(volatile uint32_t *)0xE0001004UL)
#define STAR_TRCENA_MASK        (1UL << 24U)
#define STAR_CYCCNT_MASK        (1UL << 0U)

/* The SDK relocates the active interrupt vector table to RAM at startup. */
__asm__(".global g_pfnVectors\n.set g_pfnVectors, __VECTOR_RAM");

typedef struct
{
    uint32_t completed_samples;
    uint32_t core_frequency_hz;
    uint32_t error_flags;
    uint32_t semaphore_init_status;
    uint32_t rts_init_status;
    uint32_t receiver_create_status;
    uint32_t sender_create_status;
    uint32_t rts_start_status;
    uint32_t send_mean_us_x1000;
    uint32_t send_max_us_x1000;
    uint32_t send_return_to_irq_mean_us_x1000;
    uint32_t send_return_to_irq_max_us_x1000;
    uint32_t irq_handler_mean_us_x1000;
    uint32_t irq_handler_max_us_x1000;
    uint32_t irq_exit_to_task_mean_us_x1000;
    uint32_t irq_exit_to_task_max_us_x1000;
    uint32_t send_return_to_task_mean_us_x1000;
    uint32_t send_return_to_task_max_us_x1000;
} StarFlexcanIsrLatencyResults;

RTS_TASK_STACK_DECLARE(g_star_sender_stack, STAR_STACK_SIZE_BYTES);
RTS_TASK_STACK_DECLARE(g_star_receiver_stack, STAR_STACK_SIZE_BYTES);

volatile uint32_t g_star_flexcan_isr_complete;
volatile uint32_t g_star_flexcan_isr_current;
volatile StarFlexcanIsrLatencyResults g_star_flexcan_isr_results;

static rts_semaphore_t g_star_tx_semaphore;
static volatile uint32_t g_star_send_return_cycle;
static volatile uint32_t g_star_irq_entry_cycle;
static volatile uint32_t g_star_irq_exit_cycle;
static volatile uint32_t g_star_irq_handler_cycles;
static uint64_t g_star_send_total;
static uint64_t g_star_send_to_irq_total;
static uint64_t g_star_irq_total;
static uint64_t g_star_irq_to_task_total;
static uint64_t g_star_send_to_task_total;
static uint32_t g_star_send_max;
static uint32_t g_star_send_to_irq_max;
static uint32_t g_star_irq_max;
static uint32_t g_star_irq_to_task_max;
static uint32_t g_star_send_to_task_max;

static const uint8_t g_star_payload[8] =
    { 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U };
static const flexcan_data_info_t g_star_tx_info =
{
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = 8U,
    .fd_enable = false,
    .fd_padding = 0U,
    .enable_brs = false,
    .is_remote = false
};

static uint32_t StarCyclesToUsX1000(uint64_t cycles)
{
    return (uint32_t)((cycles * UINT64_C(1000000000)) /
                      g_star_flexcan_isr_results.core_frequency_hz);
}

static void StarUpdateMaximum(uint32_t value, uint32_t *maximum)
{
    if (value > *maximum) { *maximum = value; }
}

static void StarFlexcanCallback(uint8_t instance,
                                flexcan_event_type_t event_type,
                                uint32_t buffer_index,
                                flexcan_state_t *state)
{
    bool higher_priority_task_woken = false;
    (void)instance;
    (void)state;
    if ((event_type == FLEXCAN_EVENT_TX_COMPLETE) &&
        (buffer_index == STAR_CAN_TX_MB))
    {
        if (rts_semaphore_give_from_isr(&g_star_tx_semaphore,
                                        &higher_priority_task_woken) !=
            RTS_STATUS_OK)
        {
            g_star_flexcan_isr_results.error_flags |= 2U;
        }
        if (higher_priority_task_woken)
        {
            rts_port_request_reschedule(rts_cpu_current_id());
        }
    }
}

void CAN0_ORed_0_15_MB_IRQHandler(void)
{
    g_star_irq_entry_cycle = STAR_DWT_CYCCNT;
    FLEXCAN_IRQHandler(STAR_CAN_INSTANCE);
    g_star_irq_exit_cycle = STAR_DWT_CYCCNT;
    g_star_irq_handler_cycles =
        g_star_irq_exit_cycle - g_star_irq_entry_cycle;
}

static void StarReceiverTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        uint32_t task_cycle;
        uint32_t send_to_irq;
        uint32_t irq_to_task;
        uint32_t send_to_task;
        if (rts_semaphore_take(&g_star_tx_semaphore, RTS_WAIT_FOREVER) !=
            RTS_STATUS_OK)
        {
            g_star_flexcan_isr_results.error_flags |= 4U;
            continue;
        }
        task_cycle = STAR_DWT_CYCCNT;
        send_to_irq = g_star_irq_entry_cycle - g_star_send_return_cycle;
        irq_to_task = task_cycle - g_star_irq_exit_cycle;
        send_to_task = task_cycle - g_star_send_return_cycle;
        g_star_send_to_irq_total += send_to_irq;
        g_star_irq_total += g_star_irq_handler_cycles;
        g_star_irq_to_task_total += irq_to_task;
        g_star_send_to_task_total += send_to_task;
        StarUpdateMaximum(send_to_irq, &g_star_send_to_irq_max);
        StarUpdateMaximum(g_star_irq_handler_cycles, &g_star_irq_max);
        StarUpdateMaximum(irq_to_task, &g_star_irq_to_task_max);
        StarUpdateMaximum(send_to_task, &g_star_send_to_task_max);
        ++g_star_flexcan_isr_results.completed_samples;
        if (g_star_flexcan_isr_results.completed_samples == STAR_SAMPLE_COUNT)
        {
            g_star_flexcan_isr_results.send_mean_us_x1000 =
                StarCyclesToUsX1000(g_star_send_total / STAR_SAMPLE_COUNT);
            g_star_flexcan_isr_results.send_max_us_x1000 =
                StarCyclesToUsX1000(g_star_send_max);
            g_star_flexcan_isr_results.send_return_to_irq_mean_us_x1000 =
                StarCyclesToUsX1000(g_star_send_to_irq_total / STAR_SAMPLE_COUNT);
            g_star_flexcan_isr_results.send_return_to_irq_max_us_x1000 =
                StarCyclesToUsX1000(g_star_send_to_irq_max);
            g_star_flexcan_isr_results.irq_handler_mean_us_x1000 =
                StarCyclesToUsX1000(g_star_irq_total / STAR_SAMPLE_COUNT);
            g_star_flexcan_isr_results.irq_handler_max_us_x1000 =
                StarCyclesToUsX1000(g_star_irq_max);
            g_star_flexcan_isr_results.irq_exit_to_task_mean_us_x1000 =
                StarCyclesToUsX1000(g_star_irq_to_task_total / STAR_SAMPLE_COUNT);
            g_star_flexcan_isr_results.irq_exit_to_task_max_us_x1000 =
                StarCyclesToUsX1000(g_star_irq_to_task_max);
            g_star_flexcan_isr_results.send_return_to_task_mean_us_x1000 =
                StarCyclesToUsX1000(g_star_send_to_task_total / STAR_SAMPLE_COUNT);
            g_star_flexcan_isr_results.send_return_to_task_max_us_x1000 =
                StarCyclesToUsX1000(g_star_send_to_task_max);
            g_star_flexcan_isr_complete =
                (g_star_flexcan_isr_results.error_flags == 0U) ? 1U : 2U;
        }
    }
}

static void StarSenderTask(void *argument)
{
    uint32_t sample = 0U;
    (void)argument;
    for (;;)
    {
        if (sample < STAR_SAMPLE_COUNT)
        {
            uint32_t start_cycle = STAR_DWT_CYCCNT;
            uint32_t send_cycles;
            status_t status = FLEXCAN_DRV_Send(
                STAR_CAN_INSTANCE, STAR_CAN_TX_MB, &g_star_tx_info,
                0x531U, g_star_payload);
            g_star_send_return_cycle = STAR_DWT_CYCCNT;
            send_cycles = g_star_send_return_cycle - start_cycle;
            if (status != STATUS_SUCCESS)
            {
                g_star_flexcan_isr_results.error_flags |= 8U;
            }
            else
            {
                g_star_flexcan_isr_current = sample;
                g_star_send_total += send_cycles;
                StarUpdateMaximum(send_cycles, &g_star_send_max);
                ++sample;
            }
        }
        (void)rts_task_delay(2U);
    }
}

int main(void)
{
    rts_task_handle_t sender = NULL;
    rts_task_handle_t receiver = NULL;
    rts_status_t scheduler_status;
    const rts_task_config_t sender_config =
    {
        .entry = StarSenderTask,
        .argument = NULL,
        .stack_buffer = g_star_sender_stack,
        .stack_size_bytes = sizeof(g_star_sender_stack),
        .priority = 1U,
        .period = 2U,
        .relative_deadline = 2U,
        .execution_budget = 0U
    };
    const rts_task_config_t receiver_config =
    {
        .entry = StarReceiverTask,
        .argument = NULL,
        .stack_buffer = g_star_receiver_stack,
        .stack_size_bytes = sizeof(g_star_receiver_stack),
        .priority = 3U,
        .period = 2U,
        .relative_deadline = 2U,
        .execution_budget = 0U
    };

    g_star_flexcan_isr_results.semaphore_init_status = UINT32_MAX;
    g_star_flexcan_isr_results.rts_init_status = UINT32_MAX;
    g_star_flexcan_isr_results.receiver_create_status = UINT32_MAX;
    g_star_flexcan_isr_results.sender_create_status = UINT32_MAX;
    g_star_flexcan_isr_results.rts_start_status = UINT32_MAX;

    (void)CLOCK_DRV_Init(&clockMan1_InitConfig0);
    flexcanInitConfig0.flexcanMode = FLEXCAN_LOOPBACK_MODE;
    flexcanInitConfig0.fd_enable = false;
    flexcanInitConfig0.payload = FLEXCAN_PAYLOAD_SIZE_8;
    STAR_DEMCR |= STAR_TRCENA_MASK;
    STAR_DWT_CYCCNT = 0U;
    STAR_DWT_CTRL |= STAR_CYCCNT_MASK;
    (void)CLOCK_SYS_GetFreq(CORE_CLK,
                            (uint32_t *)&g_star_flexcan_isr_results.core_frequency_hz);

    if ((g_star_flexcan_isr_results.core_frequency_hz == 0U) ||
        (FLEXCAN_DRV_Init(STAR_CAN_INSTANCE, &flexcanState0,
                          &flexcanInitConfig0) != STATUS_SUCCESS) ||
        (FLEXCAN_DRV_ConfigTxMb(STAR_CAN_INSTANCE, STAR_CAN_TX_MB,
                                &g_star_tx_info, 0x531U) != STATUS_SUCCESS))
    {
        g_star_flexcan_isr_results.error_flags = 1U;
        g_star_flexcan_isr_complete = 2U;
        for (;;) { __asm__ volatile ("nop"); }
    }
    FLEXCAN_DRV_InstallEventCallback(STAR_CAN_INSTANCE,
                                     StarFlexcanCallback, NULL);
    INT_SYS_SetPriority(CAN0_ORed_0_15_MB_IRQn, 6U);

    scheduler_status = rts_semaphore_init(&g_star_tx_semaphore, 0U, 1U);
    g_star_flexcan_isr_results.semaphore_init_status =
        (uint32_t)scheduler_status;
    if (scheduler_status != RTS_STATUS_OK)
    {
        g_star_flexcan_isr_results.error_flags |= 16U;
        g_star_flexcan_isr_complete = 2U;
        for (;;) { __asm__ volatile ("nop"); }
    }
    scheduler_status = rts_init();
    g_star_flexcan_isr_results.rts_init_status = (uint32_t)scheduler_status;
    if (scheduler_status == RTS_STATUS_OK)
    {
        scheduler_status = rts_task_create(&receiver_config, &receiver);
        g_star_flexcan_isr_results.receiver_create_status =
            (uint32_t)scheduler_status;
    }
    if (scheduler_status == RTS_STATUS_OK)
    {
        scheduler_status = rts_task_create(&sender_config, &sender);
        g_star_flexcan_isr_results.sender_create_status =
            (uint32_t)scheduler_status;
    }
    if (scheduler_status == RTS_STATUS_OK)
    {
        scheduler_status = rts_start();
        g_star_flexcan_isr_results.rts_start_status =
            (uint32_t)scheduler_status;
    }
    g_star_flexcan_isr_results.error_flags |= 16U;
    g_star_flexcan_isr_complete = 2U;
    for (;;) { __asm__ volatile ("nop"); }
}
