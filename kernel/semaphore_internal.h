#ifndef RTS_SEMAPHORE_INTERNAL_H
#define RTS_SEMAPHORE_INTERNAL_H

#include <stdbool.h>

#include "rts/rts_semaphore.h"
#include "scheduler_internal.h"

#define RTS_SEMAPHORE_SIGNATURE UINT32_C(0x52545353)

bool rts_semaphore_is_valid(const rts_semaphore_t *semaphore);
bool rts_semaphore_timeout_task(rts_kernel_state_t *kernel,
                                rts_tcb_t *task);
rts_status_t rts_semaphore_wait_result_consume(rts_tcb_t *task);

#endif /* RTS_SEMAPHORE_INTERNAL_H */
