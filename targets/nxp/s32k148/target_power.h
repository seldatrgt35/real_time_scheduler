#ifndef RTS_S32K148_TARGET_POWER_H
#define RTS_S32K148_TARGET_POWER_H

#include "rts/rts_types.h"

rts_status_t rts_s32k148_power_initialize(void);
uint32_t rts_s32k148_lptmr_count_read(void);
void LPTMR0_IRQHandler(void);

#endif /* RTS_S32K148_TARGET_POWER_H */
