/* lib/squid/link.c */
#include "squid/snet.h"
#include "internal.h"

bool snet_link_is_up(void)
{
    /*
     * The link is "up" whenever the handshake has completed and
     * the peer has not restarted or timed out.  This covers both
     * SNET_ENG_CONNECTED (idle) and SNET_ENG_WAITING (DATA sent,
     * waiting for ACK).  link_up is cleared only by
     * _set_disconnected() and _peer_restarted().
     */
    return (g_snet.link_up != 0u);
}
