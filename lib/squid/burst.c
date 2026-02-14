/* lib/squid/burst.c – core protocol engine: one RX + one TX per call. */
#include "internal.h"

/* ------------------------------------------------------------------ */
/*  Frame layout (20 bytes):                                          */
/*   [0]  STX   0x7E                                                  */
/*   [1]  CHLEN  CH(7..4) | LEN(3..0)                                */
/*   [2]  CTRL   TYP(7..5) | STS(4) | SEQ(3) | RES(2..0)            */
/*   [3..17] payload (15 bytes max, LEN valid)                        */
/*   [18] HSH   XOR of bytes 1..17                                   */
/*   [19] ETX   0xD3                                                  */
/* ------------------------------------------------------------------ */
#define F_STX   0
#define F_CHLEN 1
#define F_CTRL  2
#define F_PAY   3
#define F_HSH   (SNET_FRAME_BYTES - 2)
#define F_ETX   (SNET_FRAME_BYTES - 1)

/* ---- tick helpers (8-bit wraparound safe) ---- */
static uint8_t _elapsed(uint8_t since)
{
    return (uint8_t)(g_snet.plat->get_tick() - since);
}

static void _set_connected(void)
{
    g_snet.seq_tx = 0u;
    g_snet.seq_expect = 0u;
    g_snet.eng = SNET_ENG_CONNECTED;
    g_snet.link_up = 1u;
    g_snet.retries = 0u;
}

static void _set_disconnected(void)
{
    g_snet.eng = SNET_ENG_DISCONNECTED;
    g_snet.link_up = 0u;
}

static void _peer_restarted(void)
{
    g_snet.eng = SNET_ENG_STARTUP;
    g_snet.link_up = 0u;
}

static void _schedule_ack(void)
{
    g_snet.ack_needed = 1u;
    g_snet.ack_wait = g_snet.plat->get_tick();
}

/* ---- XOR hash over bytes 1..17 ---- */
static uint8_t _hash(const uint8_t *frame)
{
    uint8_t h = 0;
    for (uint8_t i = 1; i <= F_HSH - 1; i++) h ^= frame[i];
    return h;
}

/* ---- send a raw frame (20 bytes) ---- */
static void _send_frame(const uint8_t *frame)
{
    for (uint8_t i = 0; i < SNET_FRAME_BYTES; i++)
        g_snet.plat->send_char(frame[i]);
    memcpy(g_snet.last_sent, frame, SNET_FRAME_BYTES);
    g_snet.last_tx_tick = g_snet.plat->get_tick();
}

/* ---- resend last frame ---- */
static void _resend(void)
{
    for (uint8_t i = 0; i < SNET_FRAME_BYTES; i++)
        g_snet.plat->send_char(g_snet.last_sent[i]);
    g_snet.last_tx_tick = g_snet.plat->get_tick();
}

/* ---- build and send a frame ---- */
static void _build_and_send(uint8_t typ, uint8_t sts, uint8_t ch,
                            const uint8_t *payload, uint8_t len)
{
    uint8_t frame[SNET_FRAME_BYTES];
    memset(frame, 0, SNET_FRAME_BYTES);
    frame[F_STX]   = SNET_STX;
    frame[F_CHLEN] = SNET_MAKE_CHLEN(ch, len);
    frame[F_CTRL]  = SNET_MAKE_CTRL(typ, sts, g_snet.seq_tx);
    if (payload && len > 0) {
        uint8_t n = (len > SNET_PAY_MAX) ? SNET_PAY_MAX : len;
        memcpy(&frame[F_PAY], payload, n);
    }
    frame[F_HSH] = _hash(frame);
    frame[F_ETX] = SNET_ETX;
    _send_frame(frame);
}

/* ---- find channel by id ---- */
static snet_chan_t *_find_chan(uint8_t id)
{
    snet_chan_t *c = g_snet.chan_head;
    while (c) { if (c->id == id) return c; c = c->next; }
    return (snet_chan_t*)0;
}

/* ---- enqueue received payload into channel RX queue ---- */
static void _enqueue_rx(uint8_t ch_id, const uint8_t *data, uint8_t len)
{
    if (len == 0) return;
    snet_chan_t *ch = _find_chan(ch_id);
    if (!ch) return;                    /* channel not open, discard */
    if (ch->rx_cap && (ch->rx_bytes + len > ch->rx_cap)) return; /* full */

    snet_node_t *n = (snet_node_t*)g_snet.plat->malloc(
        (uint16_t)(sizeof(snet_node_t) + len));
    if (!n) return;
    n->next = (snet_node_t*)0;
    n->len  = len;
    n->off  = 0;
    memcpy(n->data, data, len);

    if (ch->rx_tail) ch->rx_tail->next = n; else ch->rx_head = n;
    ch->rx_tail = n;
    ch->rx_bytes += len;
}

static void _accept_data(uint8_t ch_id, uint8_t len)
{
    _enqueue_rx(ch_id, &g_snet.rx_buf[F_PAY], len);
    g_snet.seq_expect ^= 1u;
    _schedule_ack();
}

/* ---- dequeue payload from channel TX queue (up to SNET_PAY_MAX) ---- */
static uint8_t _dequeue_tx(snet_chan_t *ch, uint8_t *out)
{
    uint8_t total = 0;
    while (ch->tx_head && total < SNET_PAY_MAX) {
        snet_node_t *n = ch->tx_head;
        uint16_t avail = n->len - n->off;
        uint8_t  take  = (avail > (SNET_PAY_MAX - total))
                         ? (SNET_PAY_MAX - total) : (uint8_t)avail;
        memcpy(out + total, n->data + n->off, take);
        n->off += take;
        total  += take;
        ch->tx_bytes -= take;
        if (n->off >= n->len) {         /* node consumed */
            ch->tx_head = n->next;
            if (!ch->tx_head) ch->tx_tail = (snet_node_t*)0;
            g_snet.plat->free(n);
        }
    }
    return total;
}

/* ---- pick next channel with data (round-robin) ---- */
static snet_chan_t *_next_tx_chan(void)
{
    if (!g_snet.chan_head) return (snet_chan_t*)0;

    /* start after last served channel */
    uint8_t start = (uint8_t)(g_snet.rr_last_id + 1u);
    for (uint8_t pass = 0; pass < 16; pass++) {
        uint8_t id = (uint8_t)((start + pass) & 0x0Fu);
        snet_chan_t *c = _find_chan(id);
        if (c && c->tx_head) {
            g_snet.rr_last_id = id;
            return c;
        }
    }
    return (snet_chan_t*)0;
}

/* ================================================================== */
/*  RX: try to receive one complete frame                             */
/* ================================================================== */
static void _rx(void)
{
    /* read bytes until we have a full frame or no more data */
    for (;;) {
        int b = g_snet.plat->recv_char();
        if (b < 0) return;             /* no data available */

        uint8_t c = (uint8_t)b;

        /* sync on STX */
        if (g_snet.rx_pos == 0) {
            if (c == SNET_STX) g_snet.rx_buf[g_snet.rx_pos++] = c;
            continue;                   /* skip garbage */
        }

        g_snet.rx_buf[g_snet.rx_pos++] = c;

        if (g_snet.rx_pos < SNET_FRAME_BYTES)
            continue;                   /* frame not complete yet */

        /* ---- full frame received ---- */
        g_snet.rx_pos = 0;             /* reset for next frame */

        /* validate ETX and hash */
        if (g_snet.rx_buf[F_ETX] != SNET_ETX) break;
        if (_hash(g_snet.rx_buf) != g_snet.rx_buf[F_HSH]) break;

        /* parse header */
        uint8_t ctrl  = g_snet.rx_buf[F_CTRL];
        uint8_t chlen = g_snet.rx_buf[F_CHLEN];
        uint8_t typ   = SNET_GET_TYP(ctrl);
        uint8_t seq   = SNET_GET_SEQ(ctrl);
        uint8_t ch_id = SNET_GET_CH(chlen);
        uint8_t len   = SNET_GET_LEN(chlen);

        switch (g_snet.eng) {

        case SNET_ENG_STARTUP:
            if (typ == SNET_TYP_HELLO) {
                /* peer says hello — reply with HELLO_ACK */
                _build_and_send(SNET_TYP_HELLO_ACK, 0, SNET_CH_SYS,
                                (const uint8_t*)0, 0);
                _set_connected();
            } else if (typ == SNET_TYP_HELLO_ACK) {
                /* our HELLO was accepted */
                _set_connected();
            }
            break;

        case SNET_ENG_WAITING:
            /* we are waiting for ACK of the last DATA we sent */
            if (typ == SNET_TYP_ACK || typ == SNET_TYP_DATA) {
                /* any valid frame in WAITING means peer is alive;
                   for ACK, STS=0 means positive ack */
                if (SNET_GET_STS(ctrl) == 0u) {
                    /* positive ACK — advance TX seq */
                    g_snet.seq_tx ^= 1u;
                    g_snet.retries = 0u;
                    g_snet.eng = SNET_ENG_CONNECTED;
                }
                /* if it also carries DATA, accept it */
                if (typ == SNET_TYP_DATA && seq == g_snet.seq_expect) {
                    _accept_data(ch_id, len);
                }
            } else if (typ == SNET_TYP_HELLO) {
                /* peer restarted — go back to startup */
                _peer_restarted();
            }
            break;

        case SNET_ENG_CONNECTED:
            if (typ == SNET_TYP_DATA) {
                if (seq == g_snet.seq_expect) {
                    /* new data — accept */
                    _accept_data(ch_id, len);
                }
                /* duplicate (seq != expected) — just re-ACK below */
            } else if (typ == SNET_TYP_ACK) {
                /* pure ACK — already connected, nothing extra */
            } else if (typ == SNET_TYP_PING) {
                /* respond with ACK */
                _schedule_ack();
            } else if (typ == SNET_TYP_HELLO) {
                /* peer restarted */
                _peer_restarted();
            }
            break;

        case SNET_ENG_DISCONNECTED:
            /* ignore everything while disconnected */
            break;
        }

        break;  /* process at most one complete frame per burst */
    }
}

/* ================================================================== */
/*  TX: send at most one frame                                        */
/* ================================================================== */
static void _tx(void)
{
    switch (g_snet.eng) {

    case SNET_ENG_STARTUP:
        /* periodically send HELLO */
        if (_elapsed(g_snet.last_tx_tick) >= g_snet.timeout_ticks) {
            _build_and_send(SNET_TYP_HELLO, 0, SNET_CH_SYS,
                            (const uint8_t*)0, 0);
            g_snet.retries++;
            if (g_snet.retries > g_snet.max_retries) {
                _set_disconnected();
            }
        }
        break;

    case SNET_ENG_WAITING:
        /* resend on timeout */
        if (_elapsed(g_snet.last_tx_tick) >= g_snet.timeout_ticks) {
            g_snet.retries++;
            if (g_snet.retries > g_snet.max_retries) {
                _set_disconnected();
            } else {
                _resend();
            }
        }
        break;

    case SNET_ENG_CONNECTED: {
        /* 1) if we owe an ACK and delay expired, send it */
        if (g_snet.ack_needed &&
            _elapsed(g_snet.ack_wait) >= g_snet.ack_delay_ticks) {
            /* try to piggyback ACK on DATA if available */
            snet_chan_t *ch = _next_tx_chan();
            if (ch) {
                uint8_t pay[SNET_PAY_MAX];
                uint8_t n = _dequeue_tx(ch, pay);
                _build_and_send(SNET_TYP_DATA, 0, ch->id, pay, n);
                g_snet.eng = SNET_ENG_WAITING;
            } else {
                _build_and_send(SNET_TYP_ACK, 0, SNET_CH_SYS,
                                (const uint8_t*)0, 0);
            }
            g_snet.ack_needed = 0u;
            break;
        }

        /* 2) send queued DATA */
        snet_chan_t *ch = _next_tx_chan();
        if (ch) {
            uint8_t pay[SNET_PAY_MAX];
            uint8_t n = _dequeue_tx(ch, pay);
            _build_and_send(SNET_TYP_DATA, 0, ch->id, pay, n);
            g_snet.eng = SNET_ENG_WAITING;
            break;
        }

        /* 3) ping keepalive */
        if (g_snet.ping_ticks &&
            _elapsed(g_snet.last_ping_tick) >= g_snet.ping_ticks) {
            _build_and_send(SNET_TYP_PING, 0, SNET_CH_SYS,
                            (const uint8_t*)0, 0);
            g_snet.last_ping_tick = g_snet.plat->get_tick();
        }
        break;
    }

    case SNET_ENG_DISCONNECTED:
        /* wait for timeout then restart */
        if (_elapsed(g_snet.last_tx_tick) >= g_snet.timeout_ticks) {
            g_snet.eng     = SNET_ENG_STARTUP;
            g_snet.retries = 0u;
            g_snet.seq_tx     = 0u;
            g_snet.seq_expect = 0u;
        }
        break;
    }
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */
void snet_burst(void)
{
    if (!g_snet.plat) return;
    _rx();
    _tx();
}
