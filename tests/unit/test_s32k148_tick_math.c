#include <stdint.h>

#include "target_tick.h"

static int test_failures;
#define CHECK(condition) do { if (!(condition)) { ++test_failures; } } while (0)

int main(void)
{
    uint32_t reload = UINT32_MAX;

    CHECK(rts_s32k148_tick_reload_calculate(UINT32_C(48000000), 1000u,
                                            &reload));
    CHECK(reload == UINT32_C(47999));
    CHECK(rts_s32k148_tick_reload_calculate(UINT32_C(48000000),
                                            UINT32_C(48000000), &reload));
    CHECK(reload == 0u);
    CHECK(!rts_s32k148_tick_reload_calculate(0u, 1000u, &reload));
    CHECK(!rts_s32k148_tick_reload_calculate(UINT32_C(48000000), 0u,
                                             &reload));
    CHECK(!rts_s32k148_tick_reload_calculate(1000u, 1001u, &reload));
    CHECK(!rts_s32k148_tick_reload_calculate(UINT32_C(48000000), 1001u,
                                             &reload));
    CHECK(!rts_s32k148_tick_reload_calculate(UINT32_C(48000000), 1u,
                                             &reload));
    CHECK(!rts_s32k148_tick_reload_calculate(UINT32_C(48000000), 1000u,
                                             NULL));
    return test_failures;
}
