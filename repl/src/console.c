#include <zephyr/device.h>
#include <zephyr/drivers/console/uart_console.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "mu_console.h"

#define RXBUF  4096
#define CTRL_C 0x03

BUILD_ASSERT((RXBUF & (RXBUF - 1)) == 0, "RXBUF must be a power of two");

static const struct device* const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static uint8_t rxbuf[RXBUF];
static volatile uint16_t i_get, i_put;
static volatile bool ctrl_c;
static volatile bool dropped;

static int rx_hook(uint8_t ch) {
    if (ch == CTRL_C) {
        ctrl_c = true;
        return 1;
    }

    uint16_t next = (i_put + 1) & (RXBUF - 1);

    if (next != i_get) {
        rxbuf[i_put] = ch;
        i_put = next;
    } else {
        // Ring full, byte lost, raise flag
        dropped = true;
    }

    return 1;
}

/* Hook console to callback */
int mu_console_init(void) {
    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    uart_console_in_debug_hook_install(rx_hook);
    // All nulls, only care about the callback
    uart_register_input(NULL, NULL, NULL);

    return 0;
}

int mu_getchar(void) {
    for (;;) {
        // Checked first so interrupts dont get queued behind the buffered inputs
        if (ctrl_c) {
            ctrl_c = false;
            return -1;
        }

        if (i_get != i_put) {
            break;
        }

        k_msleep(1);
    }

    int c = rxbuf[i_get];

    i_get = (i_get + 1) & (RXBUF - 1);

    return c;
}

/* Poll char out to uart */
void mu_putchar(char c) {
    uart_poll_out(uart, c);
}

/* Poll out string to uart */
void mu_write(const char* s, size_t len) {
    while (len--) {
        uart_poll_out(uart, *s++);
    }
}

/* Check if input was dropped */
bool mu_dropped(void) {
    bool was = dropped;

    dropped = false;

    return was;
}

/* Check if ctrl c */
bool mu_interrupted(void) {
    bool was = ctrl_c;

    ctrl_c = false;

    return was;
}
