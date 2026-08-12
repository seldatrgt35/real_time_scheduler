#include "osif.h"

status_t OSIF_SemaCreate(semaphore_t * const semaphore,
                         const uint8_t initial_value)
{
    *semaphore = initial_value;
    return STATUS_SUCCESS;
}

status_t OSIF_SemaDestroy(const semaphore_t * const semaphore)
{
    (void)semaphore;
    return STATUS_SUCCESS;
}

status_t OSIF_SemaPost(semaphore_t * const semaphore)
{
    *semaphore = 1U;
    return STATUS_SUCCESS;
}

status_t OSIF_SemaWait(semaphore_t * const semaphore, const uint32_t timeout)
{
    (void)timeout;
    if (*semaphore == 0U)
    {
        return STATUS_TIMEOUT;
    }
    *semaphore = 0U;
    return STATUS_SUCCESS;
}
