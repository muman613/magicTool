#include <stdio.h>
#include <string.h>
#include <cstdlib>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"

#define DEBUG_PIN 2
#define MAX_CMD_LEN 64

// Pulse timing (adjust as needed)
#define PULSE_HIGH_US 10
#define PULSE_LOW_US  10

// --------------------------------------------------
// GPIO helpers
// --------------------------------------------------

void pulse(int count) {
    for (int i = 0; i < count; i++) {
        gpio_put(DEBUG_PIN, 1);
        busy_wait_us_32(PULSE_HIGH_US);

        gpio_put(DEBUG_PIN, 0);
        busy_wait_us_32(PULSE_LOW_US);
    }
}

void set_pin(bool v) {
    gpio_put(DEBUG_PIN, v ? 1 : 0);
}

void toggle_pin() {
    gpio_put(DEBUG_PIN, !gpio_get(DEBUG_PIN));
}

// --------------------------------------------------
// Command parser
// --------------------------------------------------

void handle_command(char *cmd) {
    if (strncmp(cmd, "PULSE", 5) == 0) {
        int n = atoi(cmd + 6);
        if (n <= 0) n = 1;
        pulse(n);
        printf("OK PULSE %d\n", n);
    }
    else if (strncmp(cmd, "SET", 3) == 0) {
        int v = atoi(cmd + 4);
        set_pin(v);
        printf("OK SET %d\n", v);
    }
    else if (strncmp(cmd, "CLR", 3) == 0) {
        set_pin(0);
        printf("OK CLR\n");
    }
    else if (strncmp(cmd, "TOGGLE", 6) == 0) {
        toggle_pin();
        printf("OK TOGGLE\n");
    }
    else {
        printf("ERR UNKNOWN CMD: %s\n", cmd);
    }
}

// --------------------------------------------------
// Main
// --------------------------------------------------

int main() {
    stdio_init_all();

    gpio_init(DEBUG_PIN);
    gpio_set_dir(DEBUG_PIN, GPIO_OUT);
    gpio_put(DEBUG_PIN, 0);

    // Wait for USB connection (optional but helpful)
    sleep_ms(2000);

    char cmd_buffer[MAX_CMD_LEN];
    int idx = 0;

    while (true) {
        int c = getchar_timeout_us(0);

        if (c == PICO_ERROR_TIMEOUT) {
            tight_loop_contents();
            continue;
        }

        if (c == '\r' || c == '\n') {
            if (idx > 0) {
                cmd_buffer[idx] = '\0';
                handle_command(cmd_buffer);
                idx = 0;
            }
        } else {
            if (idx < MAX_CMD_LEN - 1) {
                cmd_buffer[idx++] = (char)c;
            }
        }
    }
}
