#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform hooks (required). */
typedef struct {
    int     (*send_char)(uint8_t c); /* return 0 on success */
    int     (*recv_char)(void);      /* return next byte, or -1 if none */
    uint8_t (*get_tick)(void);       /* 8-bit tick counter (wraps) */
    void*   (*malloc)(uint16_t n);
    void    (*free)(void* p);
} squid_platform_t;

/* Timing parameters (expressed in ticks). */
typedef struct {
    uint8_t timeout_ticks;    /* resend timeout */
    uint8_t ack_delay_ticks;  /* delay before sending ack-only/empty DATA */
    uint8_t ping_ticks;       /* heartbeat period (0 = disabled) */
    uint8_t max_retries;      /* typical value: 3 */
} squid_timing_t;

/* Engine control. */
void     snet_init(const squid_platform_t *plat, const squid_timing_t *tm);
void     snet_burst(void);              /* process at most one RX and one TX */
bool     snet_link_is_up(void);

#ifdef __cplusplus
}
#endif
