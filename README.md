# squid

`squid` is a flexible serial communication protocol and hardware bridge designed to connect modern USB and wireless devices to retro 8-bit computers.

## Overview

The primary implementation of `squid` runs on a Raspberry Pi Pico, which acts as a bridge between modern peripherals and a retro computer via a simple 8-bit-friendly serial protocol. The Pico reads input from USB devices such as:

- Mice
- Keyboards
- Gamepads
- Joysticks

It can also support WiFi and virtual devices, and multiplex data from multiple sources (e.g. a PC, network, or other USB hardware) into a single stream that is encoded using the `squid` protocol. This stream is then decoded on the 8-bit system.

The protocol is symmetrical, allowing both the 8-bit machine and the Pico to send and receive data in small, fixed-length packets optimized for interrupt-driven serial communication.

## `libsquid`: Embedded Protocol Engine

The `libsquid` library is a lightweight C implementation of the core protocol logic, designed for embedding in both the Raspberry Pi Pico and on the 8-bit computer side. It handles:

- Packet parsing
- State management (HELLO handshake, ACK/NAK, connection status)
- Timeout and retry logic
- Bidirectional flow control

### Features

- Two block formats:
  - **Data blocks** (24 bytes) carry payloads with ACK/NAK and metadata
  - **Control blocks** (5 bytes) carry simple commands (e.g. PING)
- Uses small, fixed-size blocks to minimize complexity and fit within interrupt timing constraints
- Timeout and retry system based on a simple `get_tick()` function that returns a monotonically increasing counter (typically incremented at 50 Hz)
- Symmetrical sender/receiver behavior with built-in error handling
- Requires only three platform hooks:
  - `send_char(uint8_t)` – send one byte
  - `recv_char(void)` – receive one byte (returns -1 if unavailable)
  - `get_tick(void)` – return time tick counter

### Platform Integration Example

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

### Project Structure

```
include/        Public headers (squid.h)
lib/squid/      Protocol logic implementation (libsquid)
src/            Test program or integration example
```

### Status

squid is under active development. The libsquid protocol engine is complete and being integrated into the Raspberry Pi Pico firmware. Future updates will include device drivers for USB and WiFi, command multiplexing, and retro machine-specific client libraries.
