/* lib/squid/init.c */
#include "internal.h"   /* provides g_snet context, types, and state enums */

/* free all dynamic channels and their queued nodes (used on re-init) */
static void _free_all_channels(void)
{
    snet_chan_t *c = g_snet.chan_head;
    while (c) {
        snet_node_t *n = c->tx_head;                 /* drop TX queue */
        while (n) { snet_node_t *nx = n->next; g_snet.plat->free(n); n = nx; }
        n = c->rx_head;                              /* drop RX queue */
        while (n) { snet_node_t *nx = n->next; g_snet.plat->free(n); n = nx; }
        snet_chan_t *nextc = c->next;                /* unlink channel */
        g_snet.plat->free(c);
        c = nextc;
    }
    g_snet.chan_head  = (snet_chan_t*)0;             /* allocator reset */
    g_snet.used_mask  = 0u;
    g_snet.rr_last_id = 0xFFu;
}

void snet_init(const snet_platform_t *plat, const snet_timing_t *tm)
{
    if (g_snet.plat && g_snet.plat->free) _free_all_channels();     /* clean old state */
    memset(&g_snet, 0, sizeof(g_snet));                             /* hard reset ctx */

    g_snet.plat = plat;                                             /* install hooks */
    if (!plat || !plat->send_char || !plat->recv_char ||            /* validate req */
        !plat->get_tick || !plat->malloc || !plat->free) {
        g_snet.eng = SNET_ENG_DISCONNECTED;                         /* stay disabled */
        return;
    }

    if (tm) {                                                       /* copy timing */
        g_snet.timeout_ticks   = tm->timeout_ticks;
        g_snet.ack_delay_ticks = tm->ack_delay_ticks;
        g_snet.ping_ticks      = tm->ping_ticks;
        g_snet.max_retries     = tm->max_retries;
    }
    if (!g_snet.timeout_ticks)   g_snet.timeout_ticks   = 6u;       /* fill defaults */
    if (!g_snet.ack_delay_ticks) g_snet.ack_delay_ticks = 2u;
    /* ping_ticks: 0 = disabled */
    if (!g_snet.max_retries)     g_snet.max_retries     = 3u;

    g_snet.eng         = SNET_ENG_STARTUP;                          /* FSM reset */
    g_snet.seq_tx      = 0u;
    g_snet.seq_expect  = 0u;
    g_snet.retries     = 0u;
    g_snet.last_tx_tick   = 0u;                                     /* timers clear */
    g_snet.last_ping_tick = 0u;
    g_snet.ack_needed  = 0u;                                        /* no pending ACK */
    g_snet.ack_wait    = 0u;
    g_snet.link_up     = 0u;                                        /* handshake not done */

    g_snet.chan_head   = (snet_chan_t*)0;                           /* no channels yet */
    g_snet.used_mask   = 0u;
    g_snet.rr_last_id  = 0xFFu;
    /* g_snet.last_sent[] already zero from memset */
}
