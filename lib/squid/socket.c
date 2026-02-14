/* lib/squid/socket.c – multiplexed socket API over snet */
#include "internal.h"

static snet_chan_t *_find_chan(uint8_t id)
{
    snet_chan_t *ch = g_snet.chan_head;
    while (ch) {
        if (ch->id == id) return ch;
        ch = ch->next;
    }
    return (snet_chan_t*)0;
}

static void _free_queue(snet_node_t *head)
{
    while (head) {
        snet_node_t *next = head->next;
        g_snet.plat->free(head);
        head = next;
    }
}

int squid_open(void)
{
    if (!g_snet.plat || g_snet.eng == SNET_ENG_DISCONNECTED) return -1;

    /* find first free channel id (1..15; 0 is SYS) */
    for (uint8_t id = 1; id <= 15; id++) {
        if (!(g_snet.used_mask & (1u << id))) {
            snet_chan_t *ch = (snet_chan_t*)g_snet.plat->malloc(
                (uint16_t)sizeof(snet_chan_t));
            if (!ch) return -1;
            memset(ch, 0, sizeof(snet_chan_t));
            ch->id = id;
            /* prepend to list */
            ch->next = g_snet.chan_head;
            g_snet.chan_head = ch;
            g_snet.used_mask |= (uint16_t)(1u << id);
            return (int)id;
        }
    }
    return -1;  /* all channels in use */
}

void squid_close(int ch_id)
{
    if (ch_id < 1 || ch_id > 15) return;
    if (!g_snet.plat) return;

    snet_chan_t **pp = &g_snet.chan_head;
    while (*pp) {
        snet_chan_t *c = *pp;
        if (c->id == (uint8_t)ch_id) {
            /* drain queues — free all malloc'd blocks */
            _free_queue(c->tx_head);
            _free_queue(c->rx_head);
            *pp = c->next;
            g_snet.plat->free(c);
            g_snet.used_mask &= ~(uint16_t)(1u << ch_id);
            return;
        }
        pp = &c->next;
    }
}

int squid_send(int ch_id, const uint8_t *data, uint16_t len)
{
    if (ch_id < 1 || ch_id > 15 || !data || len == 0) return -1;
    if (!g_snet.plat) return -1;

    snet_chan_t *ch = _find_chan((uint8_t)ch_id);
    if (!ch) return -1;

    /* check capacity */
    if (ch->tx_cap && (ch->tx_bytes + len > ch->tx_cap)) return -1;

    /* allocate a block for the data and append to TX queue */
    snet_node_t *n = (snet_node_t*)g_snet.plat->malloc(
        (uint16_t)(sizeof(snet_node_t) + len));
    if (!n) return -1;
    n->next = (snet_node_t*)0;
    n->len  = len;
    n->off  = 0;
    memcpy(n->data, data, len);

    if (ch->tx_tail) ch->tx_tail->next = n; else ch->tx_head = n;
    ch->tx_tail = n;
    ch->tx_bytes += len;
    return (int)len;
}

int squid_recv(int ch_id, uint8_t *buf, uint16_t max)
{
    if (ch_id < 1 || ch_id > 15 || !buf || max == 0) return -1;
    if (!g_snet.plat) return -1;

    snet_chan_t *ch = _find_chan((uint8_t)ch_id);
    if (!ch) return -1;

    /* copy queued RX blocks into caller's buffer, freeing as we go */
    uint16_t total = 0;
    while (ch->rx_head && total < max) {
        snet_node_t *n = ch->rx_head;
        uint16_t avail = n->len - n->off;
        uint16_t take  = (avail > (max - total)) ? (max - total) : avail;
        memcpy(buf + total, n->data + n->off, take);
        n->off += take;
        total  += take;
        ch->rx_bytes -= take;
        if (n->off >= n->len) {         /* block fully consumed — release */
            ch->rx_head = n->next;
            if (!ch->rx_head) ch->rx_tail = (snet_node_t*)0;
            g_snet.plat->free(n);
        }
    }
    return (int)total;
}
