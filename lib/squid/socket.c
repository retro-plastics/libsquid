/* lib/squid/socket.c – multiplexed socket API over snet */
#include "internal.h"

static snet_chan_t *_find_by_fd(uint8_t fd)
{
    snet_chan_t *ch = g_snet.chan_head;
    while (ch) {
        if (ch->fd == fd) return ch;
        ch = ch->next;
    }
    return (snet_chan_t*)0;
}

static snet_chan_t *_find_by_channel(uint8_t ch_id)
{
    snet_chan_t *ch = g_snet.chan_head;
    while (ch) {
        if (ch->ch_id == ch_id) return ch;
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

    /* find first free local fd (1..15) */
    for (uint8_t fd = 1; fd <= 15; fd++) {
        if (!(g_snet.fd_mask & (1u << fd))) {
            snet_chan_t *ch = (snet_chan_t*)g_snet.plat->malloc(
                (uint16_t)sizeof(snet_chan_t));
            if (!ch) return -1;
            memset(ch, 0, sizeof(snet_chan_t));
            ch->fd = fd;
            /* prepend to list */
            ch->next = g_snet.chan_head;
            g_snet.chan_head = ch;
            g_snet.fd_mask |= (uint16_t)(1u << fd);
            return (int)fd;
        }
    }
    return -1;  /* all local fds in use */
}

int squid_bind(int fd, uint8_t ch_id)
{
    if (!g_snet.plat) return -1;
    if (fd < 1 || fd > 15) return -1;
    if (ch_id < 1 || ch_id > 15) return -1;

    snet_chan_t *sock = _find_by_fd((uint8_t)fd);
    if (!sock) return -1;

    /* requested channel already owned by another fd */
    snet_chan_t *owner = _find_by_channel(ch_id);
    if (owner && owner != sock) return -1;

    if (sock->ch_id != 0u) {
        g_snet.ch_mask &= (uint16_t)~(1u << sock->ch_id);
    }
    sock->ch_id = ch_id;
    g_snet.ch_mask |= (uint16_t)(1u << ch_id);
    return 0;
}

int squid_connect(int fd, uint8_t ch_id)
{
    /* Current protocol is symmetric; connect is local channel attach. */
    return squid_bind(fd, ch_id);
}

void squid_close(int fd)
{
    if (fd < 1 || fd > 15) return;
    if (!g_snet.plat) return;

    snet_chan_t **pp = &g_snet.chan_head;
    while (*pp) {
        snet_chan_t *c = *pp;
        if (c->fd == (uint8_t)fd) {
            uint8_t bound_ch = c->ch_id;
            /* drain queues — free all malloc'd blocks */
            _free_queue(c->tx_head);
            _free_queue(c->rx_head);
            *pp = c->next;
            g_snet.plat->free(c);
            g_snet.fd_mask &= (uint16_t)~(1u << fd);
            if (bound_ch != 0u) {
                g_snet.ch_mask &= (uint16_t)~(1u << bound_ch);
            }
            return;
        }
        pp = &c->next;
    }
}

int squid_send(int fd, const uint8_t *data, uint16_t len)
{
    if (fd < 1 || fd > 15 || !data || len == 0) return -1;
    if (!g_snet.plat) return -1;

    snet_chan_t *sock = _find_by_fd((uint8_t)fd);
    if (!sock || sock->ch_id == 0u) return -1;

    /* check capacity */
    if (sock->tx_cap && (sock->tx_bytes + len > sock->tx_cap)) return -1;

    /* allocate a block for the data and append to TX queue */
    snet_node_t *n = (snet_node_t*)g_snet.plat->malloc(
        (uint16_t)(sizeof(snet_node_t) + len));
    if (!n) return -1;
    n->next = (snet_node_t*)0;
    n->len  = len;
    n->off  = 0;
    memcpy(n->data, data, len);

    if (sock->tx_tail) sock->tx_tail->next = n; else sock->tx_head = n;
    sock->tx_tail = n;
    sock->tx_bytes += len;
    return (int)len;
}

int squid_recv(int fd, uint8_t *buf, uint16_t max)
{
    if (fd < 1 || fd > 15 || !buf || max == 0) return -1;
    if (!g_snet.plat) return -1;

    snet_chan_t *sock = _find_by_fd((uint8_t)fd);
    if (!sock || sock->ch_id == 0u) return -1;

    /* copy queued RX blocks into caller's buffer, freeing as we go */
    uint16_t total = 0;
    while (sock->rx_head && total < max) {
        snet_node_t *n = sock->rx_head;
        uint16_t avail = n->len - n->off;
        uint16_t take  = (avail > (max - total)) ? (max - total) : avail;
        memcpy(buf + total, n->data + n->off, take);
        n->off += take;
        total  += take;
        sock->rx_bytes -= take;
        if (n->off >= n->len) {         /* block fully consumed — release */
            sock->rx_head = n->next;
            if (!sock->rx_head) sock->rx_tail = (snet_node_t*)0;
            g_snet.plat->free(n);
        }
    }
    return (int)total;
}
