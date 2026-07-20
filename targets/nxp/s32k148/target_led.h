#ifndef RTS_S32K148_TARGET_LED_H
#define RTS_S32K148_TARGET_LED_H

#include <stdbool.h>

void rts_s32k148_led_initialize(void);
void rts_s32k148_red_led_set(bool on);
void rts_s32k148_red_led_toggle(void);

#endif
