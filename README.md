# squid

`squid` is a lightweight, symmetrical serial communication protocol and hardware bridge designed to connect modern USB and wireless devices to retro 8-bit computers.

## overview

The primary implementation of `squid` runs on a Raspberry Pi Pico, which acts as a bridge between modern peripherals and a retro computer. The Pico reads input from USB devices such as:

- mice  
- keyboards  
- gamepads  
- joysticks

It can also support Wi-Fi and virtual devices. Multiple input sources (e.g., USB, PC host, network) can be multiplexed into a single stream encoded with the `squid` protocol. This stream is decoded on the 8-bit machine.

`squid` is designed for **deterministic, interrupt-driven communication**: every interrupt tick can process exactly one block. This makes it practical for 8-bit systems where timing is tight and code must remain simple.

## why squid?

Existing protocols like SLIP, XMODEM, or TCP/IP are either too large or too irregular for 8-bit ISR handling. `squid` solves this by:

- using **short, fixed-size blocks** (always 24 or 5 bytes)  
- avoiding variable-length parsing  
- providing a **symmetrical design** (both sides use the same simple state machine)  
- embedding error detection (hash, ACK/NAK, retry)

This keeps implementations tiny and predictable.

## block formats

### data block (24 bytes)

Carries application data and includes sequencing and ACK information.

```
+-----+-----+-----+-----+-----+-----+----------------+-----+-----+
| STX | SEQ | ACK | STS | TYP | FLG | 16-byte data  | HSH | ETX |
+-----+-----+-----+-----+-----+-----+----------------+-----+-----+
 0     1     2     3     4     5     6 .. 21         22    23
```

- **STX** = 0x7E  
- **SEQ** = sequence number of this block  
- **ACK** = last sequence received from peer  
- **STS** = status (OK or NAK)  
- **TYP** = block type (HELLO, HELLO_ACK, DATA, etc.)  
- **FLG** = reserved for future use  
- **data** = 16-byte payload  
- **HSH** = XOR of bytes 1..21  
- **ETX** = 0xD3

### control block (5 bytes)

Carries simple commands such as PING.

```
+----------+-----+-----+-----+----------+
| CTRL_STX | SEQ | ACK | CMD | CTRL_ETX |
+----------+-----+-----+-----+----------+
 0          1     2     3     4
```

- **CTRL_STX** = 0xE0  
- **CMD** = command (e.g., PING = 0x01)  
- **CTRL_ETX** = 0xCF

## state machine

Each side runs the same logic:

- **STARTUP** → send `HELLO` until a `HELLO_ACK` is received  
- **CONNECTED** → normal data exchange, ACK/NAK handled  
- **WAITING_ACK** → last block retransmitted until ACK is received or timeout occurs  
- **DISCONNECTED** → retries exceeded; fall back to STARTUP after a pause

## timing model

- `get_tick()` must return an **8-bit counter** that increments regularly (e.g., 50 Hz).  
- timeout logic uses 8-bit wraparound arithmetic (`(uint8_t)(now - since)`), so wrap is safe.  
- this allows a simple 8-bit ISR to keep the link alive without large counters.

## libsquid: embedded protocol engine

The `libsquid` library is a small C implementation of the protocol, designed for both the Pico and the 8-bit computer. It handles:

- parsing and framing of blocks  
- connection state transitions  
- timeout and retry logic  
- ACK/NAK management  
- ping/keepalive

### platform hooks

Only three functions are needed:

```c
int send_char(uint8_t c);   // send one byte
int recv_char(void);        // return -1 if none
uint8_t get_tick(void);     // 8-bit tick counter
```

### example integration

```c
squid_platform_t plat = {
    .send_char = my_uart_send,
    .recv_char = my_uart_recv,
    .get_tick  = my_tick_counter
};

squid_init(&plat);

while (true) {
    squid_poll();
    if (squid_is_connected()) {
        uint8_t data[16];
        squid_get_last_received(data);
        // process data
    }
}
```

## project structure

```
include/        Public headers (squid.h)
lib/squid/      Protocol logic (libsquid)
src/            Test program / integration example
```

## status

The core `libsquid` engine is functional and tested in loopback simulation. Integration with Raspberry Pi Pico firmware is in progress, and work on retro machine clients will follow. Upcoming features include:

- USB and Wi-Fi device drivers on the Pico  
- command multiplexing  
- retro-specific client libraries
