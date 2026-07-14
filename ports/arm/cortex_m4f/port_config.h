#ifndef RTS_CORTEX_M4F_PORT_CONFIG_H
#define RTS_CORTEX_M4F_PORT_CONFIG_H

/* Supplied by the selected Cortex-M target configuration as logical values. */
#if !defined(RTS_CORTEX_M_NVIC_PRIORITY_BITS)
#error "Target must define RTS_CORTEX_M_NVIC_PRIORITY_BITS"
#endif

#if !defined(RTS_CORTEX_M_PENDSV_PRIORITY)
#error "Target must define RTS_CORTEX_M_PENDSV_PRIORITY"
#endif

#if !defined(RTS_CORTEX_M_SVC_PRIORITY)
#error "Target must define RTS_CORTEX_M_SVC_PRIORITY"
#endif

#if (RTS_CORTEX_M_NVIC_PRIORITY_BITS < 2) || \
    (RTS_CORTEX_M_NVIC_PRIORITY_BITS > 8)
#error "Implemented NVIC priority bits must be in the range 2..8"
#endif

#define RTS_CORTEX_M_LOWEST_LOGICAL_PRIORITY \
    ((1u << RTS_CORTEX_M_NVIC_PRIORITY_BITS) - 1u)

#if (RTS_CORTEX_M_PENDSV_PRIORITY != RTS_CORTEX_M_LOWEST_LOGICAL_PRIORITY)
#error "PendSV must use the lowest implemented logical priority"
#endif

#if (RTS_CORTEX_M_SVC_PRIORITY > RTS_CORTEX_M_LOWEST_LOGICAL_PRIORITY)
#error "SVC priority is outside the implemented logical range"
#endif

#endif /* RTS_CORTEX_M4F_PORT_CONFIG_H */
