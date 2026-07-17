#include <stdint.h>

#include "port_internal.h"
#include "power_internal.h"
#include "rts/rts.h"
#include "scheduler_internal.h"

static int failures;

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

int main(void)
{
    rts_power_plan_t plan = {0};

    rts_host_port_test_reset();
    rts_host_port_test_start_reset();
    *rts_kernel_state_get() = (rts_kernel_state_t){0};

    CHECK(rts_init() == RTS_STATUS_OK);
    CHECK(rts_start() == RTS_STATUS_OK);
    CHECK(rts_scheduler_current_get() == rts_kernel_state_get()->idle_task);
    CHECK(!rts_power_sleep_is_allowed());
    CHECK(!rts_power_plan_compute(&plan));

    rts_host_port_test_set_next_wake(5u, RTS_PORT_WAKE_TIMER);
    rts_power_idle();
    CHECK(rts_host_port_test_sleep_count() == 0u);
    CHECK(rts_kernel_tick_now() == 0u);
    CHECK(rts_kernel_state_get()->runtime_counters.tickless_sleep_attempts ==
          0u);

    return failures;
}
