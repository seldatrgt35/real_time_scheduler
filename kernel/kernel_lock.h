#ifndef RTS_KERNEL_LOCK_H
#define RTS_KERNEL_LOCK_H

#include "port.h"

typedef struct
{
    rts_critical_token_t interrupt_token;
} rts_kernel_lock_token_t;

static inline rts_kernel_lock_token_t rts_kernel_lock_enter(void)
{
    const rts_kernel_lock_token_t token = {
        .interrupt_token = rts_port_critical_enter()
    };
    return token;
}

static inline void rts_kernel_lock_exit(rts_kernel_lock_token_t token)
{
    rts_port_critical_exit(token.interrupt_token);
}

#endif /* RTS_KERNEL_LOCK_H */
