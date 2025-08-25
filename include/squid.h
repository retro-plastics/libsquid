#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque socket handle (channel id). 0xFF = invalid.
   NOTE: Socket 0 is RESERVED for SYS control messages. */
typedef uint8_t squid_sock_t;
#define SQUID_SOCK_INVALID ((squid_sock_t)0xFF)

/* --- platform hooks (must be provided) --- */
typedef struct {
    int     (*send_char)(uint8_t c); /* return 0 on success */
    int     (*recv_char)(void);      /* return next byte, or -1 if none */
    uint8_t (*get_tick)(void);       /* 8-bit tick counter (wraps) */
    void*   (*malloc)(uint16_t n);
    void    (*free)(void* p);
} squid_platform_t;

/* --- timing parameters (ticks, user-chosen) --- */
typedef struct {
    uint8_t timeout_ticks;    /* resend timeout */
    uint8_t ack_delay_ticks;  /* delay before sending ack-only/empty DATA */
    uint8_t ping_ticks;       /* heartbeat period (0 = disabled) */
    uint8_t max_retries;      /* typically 3 */
} squid_timing_t;

/* --- stats --- */
typedef struct {
    uint16_t rx_frames;
    uint16_t tx_frames;
    uint16_t rx_crc_err;
    uint16_t rx_dup;
    uint16_t rx_dropped;
    uint16_t timeouts;
    uint16_t resends;
} squid_stats_t;

/* --- socket-like API (BSD-ish) -------------------------------------- */
squid_sock_t squid_socket(void);  /* allocate lowest free channel in 1..15 */
void         squid_close(squid_sock_t s);
uint16_t     squid_send(squid_sock_t s, const void *buf, uint16_t len);
uint16_t     squid_recv(squid_sock_t s, void *buf, uint16_t max);
uint16_t     squid_recv_avail(squid_sock_t s);
uint16_t     squid_send_queued(squid_sock_t s);
void         squid_select(uint16_t want_read_mask,
                          uint16_t want_write_mask,
                          uint16_t *out_read_mask,
                          uint16_t *out_write_mask);

/* === SQUID EXTENSIONS (non-BSD) ===================================== */

/* Engine control (no BSD equivalent) */
void         squid_init(const squid_platform_t *plat, const squid_timing_t *tm);
void         squid_burst(void);
bool         squid_link_is_up(void);
void         squid_stats_get(squid_stats_t *out);

#ifdef __cplusplus
}
#endif
