#include "port.h"

#include <stdbool.h>
#include <stdint.h>

#include "S32K148.h"
#include "assert_internal.h"
#include "port_internal.h"
#include "target_config.h"
#include "target_tick.h"

extern void (*const g_pfnVectors[])(void);

static uint32_t rts_s32k148_critical_depth;

static uint32_t rts_s32k148_priority_encode(uint32_t logical_priority)
{
    return logical_priority << (8u - RTS_S32K148_NVIC_PRIORITY_BITS);
}

rts_critical_token_t rts_port_critical_enter(void)
{
    uint32_t previous = __get_PRIMASK() & 1u;
    uint32_t next_depth;

    __disable_irq();
    next_depth = rts_s32k148_critical_depth + 1u;
    RTS_FATAL_UNLESS(next_depth != 0u);
    rts_s32k148_critical_depth = next_depth;
    return ((rts_critical_token_t)next_depth << 1u) |
           (rts_critical_token_t)previous;
}

void rts_port_critical_exit(rts_critical_token_t token)
{
    uint32_t token_depth = (uint32_t)(token >> 1u);
    uint32_t previous = (uint32_t)(token & (rts_critical_token_t)1u);
    bool valid = token_depth != 0u &&
                 token_depth == rts_s32k148_critical_depth &&
                 (__get_PRIMASK() & 1u) != 0u;

    RTS_FATAL_UNLESS(valid);
    if (!valid)
    {
        return;
    }
    rts_s32k148_critical_depth = token_depth - 1u;
    __DSB();
    __set_PRIMASK(previous);
    __ISB();
}

bool rts_port_is_in_isr(void)
{
    return __get_IPSR() != 0u;
}

rts_status_t rts_port_initialize(void)
{
    const rts_kernel_state_t *kernel = rts_kernel_state_get();
    const rts_cm4f_start_handoff_t *start_handoff =
        rts_cm4f_start_handoff_get();
    uintptr_t vector_address = (uintptr_t)&g_pfnVectors[0];
    uint32_t svc_encoded = rts_s32k148_priority_encode(
        RTS_S32K148_SVC_LOGICAL_PRIORITY);
    uint32_t pendsv_encoded = rts_s32k148_priority_encode(
        RTS_S32K148_PENDSV_LOGICAL_PRIORITY);

    _Static_assert(__NVIC_PRIO_BITS == RTS_S32K148_NVIC_PRIORITY_BITS,
                   "CMSIS S32K148 priority bits differ from target contract");
    if (__get_IPSR() != 0u || (__get_PRIMASK() & 1u) == 0u ||
        (__get_CONTROL() & (CONTROL_SPSEL_Msk | CONTROL_FPCA_Msk)) != 0u ||
        (__get_MSP() & 7u) != 0u || SCB->VTOR != vector_address ||
        g_pfnVectors[11] != SVC_Handler ||
        g_pfnVectors[14] != PendSV_Handler ||
        g_pfnVectors[15] != SysTick_Handler ||
        start_handoff->valid != 0u || kernel->switch_plan.pending ||
        kernel->switch_plan.active)
    {
        return RTS_STATUS_PORT_ERROR;
    }

    SCB->CCR |= SCB_CCR_STKALIGN_Msk;
    SCB->CPACR &= ~(SCB_CPACR_CP10_Msk | SCB_CPACR_CP11_Msk);
    FPU->FPCCR &= ~(FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk);
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;
    SCB->SHP[7] = (uint8_t)svc_encoded;
    SCB->SHP[10] = (uint8_t)pendsv_encoded;
    __DSB();
    __ISB();

    if ((SCB->CCR & SCB_CCR_STKALIGN_Msk) == 0u ||
        (SCB->CPACR & (SCB_CPACR_CP10_Msk | SCB_CPACR_CP11_Msk)) != 0u ||
        (FPU->FPCCR & (FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk)) != 0u ||
        SCB->SHP[7] != (uint8_t)svc_encoded ||
        SCB->SHP[10] != (uint8_t)pendsv_encoded)
    {
        return RTS_STATUS_PORT_ERROR;
    }
    if (rts_s32k148_tick_initialize(RTS_S32K148_CORE_CLOCK_HZ,
                                    RTS_TICK_RATE_HZ) != RTS_STATUS_OK)
    {
        return RTS_STATUS_PORT_ERROR;
    }
    return RTS_STATUS_OK;
}
