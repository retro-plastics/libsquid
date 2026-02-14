#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Multiplexed socket API.
 *
 * squid_open() returns a local handle (fd-like). You then bind/connect
 * that socket to a wire channel (1..15), which acts like a "port".
 *
 * Received blocks are queued per bound socket and copied out on recv.
 */
int      squid_open(void);              /* returns fd 1..15, or -1 */
int      squid_bind(int fd, uint8_t ch);
int      squid_connect(int fd, uint8_t ch);
void     squid_close(int fd);
int      squid_send(int fd, const uint8_t *data, uint16_t len);
int      squid_recv(int fd, uint8_t *buf, uint16_t max);

#ifdef __cplusplus
}
#endif
