#ifndef RTS_TEST_S32K148_CMSIS_MOCK_H
#define RTS_TEST_S32K148_CMSIS_MOCK_H

#include <stdint.h>

#define __NVIC_PRIO_BITS 4u
#define SysTick_CTRL_ENABLE_Msk    (UINT32_C(1) << 0)
#define SysTick_CTRL_TICKINT_Msk   (UINT32_C(1) << 1)
#define SysTick_CTRL_CLKSOURCE_Msk (UINT32_C(1) << 2)

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
} SCB_Type;

extern SysTick_Type rts_test_systick;
extern SCB_Type rts_test_scb;
extern uint32_t rts_test_primask;
extern uint32_t rts_test_ipsr;
extern unsigned int rts_test_dsb_count;
extern unsigned int rts_test_isb_count;

#define SysTick (&rts_test_systick)
#define SCB (&rts_test_scb)

static inline uint32_t __get_PRIMASK(void)
{
    return rts_test_primask;
}

static inline uint32_t __get_IPSR(void)
{
    return rts_test_ipsr;
}

static inline void __DSB(void)
{
    ++rts_test_dsb_count;
}

static inline void __ISB(void)
{
    ++rts_test_isb_count;
}

#endif /* RTS_TEST_S32K148_CMSIS_MOCK_H */
