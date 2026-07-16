#include "S32K148.h"

SysTick_Type rts_test_systick;
SCB_Type rts_test_scb;
uint32_t rts_test_primask;
uint32_t rts_test_ipsr;
uint32_t rts_test_psp;
uint32_t rts_test_msp;
uint32_t rts_test_control;
unsigned int rts_test_dsb_count;
unsigned int rts_test_isb_count;
