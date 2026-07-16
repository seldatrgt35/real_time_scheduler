#include "S32K148.h"

SysTick_Type rts_test_systick;
SCB_Type rts_test_scb;
FPU_Type rts_test_fpu;
DWT_Type rts_test_dwt;
CoreDebug_Type rts_test_core_debug;
LPTMR_Type rts_test_lptmr0;
PCC_Type rts_test_pcc;
uint32_t rts_test_nvic_enabled[8];
uint32_t rts_test_nvic_priority[256];
unsigned int rts_test_wfi_count;
void (*rts_test_wfi_hook)(void);
uint32_t rts_test_primask;
uint32_t rts_test_ipsr;
uint32_t rts_test_psp;
uint32_t rts_test_msp;
uint32_t rts_test_control;
unsigned int rts_test_dsb_count;
unsigned int rts_test_isb_count;
