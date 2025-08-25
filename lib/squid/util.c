#include "internal.h"

/* global context defined in init.c */
extern sq_ctx_t g_sq;

/* ---- small helpers ---- */
uint8_t sq_ctrl_build(uint8_t typ, uint8_t sts_bit, uint8_t seq_bit)
{
    uint8_t v = (uint8_t)((typ << SQ_CTRL_TYP_SHIFT) & SQ_CTRL_TYP_MASK);
    if (sts_bit) v |= SQ_CTRL_STS_MASK;
    if (seq_bit) v |= SQ_CTRL_SEQ_MASK;
    return v; /* RES bits 0 */
}

uint8_t sq_chlen_build(uint8_t ch, uint8_t len)
{
    return (uint8_t)(((ch & 0x0Fu) << SQ_CH_SHIFT) | (len & SQ_LEN_MASK));
}

void sq_chlen_parse(uint8_t chlen, uint8_t *ch, uint8_t *len)
{
    if (ch)  *ch  = (uint8_t)((chlen & SQ_CH_MASK) >> SQ_CH_SHIFT);
    if (len) *len = (uint8_t)(chlen & SQ_LEN_MASK);
}

uint8_t sq_xor_hash_1_17(const uint8_t *b)
{
    uint8_t h = 0, i;
    for (i = 1; i <= 17; ++i) h ^= b[i];
    return h;
}

int sq_timeout(uint8_t now, uint8_t since, uint8_t delay)
{
    return (uint8_t)(now - since) >= delay;
}

/* ---- alloc/free wrappers ---- */
sq_node_t* sq_alloc_node(uint16_t payload_len)
{
    if (!g_sq.plat || !g_sq.plat->malloc) return (sq_node_t*)0;
    if (payload_len > (uint16_t)(0xFFFFu - (uint16_t)sizeof(sq_node_t))) return (sq_node_t*)0;
    return (sq_node_t*)g_sq.plat->malloc((uint16_t)(sizeof(sq_node_t) + payload_len));
}

void sq_free_node(sq_node_t *n)
{
    if (n && g_sq.plat && g_sq.plat->free) g_sq.plat->free(n);
}

/* ---- channels: lookup + bitmap ---- */
sq_chan_t* sq_find_chan(uint8_t id)
{
    sq_chan_t *c = g_sq.chan_head;
    while (c) { if (c->id == id) return c; c = c->next; }
    return (sq_chan_t*)0;
}

int sq_is_used(uint8_t id) { return (g_sq.used_mask & (uint16_t)(1u << id)) != 0; }
void sq_mark_used(uint8_t id) { g_sq.used_mask = (uint16_t)(g_sq.used_mask |  (uint16_t)(1u << id)); }
void sq_mark_free(uint8_t id) { g_sq.used_mask = (uint16_t)(g_sq.used_mask & ~(uint16_t)(1u << id)); }

/* ---- TX: build + send one frame ---- */
void sq_tx_frame(uint8_t ch, uint8_t typ, uint8_t sts_bit, uint8_t seq_bit,
                 const uint8_t *payload, uint8_t len, int remember_for_resend)
{
    uint8_t b[SQ_FRAME_BYTES];
    uint8_t i;

    b[0] = SQ_STX;
    b[1] = sq_chlen_build(ch, len);
    b[2] = sq_ctrl_build(typ, sts_bit, seq_bit);

    /* copy up to 15 bytes, zero-fill remainder to keep hash window fixed */
    for (i = 0; i < len && i < SQ_PAY_MAX; ++i) b[3 + i] = payload ? payload[i] : 0;
    for (; i < SQ_PAY_MAX; ++i) b[3 + i] = 0;

    b[18] = sq_xor_hash_1_17(b);
    b[19] = SQ_ETX;

    /* write out */
    for (i = 0; i < SQ_FRAME_BYTES; ++i) (void)g_sq.plat->send_char(b[i]);

    /* remember for resend if requested */
    if (remember_for_resend) {
        for (i = 0; i < SQ_FRAME_BYTES; ++i) g_sq.last_sent[i] = b[i];
        g_sq.last_tx_tick = g_sq.plat->get_tick();
    }

    g_sq.stats.tx_frames = (uint16_t)(g_sq.stats.tx_frames + 1u);
}

/* ---- RX assemble: at most one full frame ---- */
int sq_rx_one_frame(uint8_t out[SQ_FRAME_BYTES])
{
    /* simple state machine: waiting for STX, then read 19 more bytes */
    static uint8_t buf[SQ_FRAME_BYTES];
    static uint8_t idx = 0;
    int c;

    while (1) {
        c = g_sq.plat->recv_char();
        if (c < 0) break;

        uint8_t byte = (uint8_t)c;

        if (idx == 0) {
            if (byte != SQ_STX) continue;
            buf[idx++] = byte;
            continue;
        }

        buf[idx++] = byte;

        if (idx == SQ_FRAME_BYTES) {
            idx = 0;
            /* quick checks */
            if (buf[19] != SQ_ETX) { g_sq.stats.rx_crc_err++; continue; }
            if (sq_xor_hash_1_17(buf) != buf[18]) { g_sq.stats.rx_crc_err++; continue; }

            /* valid frame */
            memcpy(out, buf, SQ_FRAME_BYTES);
            g_sq.stats.rx_frames = (uint16_t)(g_sq.stats.rx_frames + 1u);
            return 1;
        }
    }

    return 0;
}

/* ---- RX dispatcher: to be filled in burst/recv stages later ---- */
void sq_handle_valid_rx(const uint8_t *frm)
{
    (void)frm;
    /* will be implemented in recv/burst modules */
}
