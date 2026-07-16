#ifndef RTS_STACK_CHECK_INTERNAL_H
#define RTS_STACK_CHECK_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "task_internal.h"

#define RTS_STACK_GUARD_PATTERN UINT8_C(0xa5)
#define RTS_STACK_FILL_PATTERN  UINT8_C(0xcd)

void rts_stack_diagnostics_prepare(unsigned char *stack_low,
                                   unsigned char *stack_high);
bool rts_stack_guard_is_valid(const rts_tcb_t *task);
bool rts_stack_saved_sp_is_valid(const rts_tcb_t *task);
size_t rts_stack_high_water_used_bytes(const rts_tcb_t *task);
size_t rts_stack_watermark_update(rts_tcb_t *task);

#endif /* RTS_STACK_CHECK_INTERNAL_H */
