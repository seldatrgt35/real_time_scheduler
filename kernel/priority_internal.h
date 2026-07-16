#ifndef RTS_PRIORITY_INTERNAL_H
#define RTS_PRIORITY_INTERNAL_H

#include <stdbool.h>

#include "task_internal.h"

bool rts_priority_set_effective(rts_tcb_t *task,
                                rts_priority_t effective_priority);
bool rts_priority_recompute_chain(rts_tcb_t *task);

#endif /* RTS_PRIORITY_INTERNAL_H */
