#ifndef RTS_INVARIANT_CHECK_INTERNAL_H
#define RTS_INVARIANT_CHECK_INTERNAL_H

#include <stdbool.h>

#include "task_internal.h"

bool rts_task_validate_internal(const rts_tcb_t *task);
bool rts_scheduler_validate_internal(void);
bool rts_sync_validate_internal(void);
bool rts_kernel_validate_all(void);

#endif /* RTS_INVARIANT_CHECK_INTERNAL_H */
