#ifndef RTS_S32K148_TARGET_CONFIG_H
#define RTS_S32K148_TARGET_CONFIG_H

#include <stdint.h>

#define RTS_S32K148_VECTOR_COUNT              256u
#define RTS_S32K148_MSP_STACK_SIZE_BYTES      4096u
#define RTS_S32K148_NVIC_PRIORITY_BITS        4u
#define RTS_S32K148_CORE_CLOCK_HZ              UINT32_C(48000000)
#define RTS_S32K148_SVC_LOGICAL_PRIORITY      13u
#define RTS_S32K148_SYSTICK_LOGICAL_PRIORITY  14u
#define RTS_S32K148_PENDSV_LOGICAL_PRIORITY   15u

#if RTS_S32K148_NVIC_PRIORITY_BITS != RTS_CORTEX_M_NVIC_PRIORITY_BITS
#error "S32K148 target and Cortex port priority-bit declarations differ"
#endif
#if RTS_S32K148_SVC_LOGICAL_PRIORITY != RTS_CORTEX_M_SVC_PRIORITY
#error "S32K148 target and Cortex port SVC priorities differ"
#endif
#if RTS_S32K148_PENDSV_LOGICAL_PRIORITY != RTS_CORTEX_M_PENDSV_PRIORITY
#error "S32K148 target and Cortex port PendSV priorities differ"
#endif

#if RTS_S32K148_SYSTICK_LOGICAL_PRIORITY <= RTS_S32K148_SVC_LOGICAL_PRIORITY
#error "SysTick must have lower urgency than SVC"
#endif
#if RTS_S32K148_SYSTICK_LOGICAL_PRIORITY >= RTS_S32K148_PENDSV_LOGICAL_PRIORITY
#error "SysTick must have higher urgency than PendSV"
#endif

#endif /* RTS_S32K148_TARGET_CONFIG_H */
