#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SQUID_DATA_SIZE 16
#define SQUID_BLOCK_SIZE 24
#define SQUID_CTRL_SIZE 5 // Control blocks (e.g., ping)

    typedef enum
    {
        SQUID_STATE_STARTUP,
        SQUID_STATE_WAITING_ACK,
        SQUID_STATE_CONNECTED,
        SQUID_STATE_DISCONNECTED
    } squid_state_t;

// Command types for control blocks
#define SQUID_CMD_PING 0x01
    // Future: #define SQUID_CMD_PAUSE 0x02, etc.

    typedef struct
    {
        // Platform functions (must be provided by host)
        int (*send_char)(uint8_t c); // Send a byte
        int (*recv_char)(void);      // Read a byte (return -1 if no byte)
        uint8_t (*get_tick)(void);   // Time base, increases regularly (e.g., every 1/50 sec)
    } squid_platform_t;

    /**
     * Initialize the protocol engine with platform callbacks.
     */
    void squid_init(const squid_platform_t *platform);

    /**
     * Call this regularly (e.g., from your main loop or timer interrupt).
     */
    void squid_poll(void);

    /**
     * Retrieve the last valid received data block payload.
     */
    void squid_get_last_received(uint8_t *out_data);

    /**
     * Check if the link is currently in connected state.
     */
    bool squid_is_connected(void);

#ifdef __cplusplus
}
#endif
