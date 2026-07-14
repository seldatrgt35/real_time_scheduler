#ifndef RTS_ASSERT_INTERNAL_H
#define RTS_ASSERT_INTERNAL_H

#include <stdint.h>

#include "rts/rts_types.h"

#define RTS_TASK_VALIDATION_MAGIC UINT32_C(0x52545354)

void rts_assert_fail(const char *expression, const char *file, int line);

#if RTS_ENABLE_ASSERTIONS
#define RTS_ASSERT(expression) \
    ((expression) ? (void)0 : rts_assert_fail(#expression, __FILE__, __LINE__))
#else
#define RTS_ASSERT(expression) ((void)0)
#endif

#define RTS_FATAL_UNLESS(expression) \
    ((expression) ? (void)0 : rts_assert_fail(#expression, __FILE__, __LINE__))

#endif /* RTS_ASSERT_INTERNAL_H */
