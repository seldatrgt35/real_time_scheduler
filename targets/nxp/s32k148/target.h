#ifndef RTS_S32K148_TARGET_H
#define RTS_S32K148_TARGET_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    volatile uint32_t assertion_failed;
    volatile uint32_t hardfault_seen;
    volatile uint32_t stacked_r0;
    volatile uint32_t stacked_r1;
    volatile uint32_t stacked_r2;
    volatile uint32_t stacked_r3;
    volatile uint32_t stacked_r12;
    volatile uint32_t stacked_lr;
    volatile uint32_t stacked_pc;
    volatile uint32_t stacked_xpsr;
    volatile uint32_t cfsr;
    volatile uint32_t hfsr;
    volatile uint32_t mmfar;
    volatile uint32_t bfar;
    volatile uint32_t active_msp;
    volatile uint32_t active_psp;
    volatile uint32_t exc_return;
    volatile uint32_t handler_probe_msp;
    volatile uint32_t handler_probe_ipsr;
} rts_s32k148_fault_record_t;

extern rts_s32k148_fault_record_t g_rts_s32k148_fault_record;

void rts_s32k148_request_handler_probe(void);
bool rts_s32k148_handler_probe_passed(void);
void rts_s32k148_record_handler_probe(uint32_t msp, uint32_t ipsr);
_Noreturn void rts_s32k148_hardfault_capture(const uint32_t *stacked_frame,
                                             uint32_t exc_return,
                                             uint32_t active_msp,
                                             uint32_t active_psp);

#endif /* RTS_S32K148_TARGET_H */
