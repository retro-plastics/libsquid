#pragma once
#include "squid.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---- on-wire fixed constants ---- */
#define SQ_STX           ((uint8_t)0x7E)
#define SQ_ETX           ((uint8_t)0xD3)
#define SQ_FRAME_BYTES   ((uint8_t)20)
#define SQ_PAY_MAX       ((uint8_t)15)

/* CTRL (byte 2): TYP(7..5) | STS(4) | SEQ(3) | RES(2..0) */
#define SQ_CTRL_TYP_SHIFT 5u
#define SQ_CTRL_TYP_MASK  ((uint8_t)(0x07u << SQ_CTRL_TYP_SHIFT))
#define SQ_CTRL_STS_MASK  ((uint8_t)0x10u) /* 0=ACK, 1=NAK */
#define SQ_CTRL_SEQ_MASK  ((uint8_t)0x08u) /* alternating bit */
#define SQ_CTRL_RES_MASK  ((uint8_t)0x07u)

/* CHLEN (byte 1): CH(7..4) | LEN(3..0) */
#define SQ_CH_SHIFT 4u
#define SQ_CH_MASK  ((uint8_t)0xF0u)
#define SQ_LEN_MASK ((uint8_t)0x0Fu)

/* ---- engine states ---- */
typedef enum {
    SQ_ENG_STARTUP = 0,
    SQ_ENG_WAITING,
    SQ_ENG_CONNECTED,
    SQ_ENG_DISCONNECTED
} sq_eng_state_t;

/* ---- queue node (malloc-backed) ---- */
typedef struct sq_node {
    struct sq_node *next;
    uint16_t len;   /* total bytes in data[] */
    uint16_t off;   /* bytes already consumed */
    uint8_t  data[];/* flexible array (C99) */
} sq_node_t;

/* ---- dynamic channel (linked list; <=16 total) ---- */
typedef struct sq_chan {
    struct sq_chan *next;
    uint8_t  id;                   /* 0..15 */
    sq_node_t *tx_head, *tx_tail;  /* app->wire */
    sq_node_t *rx_head, *rx_tail;  /* wire->app */
    uint16_t  tx_bytes, rx_bytes;  /* queued bytes */
    uint16_t  tx_cap,  rx_cap;     /* 0 = unlimited */
} sq_chan_t;

/* ---- stats (internal == public shape) ---- */
typedef struct {
    uint16_t rx_frames;
    uint16_t tx_frames;
    uint16_t rx_crc_err;
    uint16_t rx_dup;
    uint16_t rx_dropped;
    uint16_t timeouts;
    uint16_t resends;
} sq_stats_t;

/* ---- global context ---- */
typedef struct {
    /* platform */
    const squid_platform_t *plat;

    /* timing (ticks) */
    uint8_t timeout_ticks;
    uint8_t ack_delay_ticks;
    uint8_t ping_ticks;
    uint8_t max_retries;

    /* FSM */
    sq_eng_state_t eng;
    uint8_t seq_tx;
    uint8_t seq_expect;
    uint8_t retries;
    uint8_t last_tx_tick;
    uint8_t last_ping_tick;
    uint8_t ack_needed;
    uint8_t ack_wait;
    uint8_t link_up;

    /* resend buffer */
    uint8_t last_sent[SQ_FRAME_BYTES];

    /* channels */
    sq_chan_t *chan_head;
    uint16_t   used_mask;    /* bit i set => channel i in use */
    uint8_t    rr_last_id;   /* last channel served (for RR) */

    /* stats */
    sq_stats_t stats;
} sq_ctx_t;

/* single global instance */
extern sq_ctx_t g_sq;

/* ---- helper function declarations (implemented in util.c) ---- */
uint8_t  sq_ctrl_build(uint8_t typ, uint8_t sts_bit, uint8_t seq_bit);
uint8_t  sq_chlen_build(uint8_t ch, uint8_t len);
void     sq_chlen_parse(uint8_t chlen, uint8_t *ch, uint8_t *len);
uint8_t  sq_xor_hash_1_17(const uint8_t *b);
int      sq_timeout(uint8_t now, uint8_t since, uint8_t delay);

sq_node_t* sq_alloc_node(uint16_t payload_len);
void       sq_free_node(sq_node_t *n);

sq_chan_t* sq_find_chan(uint8_t id);
int       sq_is_used(uint8_t id);
void      sq_mark_used(uint8_t id);
void      sq_mark_free(uint8_t id);

/* round-robin chooser: returns channel id 0..15 with pending TX, or 0xFF if none */
uint8_t   sq_rr_pick_next_tx(void);

/* low-level TX: build+send one frame; remember if nonzero */
void      sq_tx_frame(uint8_t ch, uint8_t typ, uint8_t sts_bit, uint8_t seq_bit,
                      const uint8_t *payload, uint8_t len, int remember_for_resend);

/* RX: assemble one full, valid frame into out[20]; return 1 if ok, 0 otherwise */
int       sq_rx_one_frame(uint8_t out[SQ_FRAME_BYTES]);

/* RX dispatcher: consume a validated frame buffer */
void      sq_handle_valid_rx(const uint8_t *frm);