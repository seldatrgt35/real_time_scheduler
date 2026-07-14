#include "scheduler_internal.h"

static rts_kernel_state_t rts_kernel_state;

rts_kernel_state_t *rts_kernel_state_get(void)
{
    return &rts_kernel_state;
}
