#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "squid/snet.h"   /* snet_platform_t, snet_timing_t, engine APIs */

/* ---- on-wire fixed constants (private) ---- */
#define SNET_STX           ((uint8_t)0x7E)
#define SNET_ETX           ((uint8_t)0xD3)
#define SNET_FRAME_BYTES   ((uint8_t)20)
#define SNET_PAY_MAX       ((uint8_t)15)

/* CTRL (byte 2): TYP(7..5) | STS(4) | SEQ(3) | RES(2..0) */
#define SNET_CTRL_TYP_SHIFT 5u
#define SNET_CTRL_TYP_MASK  ((uint8_t)(0x07u << SNET_CTRL_TYP_SHIFT))
#define SNET_CTRL_STS_MASK  ((uint8_t)0x10u) /* 0=ACK, 1=NAK */
#define SNET_CTRL_SEQ_MASK  ((uint8_t)0x08u) /* alternating bit */
#define SNET_CTRL_RES_MASK  ((uint8_t)0x07u)

/* CHLEN (byte 1): CH(7..4) | LEN(3..0) */
#define SNET_CH_SHIFT 4u
#define SNET_CH_MASK  ((uint8_t)0xF0u)
#define SNET_LEN_MASK ((uint8_t)0x0Fu)

/* ---- engine states (private) ---- */
typedef enum {
    SNET_ENG_STARTUP = 0,
    SNET_ENG_WAITING,
    SNET_ENG_CONNECTED,
    SNET_ENG_DISCONNECTED
} snet_eng_state_t;

/* ---- queue node (malloc-backed) ---- */
typedef struct snet_node {
    struct snet_node *next;
    uint16_t len;    /* total bytes in data[] */
    uint16_t off;    /* bytes already consumed */
    uint8_t  data[]; /* flexible array (C99) */
} snet_node_t;

/* ---- dynamic channel (linked list; <=16 total) ---- */
typedef struct snet_chan {
    struct snet_chan *next;
    uint8_t  id;                    /* 0..15 (0 reserved for SYS; never handed out) */
    snet_node_t *tx_head, *tx_tail; /* app -> wire */
    snet_node_t *rx_head, *rx_tail; /* wire -> app */
    uint16_t  tx_bytes, rx_bytes;   /* queued bytes */
    uint16_t  tx_cap,  rx_cap;      /* 0 = unlimited (optional caps) */
} snet_chan_t;

/* ---- global engine context (single instance) ---- */
typedef struct {
    /* platform hooks */
    const snet_platform_t *plat;

    /* timing (ticks) */
    uint8_t timeout_ticks;
    uint8_t ack_delay_ticks;
    uint8_t ping_ticks;
    uint8_t max_retries;

    /* FSM */
    snet_eng_state_t eng;
    uint8_t seq_tx;         /* next DATA seq we will send (0/1) */
    uint8_t seq_expect;     /* seq we expect to receive next (0/1) */
    uint8_t retries;
    uint8_t last_tx_tick;
    uint8_t last_ping_tick;
    uint8_t ack_needed;     /* we owe ACK for last accepted DATA */
    uint8_t ack_wait;       /* ticks since we started owing ACK */
    uint8_t link_up;        /* set after HELLO/HELLO_ACK */

    /* last-sent frame (for resend on timeout) */
    uint8_t last_sent[SNET_FRAME_BYTES];

    /* dynamic channels + allocator */
    snet_chan_t *chan_head; /* forward list of active channels */
    uint16_t     used_mask; /* bit i set => channel i in use (1..15) */
    uint8_t      rr_last_id;/* last channel served for round-robin (0xFF = none) */
} snet_ctx_t;

/* single global instance (defined in init.c) */
extern snet_ctx_t g_snet;
