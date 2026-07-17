#ifndef RTS_S32K148_TARGET_DEVICE_H
#define RTS_S32K148_TARGET_DEVICE_H

/*
 * The NXP SDK device header supplies IRQn_Type and the Cortex feature macros.
 * CMSIS core_cm4.h consumes that device-level contract and supplies the core
 * register definitions and intrinsic functions used by the target layer.
 */
#include "S32K148.h"

/* The SDK exposes an MPU peripheral type named MPU_Type. CMSIS uses the same
 * name for its core MPU register block. This project does not enable or access
 * the MPU, so suppress the optional CMSIS MPU declarations while retaining
 * the SCB, DWT, SysTick, NVIC and intrinsic interfaces. */
#undef __MPU_PRESENT
#define __MPU_PRESENT 0U
#include "core_cm4.h"

/* The SDK names the CPACR masks with its S32_ prefix, while the CMSIS SCB
 * register interface uses the CMSIS spelling. Keep the target source portable
 * across the two header conventions. */
#ifndef SCB_CPACR_CP10_Msk
#define SCB_CPACR_CP10_Msk S32_SCB_CPACR_CP10_MASK
#endif
#ifndef SCB_CPACR_CP11_Msk
#define SCB_CPACR_CP11_Msk S32_SCB_CPACR_CP11_MASK
#endif

#endif /* RTS_S32K148_TARGET_DEVICE_H */
