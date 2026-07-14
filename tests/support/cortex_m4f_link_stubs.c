/* Link-verification only; real special-register setup belongs to the target. */
#include "port.h"

rts_status_t rts_port_initialize(void)
{
    return RTS_STATUS_OK;
}

rts_critical_token_t rts_port_critical_enter(void)
{
    return (rts_critical_token_t)0u;
}

void rts_port_critical_exit(rts_critical_token_t token)
{
    (void)token;
}

bool rts_port_is_in_isr(void)
{
    return false;
}
