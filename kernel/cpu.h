#ifndef RTS_CPU_H
#define RTS_CPU_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t rts_cpu_id_t;

#define RTS_BOOT_CPU_ID UINT32_C(0)

static inline rts_cpu_id_t rts_cpu_current_id(void)
{
    return RTS_BOOT_CPU_ID;
}

static inline bool rts_cpu_id_is_valid(rts_cpu_id_t cpu)
{
    return cpu == RTS_BOOT_CPU_ID;
}

#endif /* RTS_CPU_H */
