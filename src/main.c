/* src/main.c – two-terminal chat over libsquid.
 *
 * stdin/stdout = serial link (binary).
 * /dev/tty     = keyboard + display (via stderr).
 *
 * Usage with FIFOs:
 *   mkfifo /tmp/a2b /tmp/b2a
 *   terminal 1:  ./squid-chat < /tmp/b2a > /tmp/a2b
 *   terminal 2:  ./squid-chat < /tmp/a2b > /tmp/b2a
 *
 * Ctrl-C to quit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>

#include "squid/snet.h"
#include "squid/socket.h"

/* serial = stdin/stdout */
static int putch(uint8_t c) { return (write(1, &c, 1) == 1) ? 0 : -1; }
static int getch(void)      { uint8_t c; return (read(0, &c, 1) == 1) ? (int)c : -1; }

static uint8_t get_tick(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint8_t)((ts.tv_sec * 50 + ts.tv_nsec / 20000000) & 0xFF);
}

static const squid_platform_t plat = {
    putch, getch, get_tick, (void*(*)(uint16_t))malloc, free
};

int main(void)
{
    /* stdin non-blocking (serial link) */
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    /* open real terminal for keyboard input */
    int tty = open("/dev/tty", O_RDONLY | O_NONBLOCK);
    if (tty < 0) { perror("/dev/tty"); return 1; }

    struct termios old, raw;
    tcgetattr(tty, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(tty, TCSANOW, &raw);

    squid_timing_t tm = { 6, 2, 50, 3 };
    snet_init(&plat, &tm);

    int sock = -1;
    char line[256];
    int pos = 0;

    fprintf(stderr, "waiting for peer...\n");

    for (;;) {
        snet_burst();

        if (snet_link_is_up() && sock < 0) {
            sock = squid_open();
            fprintf(stderr, "connected!\nyou> ");
        }

        if (sock >= 0) {
            /* keyboard → send */
            char ch;
            if (read(tty, &ch, 1) == 1) {
                if (ch == '\n' || ch == '\r') {
                    if (pos > 0) {
                        squid_send(sock, (uint8_t *)line, (uint16_t)pos);
                        pos = 0;
                        fprintf(stderr, "\nyou> ");
                    }
                } else if (ch == 3) {   /* Ctrl-C */
                    break;
                } else if (pos < (int)sizeof(line) - 1) {
                    line[pos++] = ch;
                    fputc(ch, stderr);
                }
            }

            /* recv → display */
            uint8_t buf[256];
            int n = squid_recv(sock, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                fprintf(stderr, "\npeer> %s\nyou> ", (char *)buf);
                if (pos > 0) fwrite(line, 1, (size_t)pos, stderr);
            }
        }

        usleep(5000);
    }

    tcsetattr(tty, TCSANOW, &old);
    close(tty);
    if (sock >= 0) squid_close(sock);
    fprintf(stderr, "\nbye!\n");
    return 0;
}
