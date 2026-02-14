#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Multiplexed socket API.
 *
 * Each socket maps to a channel (1..15) on the squid wire protocol.
 * Both sides must open the same channel id to exchange data.
 * Received blocks are queued per-channel and copied out on recv.
 */
int      squid_open(void);              /* returns channel id 1..15, or -1 */
void     squid_close(int ch);
int      squid_send(int ch, const uint8_t *data, uint16_t len);
int      squid_recv(int ch, uint8_t *buf, uint16_t max);

#ifdef __cplusplus
}
#endif
