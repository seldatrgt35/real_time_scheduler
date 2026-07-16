#ifndef RTS_TEST_S32K148_CMSIS_MOCK_H
#define RTS_TEST_S32K148_CMSIS_MOCK_H

#include <stdint.h>

#define __NVIC_PRIO_BITS 4u
#define SysTick_CTRL_ENABLE_Msk    (UINT32_C(1) << 0)
#define SysTick_CTRL_TICKINT_Msk   (UINT32_C(1) << 1)
#define SysTick_CTRL_CLKSOURCE_Msk (UINT32_C(1) << 2)
#define CONTROL_SPSEL_Msk          (UINT32_C(1) << 1)
#define CONTROL_FPCA_Msk           (UINT32_C(1) << 2)
#define SCB_ICSR_NMIPENDSET_Msk    (UINT32_C(1) << 31)
#define SCB_ICSR_PENDSVCLR_Msk     (UINT32_C(1) << 27)
#define SCB_CCR_STKALIGN_Msk       (UINT32_C(1) << 9)
#define SCB_CPACR_CP10_Msk         (UINT32_C(3) << 20)
#define SCB_CPACR_CP11_Msk         (UINT32_C(3) << 22)
#define FPU_FPCCR_LSPEN_Msk        (UINT32_C(1) << 30)
#define FPU_FPCCR_ASPEN_Msk        (UINT32_C(1) << 31)
#define CoreDebug_DEMCR_TRCENA_Msk (UINT32_C(1) << 24)
#define DWT_CTRL_CYCCNTENA_Msk     (UINT32_C(1) << 0)
#define PCC_PCCn_CGC_MASK          (UINT32_C(1) << 30)
#define PCC_LPTMR0_INDEX           64u
#define LPTMR_PSR_PCS(value)       ((uint32_t)(value))
#define LPTMR_PSR_PBYP_MASK        (UINT32_C(1) << 2)
#define LPTMR_CSR_TEN_MASK         (UINT32_C(1) << 0)
#define LPTMR_CSR_TCF_MASK         (UINT32_C(1) << 7)
#define LPTMR_CSR_TIE_MASK         (UINT32_C(1) << 6)

typedef enum
{
    LPTMR0_IRQn = 58
} IRQn_Type;

typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

typedef struct
{
    volatile uint32_t CPUID, ICSR, VTOR, AIRCR, SCR, CCR;
    volatile uint8_t SHP[12];
    volatile uint32_t SHCSR, CFSR, HFSR, DFSR, MMFAR, BFAR, CPACR;
} SCB_Type;

typedef struct
{
    volatile uint32_t FPCCR;
} FPU_Type;

typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} DWT_Type;

typedef struct
{
    volatile uint32_t DEMCR;
} CoreDebug_Type;

typedef struct
{
    volatile uint32_t CSR;
    volatile uint32_t PSR;
    volatile uint32_t CMR;
    volatile uint32_t CNR;
} LPTMR_Type;

typedef struct
{
    volatile uint32_t PCCn[128];
} PCC_Type;

extern SysTick_Type rts_test_systick;
extern SCB_Type rts_test_scb;
extern FPU_Type rts_test_fpu;
extern DWT_Type rts_test_dwt;
extern CoreDebug_Type rts_test_core_debug;
extern LPTMR_Type rts_test_lptmr0;
extern PCC_Type rts_test_pcc;
extern uint32_t rts_test_nvic_enabled[8];
extern uint32_t rts_test_nvic_priority[256];
extern unsigned int rts_test_wfi_count;
extern void (*rts_test_wfi_hook)(void);
extern uint32_t rts_test_primask;
extern uint32_t rts_test_ipsr;
extern uint32_t rts_test_psp;
extern uint32_t rts_test_msp;
extern uint32_t rts_test_control;
extern unsigned int rts_test_dsb_count;
extern unsigned int rts_test_isb_count;

#define SysTick (&rts_test_systick)
#define SCB (&rts_test_scb)
#define FPU (&rts_test_fpu)
#define DWT (&rts_test_dwt)
#define CoreDebug (&rts_test_core_debug)
#define LPTMR0 (&rts_test_lptmr0)
#define PCC (&rts_test_pcc)

static inline void NVIC_ClearPendingIRQ(IRQn_Type irqn)
{
    (void)irqn;
}

static inline void NVIC_SetPriority(IRQn_Type irqn, uint32_t priority)
{
    rts_test_nvic_priority[(uint32_t)irqn] = priority;
}

static inline void NVIC_EnableIRQ(IRQn_Type irqn)
{
    rts_test_nvic_enabled[(uint32_t)irqn / 32u] |=
        UINT32_C(1) << ((uint32_t)irqn % 32u);
}

static inline uint32_t __get_PRIMASK(void)
{
    return rts_test_primask;
}

static inline uint32_t __get_IPSR(void)
{
    return rts_test_ipsr;
}

static inline uint32_t __get_PSP(void)
{
    return rts_test_psp;
}

static inline uint32_t __get_MSP(void)
{
    return rts_test_msp;
}

static inline uint32_t __get_CONTROL(void)
{
    return rts_test_control;
}

static inline void __DSB(void)
{
    ++rts_test_dsb_count;
}

static inline void __ISB(void)
{
    ++rts_test_isb_count;
}

static inline void __disable_irq(void)
{
    rts_test_primask = 1u;
}

static inline void __enable_irq(void)
{
    rts_test_primask = 0u;
}

static inline void __set_PRIMASK(uint32_t value)
{
    rts_test_primask = value & 1u;
}

static inline void __WFI(void)
{
    ++rts_test_wfi_count;
    if (rts_test_wfi_hook != 0)
    {
        rts_test_wfi_hook();
    }
}

#endif /* RTS_TEST_S32K148_CMSIS_MOCK_H */
