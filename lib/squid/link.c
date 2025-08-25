/* lib/squid/link.c */
#include "squid/snet.h"
#include "internal.h"

bool snet_link_is_up(void)
{
    /* report link-up strictly from the FSM state */
    return (g_snet.eng == SNET_ENG_CONNECTED);
}
