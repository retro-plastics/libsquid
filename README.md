![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

# libsquid

`libsquid` is a lightweight, symmetric serial transport for low-power
systems and retro computers.

It gives you:
- Fixed-size 20-byte frames (easy parser on 8-bit targets)
- Reliable delivery with ACK + retransmit
- Multiplexing via 15 application channels ("sockets")
- Non-blocking cooperative API (`snet_burst()`)

## Quick Start

### 1) Build

```bash
cmake -S . -B build
cmake --build build -j4
```

### 2) Run tests

```bash
ctest --test-dir build --output-on-failure
```

### 3) Try the chat demo

In terminal 1:

```bash
mkfifo /tmp/a2b /tmp/b2a
./build/squid-chat < /tmp/b2a > /tmp/a2b
```

In terminal 2:

```bash
./build/squid-chat < /tmp/a2b > /tmp/b2a
```

Type in either terminal and press Enter to send.

## 5-Minute Integration

### Step 1: implement platform hooks

```c
#include <stdlib.h>
#include "squid/snet.h"

static int uart_send(uint8_t c) { /* send one byte */ return 0; }
static int uart_recv(void)      { /* return byte or -1 */ return -1; }
static uint8_t hw_tick(void)    { /* wraps naturally at 255 */ return 0; }

static const squid_platform_t plat = {
    .send_char = uart_send,
    .recv_char = uart_recv,
    .get_tick  = hw_tick,
    .malloc    = (void *(*)(uint16_t))malloc,
    .free      = free,
};
```

### Step 2: initialize engine timing

```c
squid_timing_t tm = {
    .timeout_ticks   = 6,
    .ack_delay_ticks = 2,
    .ping_ticks      = 0,  /* disable keepalive by default */
    .max_retries     = 3,
};

snet_init(&plat, &tm);
```

### Step 3: pump protocol in your main loop

```c
int sock = -1;

for (;;) {
    snet_burst();  /* bounded work: at most one RX + one TX frame */

    if (snet_link_is_up()) {
        if (sock < 0) {
            sock = squid_open();
            if (sock >= 0) squid_connect(sock, 1); /* channel-port 1 */
        }

        /* send */
        static const uint8_t hello[] = "Hi";
        squid_send(sock, hello, sizeof(hello) - 1);

        /* receive */
        uint8_t buf[64];
        int n = squid_recv(sock, buf, sizeof(buf));
        if (n > 0) {
            /* use buf[0..n-1] */
        }
    }

    /* your application work here */
}
```

## Architecture

`libsquid` has two layers:

```text
socket layer (socket.h): squid_open / squid_bind|squid_connect / squid_send / squid_recv / squid_close
snet layer   (snet.h):   snet_init / snet_burst / snet_link_is_up
platform hooks:          send_char / recv_char / get_tick / malloc / free
```

- `snet` handles framing, handshake, ACK/timeout logic, and keepalive.
- `socket` provides multiplexed byte streams (channels `1..15`).

## Wire Protocol

Every frame is exactly 20 bytes:

```text
+-----+-------+------+--- 15 bytes ---+-----+-----+
| STX | CHLEN | CTRL |    payload     | HSH | ETX |
+-----+-------+------+----------------+-----+-----+
  0      1       2       3 .. 17        18    19
```

- `STX = 0x7E`, `ETX = 0xD3`
- `HSH` is XOR over bytes `1..17`
- `CHLEN`: high nibble channel, low nibble payload length (`0..15`)
- `CTRL`: type, status, sequence bit

Frame types:
- `HELLO`
- `HELLO_ACK`
- `DATA`
- `ACK`
- `PING`

## State Machine

```text
STARTUP -> CONNECTED -> WAITING
   ^           |          |
   |           +----------+
   +-------- DISCONNECTED
```

- `STARTUP`: periodic `HELLO` until handshake completes.
- `CONNECTED`: normal data flow.
- `WAITING`: sent `DATA`, waiting for ACK (or timeout/resend).
- `DISCONNECTED`: retries exceeded, pause then retry startup.

## Timing Model

`get_tick()` is an 8-bit tick source. Wraparound math is intentional:

```c
uint8_t elapsed = (uint8_t)(now - since);
```

Defaults:
- `timeout_ticks = 6`
- `ack_delay_ticks = 2`
- `ping_ticks = 0` (disabled)
- `max_retries = 3`

## API Reference

From `include/squid/snet.h`:

```c
typedef struct {
    int     (*send_char)(uint8_t c);
    int     (*recv_char)(void);
    uint8_t (*get_tick)(void);
    void*   (*malloc)(uint16_t n);
    void    (*free)(void* p);
} squid_platform_t;

typedef struct {
    uint8_t timeout_ticks;
    uint8_t ack_delay_ticks;
    uint8_t ping_ticks;
    uint8_t max_retries;
} squid_timing_t;

void snet_init(const squid_platform_t *plat, const squid_timing_t *tm);
void snet_burst(void);
bool snet_link_is_up(void);
```

From `include/squid/socket.h`:

```c
int  squid_open(void);   /* returns local fd 1..15, or -1 */
int  squid_bind(int fd, uint8_t ch);    /* server-style attach */
int  squid_connect(int fd, uint8_t ch); /* client-style attach */
void squid_close(int fd);
int  squid_send(int fd, const uint8_t *data, uint16_t len);
int  squid_recv(int fd, uint8_t *buf, uint16_t max);
```

## Project Layout

```text
include/squid/     public headers
lib/squid/         protocol implementation
src/main.c         chat demo
tests/test_squid.c loopback tests
```

## Troubleshooting

- `squid-chat` error `/dev/tty: No such device or address`:
  run it from an interactive terminal session. The demo reads keyboard
  input from `/dev/tty` on purpose.
- Link never comes up:
  ensure both peers call `snet_burst()` continuously.
- Data not received:
  both sides must attach sockets to the same channel-port using
  `squid_bind(..., ch)` / `squid_connect(..., ch)`.

[language.url]:   https://en.wikipedia.org/wiki/ANSI_C
[language.badge]: https://img.shields.io/badge/language-C-blue.svg

[standard.url]:   https://en.wikipedia.org/wiki/C11_(C_standard_revision)
[standard.badge]: https://img.shields.io/badge/standard-C11-blue.svg

[license.url]:    https://github.com/tstih/libcpm3-z80/blob/main/LICENSE
[license.badge]:  https://img.shields.io/badge/license-MIT-blue.svg

[status.badge]:  https://img.shields.io/badge/status-stable-dkgreen.svg