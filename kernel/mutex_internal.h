#ifndef RTS_MUTEX_INTERNAL_H
#define RTS_MUTEX_INTERNAL_H

#include <stdbool.h>

#include "rts/rts_mutex.h"
#include "scheduler_internal.h"

#define RTS_MUTEX_SIGNATURE UINT32_C(0x5254534d)

bool rts_mutex_is_valid(const rts_mutex_t *mutex);
bool rts_mutex_timeout_task(rts_kernel_state_t *kernel, rts_tcb_t *task);
rts_status_t rts_mutex_wait_result_consume(rts_tcb_t *task);

#endif /* RTS_MUTEX_INTERNAL_H */
