#include "target_led.h"

#include <stdbool.h>
#include <stdint.h>

/* S32K148 EVB-Q176 RGB LED: red is PORTE[21], active low. */
#define RTS_PCC_PORTE              (*(volatile uint32_t *)0x40065134u)
#define RTS_PORTE_PCR21            (*(volatile uint32_t *)0x4004D054u)
#define RTS_GPIOE_PSOR             (*(volatile uint32_t *)0x400FF104u)
#define RTS_GPIOE_PCOR             (*(volatile uint32_t *)0x400FF108u)
#define RTS_GPIOE_PTOR             (*(volatile uint32_t *)0x400FF10Cu)
#define RTS_GPIOE_PDDR             (*(volatile uint32_t *)0x400FF114u)

#define RTS_PCC_CGC                 (UINT32_C(1) << 30)
#define RTS_PORT_MUX_GPIO           (UINT32_C(1) << 8)
#define RTS_RED_LED_MASK            (UINT32_C(1) << 21)

void rts_s32k148_led_initialize(void)
{
    RTS_PCC_PORTE |= RTS_PCC_CGC;
    RTS_PORTE_PCR21 = RTS_PORT_MUX_GPIO;
    RTS_GPIOE_PDDR |= RTS_RED_LED_MASK;
    /* Active-low LED: high is the safe/off reset state. */
    RTS_GPIOE_PSOR = RTS_RED_LED_MASK;
}

void rts_s32k148_red_led_set(bool on)
{
    if (on)
    {
        RTS_GPIOE_PCOR = RTS_RED_LED_MASK;
    }
    else
    {
        RTS_GPIOE_PSOR = RTS_RED_LED_MASK;
    }
}

void rts_s32k148_red_led_toggle(void)
{
    RTS_GPIOE_PTOR = RTS_RED_LED_MASK;
}
