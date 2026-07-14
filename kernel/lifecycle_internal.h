#ifndef RTS_LIFECYCLE_INTERNAL_H
#define RTS_LIFECYCLE_INTERNAL_H

#include <stdint.h>

typedef uint8_t rts_kernel_lifecycle_t;
enum
{
    RTS_KERNEL_RESET = 0,
    RTS_KERNEL_INITIALIZED,
    RTS_KERNEL_RUNNING
};

#endif /* RTS_LIFECYCLE_INTERNAL_H */
