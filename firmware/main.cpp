#include <stdint.h>

#include "bsp/board.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#if defined(PICO_DEBUG_TARGET_PICO2_W)
#include "pico/cyw43_arch.h"
#endif
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "tusb.h"

// ------------------------------------------------------------
// Configuration
// ------------------------------------------------------------

static constexpr uint OUT_PIN_COUNT = 4;
static constexpr uint IN_PIN_COUNT = 2;

// Final agreed mapping for Pico 2 W
static constexpr uint OUTPUT_PINS[OUT_PIN_COUNT] = {2, 3, 4, 5};
static constexpr uint INPUT_PINS[IN_PIN_COUNT] = {6, 7};

// Pulse timing for synchronous PULSE command
static constexpr uint32_t PULSE_HIGH_US = 10;
static constexpr uint32_t PULSE_LOW_US = 10;

// Queue sizes
static constexpr uint CMD_QUEUE_LEN = 32;
static constexpr uint EVT_QUEUE_LEN = 64;

// Input polling interval on worker core
static constexpr uint32_t INPUT_POLL_US = 100;

#ifndef MAGICTOOL_HW_VERSION
#define MAGICTOOL_HW_VERSION 1
#endif

static constexpr uint8_t HW_TYPE_UNKNOWN = 0x0;
static constexpr uint8_t HW_TYPE_PICO2 = 0x1;
static constexpr uint8_t HW_TYPE_PICO2_W = 0x2;

bi_decl(bi_4pins_with_names(2, "OUT0", 3, "OUT1", 4, "OUT2", 5, "OUT3"));
bi_decl(bi_2pins_with_names(6, "IN0 pulldown", 7, "IN1 pulldown"));
#if defined(PICO_DEFAULT_LED_PIN)
bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "Indicator LED"));
#endif

// ------------------------------------------------------------
// Protocol
// ------------------------------------------------------------

// Host -> Pico, 2 bytes:
//   byte0: upper nibble = command, lower nibble = selector
//   byte1: argument
//   OPEN  = 0xC0 0x00, turns the onboard indicator LED on
//   CLOSE = 0xD0 0x00, turns the onboard indicator LED off
//
// Pico -> Host, 2 bytes:
//   byte0: upper nibble = event type, lower nibble = info
//   byte1: argument/payload

enum Command : uint8_t {
    CMD_NOP = 0x0,
    CMD_SET = 0x1,
    CMD_CLEAR = 0x2,
    CMD_TOGGLE = 0x3,
    CMD_PULSE = 0x4,
    CMD_WRITE_MASK = 0x5,
    CMD_READ_INPUTS = 0x6,
    CMD_READ_OUTPUTS = 0x7,
    CMD_ENABLE_NOTIFY = 0x8,
    CMD_DISABLE_NOTIFY = 0x9,
    CMD_GET_VERSION = 0xA,
    CMD_PING = 0xB,
    CMD_OPEN = 0xC,
    CMD_CLOSE = 0xD,
    CMD_GET_HARDWARE_VERSION = 0xE,
};

enum EventType : uint8_t {
    EVT_INPUT_CHANGE = 0x1,
    EVT_INPUTS = 0x2,
    EVT_OUTPUTS = 0x3,
    EVT_ACK = 0xE,
    EVT_ERROR = 0xF,
};

enum ErrorCode : uint8_t {
    ERR_BAD_PIN = 1,
    ERR_BAD_SELECTOR = 2,
    ERR_BAD_ARGUMENT = 3,
    ERR_QUEUE_FULL = 4,
    ERR_UNKNOWN_CMD = 5,
    ERR_LED_UNAVAILABLE = 6,
};

struct CommandPacket {
    uint8_t header;
    uint8_t arg;
};

struct EventPacket {
    uint8_t header;
    uint8_t arg;
};

// ------------------------------------------------------------
// Shared state between cores
// ------------------------------------------------------------

static queue_t g_cmd_queue;
static queue_t g_evt_queue;

// Bit0..bit3 correspond to outputs 0..3
volatile uint8_t g_output_state = 0;

// Bit0..bit1 correspond to inputs 0..1
volatile uint8_t g_notify_enable = 0x03;  // both enabled by default

volatile bool g_indicator_led_available = false;
volatile bool g_indicator_led_state = false;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

static inline uint8_t get_cmd_code(uint8_t header) {
    return (header >> 4) & 0x0F;
}

static inline uint8_t get_selector(uint8_t header) {
    return header & 0x0F;
}

static inline uint8_t make_header(uint8_t hi, uint8_t lo) {
    return static_cast<uint8_t>(((hi & 0x0F) << 4) | (lo & 0x0F));
}

static inline bool valid_output_index(uint8_t idx) {
    return idx < OUT_PIN_COUNT;
}

static inline bool valid_input_index(uint8_t idx) {
    return idx < IN_PIN_COUNT;
}

static inline uint8_t hardware_version_byte() {
#if defined(PICO_DEBUG_TARGET_PICO2_W)
    constexpr uint8_t hw_type = HW_TYPE_PICO2_W;
#elif defined(PICO_DEBUG_TARGET_PICO2)
    constexpr uint8_t hw_type = HW_TYPE_PICO2;
#else
    constexpr uint8_t hw_type = HW_TYPE_UNKNOWN;
#endif
    return make_header(hw_type, MAGICTOOL_HW_VERSION);
}

static uint8_t read_inputs_bitmap() {
    uint8_t bits = 0;
    for (uint8_t i = 0; i < IN_PIN_COUNT; ++i) {
        if (gpio_get(INPUT_PINS[i])) {
            bits |= (1u << i);
        }
    }
    return bits;
}

static void apply_output_state_bitmap(uint8_t mask) {
    mask &= 0x0F;
    for (uint8_t i = 0; i < OUT_PIN_COUNT; ++i) {
        const bool level = (mask >> i) & 0x1;
        gpio_put(OUTPUT_PINS[i], level);
    }
    g_output_state = mask;
}

static void set_output_index(uint8_t idx, bool level) {
    const uint8_t bit = (1u << idx);
    gpio_put(OUTPUT_PINS[idx], level);

    uint8_t state = g_output_state;
    if (level) {
        state |= bit;
    } else {
        state &= ~bit;
    }
    g_output_state = state;
}

static void toggle_output_index(uint8_t idx) {
    const uint8_t bit = (1u << idx);
    uint8_t state = g_output_state;
    const bool new_level = ((state & bit) == 0);

    gpio_put(OUTPUT_PINS[idx], new_level);

    if (new_level) {
        state |= bit;
    } else {
        state &= ~bit;
    }
    g_output_state = state;
}

static void pulse_output_index(uint8_t idx, uint8_t count) {
    if (count == 0) {
        count = 1;
    }

    for (uint8_t i = 0; i < count; ++i) {
        gpio_put(OUTPUT_PINS[idx], 1);
        sleep_us(PULSE_HIGH_US);
        gpio_put(OUTPUT_PINS[idx], 0);
        sleep_us(PULSE_LOW_US);
    }

    // Pulse command leaves pin low
    uint8_t state = g_output_state;
    state &= ~(1u << idx);
    g_output_state = state;
}

static void enqueue_event(uint8_t type, uint8_t info, uint8_t arg) {
    EventPacket evt{};
    evt.header = make_header(type, info);
    evt.arg = arg;
    (void)queue_try_add(&g_evt_queue, &evt);
}

static void enqueue_ack(uint8_t cmd, uint8_t arg = 0) {
    enqueue_event(EVT_ACK, cmd, arg);
}

static void enqueue_error(uint8_t cmd, uint8_t err) {
    enqueue_event(EVT_ERROR, cmd, err);
}

static bool board_indicator_led_init() {
#if defined(PICO_DEBUG_TARGET_PICO2_W)
    return cyw43_arch_init() == 0;
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return true;
#else
    return false;
#endif
}

static void board_indicator_led_set(bool led_on) {
#if defined(PICO_DEBUG_TARGET_PICO2_W)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, PICO_DEFAULT_LED_PIN_INVERTED ? !led_on : led_on);
#else
    (void)led_on;
#endif
}

static void set_indicator_led(uint8_t cmd, bool led_on) {
    if (!g_indicator_led_available) {
        enqueue_error(cmd, ERR_LED_UNAVAILABLE);
        return;
    }

    board_indicator_led_set(led_on);
    g_indicator_led_state = led_on;
    enqueue_ack(cmd, g_indicator_led_state ? 1 : 0);
}

// ------------------------------------------------------------
// Worker core (core 1)
// - executes output commands synchronously
// - polls inputs and emits async change notifications
// ------------------------------------------------------------

static void process_command(const CommandPacket &pkt) {
    const uint8_t cmd = get_cmd_code(pkt.header);
    const uint8_t sel = get_selector(pkt.header);

    switch (cmd) {
        case CMD_NOP:
            enqueue_ack(cmd, 0);
            break;

        case CMD_SET:
            if (!valid_output_index(sel)) {
                enqueue_error(cmd, ERR_BAD_PIN);
                break;
            }
            set_output_index(sel, true);
            enqueue_ack(cmd, g_output_state);
            break;

        case CMD_CLEAR:
            if (!valid_output_index(sel)) {
                enqueue_error(cmd, ERR_BAD_PIN);
                break;
            }
            set_output_index(sel, false);
            enqueue_ack(cmd, g_output_state);
            break;

        case CMD_TOGGLE:
            if (!valid_output_index(sel)) {
                enqueue_error(cmd, ERR_BAD_PIN);
                break;
            }
            toggle_output_index(sel);
            enqueue_ack(cmd, g_output_state);
            break;

        case CMD_PULSE:
            if (!valid_output_index(sel)) {
                enqueue_error(cmd, ERR_BAD_PIN);
                break;
            }
            pulse_output_index(sel, pkt.arg);
            enqueue_ack(cmd, g_output_state);
            break;

        case CMD_WRITE_MASK:
            apply_output_state_bitmap(pkt.arg);
            enqueue_ack(cmd, g_output_state);
            break;

        case CMD_READ_INPUTS:
            enqueue_event(EVT_INPUTS, 0, read_inputs_bitmap());
            break;

        case CMD_READ_OUTPUTS:
            enqueue_event(EVT_OUTPUTS, 0, g_output_state & 0x0F);
            break;

        case CMD_ENABLE_NOTIFY:
            if (sel == 0x0F) {
                g_notify_enable = (1u << IN_PIN_COUNT) - 1u;
                enqueue_ack(cmd, g_notify_enable);
            } else if (valid_input_index(sel)) {
                g_notify_enable |= (1u << sel);
                enqueue_ack(cmd, g_notify_enable);
            } else {
                enqueue_error(cmd, ERR_BAD_SELECTOR);
            }
            break;

        case CMD_DISABLE_NOTIFY:
            if (sel == 0x0F) {
                g_notify_enable = 0;
                enqueue_ack(cmd, g_notify_enable);
            } else if (valid_input_index(sel)) {
                g_notify_enable &= ~(1u << sel);
                enqueue_ack(cmd, g_notify_enable);
            } else {
                enqueue_error(cmd, ERR_BAD_SELECTOR);
            }
            break;

        case CMD_GET_VERSION:
            enqueue_ack(cmd, 1);
            break;

        case CMD_PING:
            enqueue_ack(cmd, pkt.arg);
            break;

        case CMD_OPEN:
            set_indicator_led(cmd, true);
            break;

        case CMD_CLOSE:
            set_indicator_led(cmd, false);
            break;

        case CMD_GET_HARDWARE_VERSION:
            enqueue_ack(cmd, hardware_version_byte());
            break;

        default:
            enqueue_error(cmd, ERR_UNKNOWN_CMD);
            break;
    }
}

void core1_entry() {
    uint8_t last_input_state = read_inputs_bitmap();
    absolute_time_t next_poll = make_timeout_time_us(INPUT_POLL_US);

    while (true) {
        CommandPacket pkt{};

        if (queue_try_remove(&g_cmd_queue, &pkt)) {
            process_command(pkt);
        }

        if (absolute_time_diff_us(get_absolute_time(), next_poll) <= 0) {
            const uint8_t current = read_inputs_bitmap();
            const uint8_t changed = (current ^ last_input_state) & g_notify_enable;

            if (changed) {
                enqueue_event(EVT_INPUT_CHANGE, changed & 0x0F, current & 0x0F);
            }

            last_input_state = current;
            next_poll = make_timeout_time_us(INPUT_POLL_US);
        }

        tight_loop_contents();
    }
}

// ------------------------------------------------------------
// USB side (core 0)
// ------------------------------------------------------------

static void usb_send_pending_events() {
    if (!tud_cdc_connected()) {
        return;
    }

    while (true) {
        EventPacket evt{};
        if (!queue_try_peek(&g_evt_queue, &evt)) {
            break;
        }

        if (tud_cdc_write_available() < 2) {
            break;
        }

        tud_cdc_write(&evt, sizeof(evt));
        queue_try_remove(&g_evt_queue, &evt);
    }

    tud_cdc_write_flush();
}

static void usb_receive_commands() {
    static bool have_header = false;
    static uint8_t rx_header = 0;

    while (tud_cdc_available()) {
        uint8_t byte = 0;
        if (tud_cdc_read(&byte, 1) != 1) {
            break;
        }

        if (!have_header) {
            rx_header = byte;
            have_header = true;
        } else {
            CommandPacket pkt{};
            pkt.header = rx_header;
            pkt.arg = byte;
            have_header = false;

            if (!queue_try_add(&g_cmd_queue, &pkt)) {
                const uint8_t cmd = get_cmd_code(pkt.header);
                enqueue_error(cmd, ERR_QUEUE_FULL);
            }
        }
    }
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------

static void init_gpio() {
    for (uint i = 0; i < OUT_PIN_COUNT; ++i) {
        gpio_init(OUTPUT_PINS[i]);
        gpio_set_dir(OUTPUT_PINS[i], GPIO_OUT);
        gpio_put(OUTPUT_PINS[i], 0);
    }

    for (uint i = 0; i < IN_PIN_COUNT; ++i) {
        gpio_init(INPUT_PINS[i]);
        gpio_set_dir(INPUT_PINS[i], GPIO_IN);
        gpio_pull_down(INPUT_PINS[i]);
    }

    g_output_state = 0;
}

static void init_indicator_led() {
    g_indicator_led_available = board_indicator_led_init();
    if (g_indicator_led_available) {
        board_indicator_led_set(false);
        g_indicator_led_state = false;
    }
}

int main() {
    board_init();
    init_indicator_led();
    init_gpio();

    queue_init(&g_cmd_queue, sizeof(CommandPacket), CMD_QUEUE_LEN);
    queue_init(&g_evt_queue, sizeof(EventPacket), EVT_QUEUE_LEN);

    multicore_launch_core1(core1_entry);

    tusb_init();

    while (true) {
        tud_task();
        usb_receive_commands();
        usb_send_pending_events();
        tight_loop_contents();
    }

    return 0;
}
