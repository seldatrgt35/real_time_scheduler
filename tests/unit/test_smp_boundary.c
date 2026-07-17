#include <stdint.h>

#include "kernel_lock.h"
#include "port_internal.h"
#include "scheduler_internal.h"

static int failures;

#define CHECK(condition) do { if (!(condition)) { ++failures; } } while (0)

_Static_assert(RTS_CPU_COUNT == 1u,
               "release contract requires one CPU");

int main(void)
{
    rts_kernel_state_t *kernel = rts_kernel_state_get();
    rts_tcb_t task = {0};
    rts_kernel_lock_token_t token;

    *kernel = (rts_kernel_state_t){0};
    rts_host_port_test_reset();

    CHECK(rts_cpu_current_id() == RTS_BOOT_CPU_ID);
    CHECK(rts_cpu_id_is_valid(RTS_BOOT_CPU_ID));
    CHECK(!rts_cpu_id_is_valid(UINT32_C(1)));
    CHECK(rts_scheduler_cpu_local(RTS_BOOT_CPU_ID) == &kernel->cpu_local);
    CHECK(rts_scheduler_switch_plan_on_cpu(RTS_BOOT_CPU_ID) ==
          &kernel->cpu_local.switch_plan);

    rts_scheduler_set_current_on_cpu(RTS_BOOT_CPU_ID, &task);
    CHECK(rts_scheduler_current_on_cpu(RTS_BOOT_CPU_ID) == &task);
    CHECK(rts_scheduler_current_get() == &task);
    rts_scheduler_set_current_on_cpu(RTS_BOOT_CPU_ID, NULL);

    token = rts_kernel_lock_enter();
    CHECK(rts_host_port_test_critical_depth() == 1u);
    rts_kernel_lock_exit(token);
    CHECK(rts_host_port_test_critical_depth() == 0u);

    rts_port_request_reschedule(RTS_BOOT_CPU_ID);
    CHECK(rts_host_port_test_switch_request_count() == 1u);
    return failures;
}
