#include <stdint.h>

#include "FreeRTOS.h"
#include "bsp/board.h"
#include "board_config.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#if defined(PICO_DEBUG_TARGET_PICO2_W)
#include "pico/cyw43_arch.h"
#endif
#include "pico/stdlib.h"
#include "queue.h"
#include "task.h"
#include "tusb.h"
#if MAGICTOOL_ENABLE_DISPLAY
#include "ui_lvgl.h"
#endif

static constexpr uint OUT_PIN_COUNT = MAGICTOOL_OUTPUT_COUNT;
static constexpr uint IN_PIN_COUNT = MAGICTOOL_INPUT_COUNT;
static constexpr uint32_t PULSE_HIGH_US = 10;
static constexpr uint32_t PULSE_LOW_US = 10;
static constexpr uint32_t INPUT_POLL_US = 100;
static constexpr UBaseType_t CORE0_AFFINITY = (1u << 0u);
static constexpr UBaseType_t CORE1_AFFINITY = (1u << 1u);

#ifndef MAGICTOOL_HW_VERSION
#define MAGICTOOL_HW_VERSION 1
#endif

#ifndef MAGICTOOL_FW_VERSION_MAJOR
#define MAGICTOOL_FW_VERSION_MAJOR 0
#endif

#ifndef MAGICTOOL_FW_VERSION_MINOR
#define MAGICTOOL_FW_VERSION_MINOR 2
#endif

#ifndef MAGICTOOL_FW_VERSION_REVISION
#define MAGICTOOL_FW_VERSION_REVISION 0
#endif

static constexpr uint8_t HW_TYPE_UNKNOWN = 0x0;
static constexpr uint8_t HW_TYPE_PICO2 = 0x1;
static constexpr uint8_t HW_TYPE_PICO2_W = 0x2;

bi_decl(bi_4pins_with_names(2, "OUT0", 3, "OUT1", 4, "OUT2", 5, "OUT3"));
bi_decl(bi_2pins_with_names(6, "IN0 pulldown", 7, "IN1 pulldown"));
#if defined(PICO_DEFAULT_LED_PIN)
bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "Indicator LED"));
#endif

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

static QueueHandle_t g_cmd_queue;
static QueueHandle_t g_evt_queue;

volatile uint8_t g_output_state = 0;
volatile uint8_t g_notify_enable = 0x03;
volatile bool g_indicator_led_available = false;
volatile bool g_indicator_led_state = false;

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

static inline void publish_output_state(uint8_t state) {
    g_output_state = state & 0x0F;
#if MAGICTOOL_ENABLE_DISPLAY
    ui_lvgl_set_output_state(g_output_state);
#endif
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
        if (gpio_get(k_magictool_input_pins[i])) {
            bits |= (1u << i);
        }
    }
    return bits;
}

static void apply_output_state_bitmap(uint8_t mask) {
    mask &= 0x0F;
    for (uint8_t i = 0; i < OUT_PIN_COUNT; ++i) {
        gpio_put(k_magictool_output_pins[i], (mask >> i) & 0x1u);
    }
    publish_output_state(mask);
}

static void set_output_index(uint8_t idx, bool level) {
    const uint8_t bit = (1u << idx);
    gpio_put(k_magictool_output_pins[idx], level);

    uint8_t state = g_output_state;
    if (level) {
        state |= bit;
    } else {
        state &= ~bit;
    }
    publish_output_state(state);
}

static void toggle_output_index(uint8_t idx) {
    const uint8_t bit = (1u << idx);
    uint8_t state = g_output_state;
    const bool new_level = ((state & bit) == 0);

    gpio_put(k_magictool_output_pins[idx], new_level);

    if (new_level) {
        state |= bit;
    } else {
        state &= ~bit;
    }
    publish_output_state(state);
}

static void pulse_output_index(uint8_t idx, uint8_t count) {
    if (count == 0) {
        count = 1;
    }

    for (uint8_t i = 0; i < count; ++i) {
        gpio_put(k_magictool_output_pins[idx], 1);
        publish_output_state(g_output_state | (1u << idx));
        sleep_us(PULSE_HIGH_US);
        gpio_put(k_magictool_output_pins[idx], 0);
        publish_output_state(g_output_state & ~(1u << idx));
        sleep_us(PULSE_LOW_US);
    }
}

static void enqueue_event(uint8_t type, uint8_t info, uint8_t arg) {
    EventPacket evt{};
    evt.header = make_header(type, info);
    evt.arg = arg;
    (void)xQueueSend(g_evt_queue, &evt, 0);
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
            switch (sel) {
                case 0:
                    enqueue_ack(cmd, MAGICTOOL_FW_VERSION_MAJOR);
                    break;
                case 1:
                    enqueue_ack(cmd, MAGICTOOL_FW_VERSION_MINOR);
                    break;
                case 2:
                    enqueue_ack(cmd, MAGICTOOL_FW_VERSION_REVISION);
                    break;
                default:
                    enqueue_error(cmd, ERR_BAD_SELECTOR);
                    break;
            }
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

static void protocol_task(void *params) {
    (void)params;

    uint8_t last_input_state = read_inputs_bitmap();
    uint64_t next_poll_us = time_us_64() + INPUT_POLL_US;

    for (;;) {
        CommandPacket pkt{};
        while (xQueueReceive(g_cmd_queue, &pkt, 0) == pdPASS) {
            process_command(pkt);
        }

        const uint64_t now = time_us_64();
        if ((int64_t)(now - next_poll_us) >= 0) {
            const uint8_t current = read_inputs_bitmap();
            const uint8_t changed = (current ^ last_input_state) & g_notify_enable;

            if (changed) {
                enqueue_event(EVT_INPUT_CHANGE, changed & 0x0F, current & 0x0F);
            }

            last_input_state = current;
            next_poll_us = now + INPUT_POLL_US;
        }

        sleep_us(50);
        taskYIELD();
    }
}

static void usb_send_pending_events() {
    if (!tud_cdc_connected()) {
        return;
    }

    while (true) {
        EventPacket evt{};
        if (xQueuePeek(g_evt_queue, &evt, 0) != pdPASS) {
            break;
        }

        if (tud_cdc_write_available() < sizeof(evt)) {
            break;
        }

        tud_cdc_write(&evt, sizeof(evt));
        (void)xQueueReceive(g_evt_queue, &evt, 0);
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

            if (xQueueSend(g_cmd_queue, &pkt, 0) != pdPASS) {
                enqueue_error(get_cmd_code(pkt.header), ERR_QUEUE_FULL);
            }
        }
    }
}

static void usb_task(void *params) {
    (void)params;

    tusb_init();

    for (;;) {
        tud_task();
        usb_receive_commands();
        usb_send_pending_events();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void init_gpio() {
    for (uint i = 0; i < OUT_PIN_COUNT; ++i) {
        gpio_init(k_magictool_output_pins[i]);
        gpio_set_dir(k_magictool_output_pins[i], GPIO_OUT);
        gpio_put(k_magictool_output_pins[i], 0);
    }

    for (uint i = 0; i < IN_PIN_COUNT; ++i) {
        gpio_init(k_magictool_input_pins[i]);
        gpio_set_dir(k_magictool_input_pins[i], GPIO_IN);
        gpio_pull_down(k_magictool_input_pins[i]);
    }

    publish_output_state(0);
}

static void init_indicator_led() {
    g_indicator_led_available = board_indicator_led_init();
    if (g_indicator_led_available) {
        board_indicator_led_set(false);
        g_indicator_led_state = false;
    }
}

extern "C" void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name) {
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

int main() {
    board_init();
    init_indicator_led();
    init_gpio();

    g_cmd_queue = xQueueCreate(32, sizeof(CommandPacket));
    configASSERT(g_cmd_queue != nullptr);
    g_evt_queue = xQueueCreate(64, sizeof(EventPacket));
    configASSERT(g_evt_queue != nullptr);

    BaseType_t ok = xTaskCreateAffinitySet(usb_task,
                                           "usb",
                                           1024,
                                           nullptr,
                                           4,
                                           CORE0_AFFINITY,
                                           nullptr);
    configASSERT(ok == pdPASS);

    ok = xTaskCreateAffinitySet(protocol_task,
                                "protocol",
                                1024,
                                nullptr,
                                3,
                                CORE1_AFFINITY,
                                nullptr);
    configASSERT(ok == pdPASS);

#if MAGICTOOL_ENABLE_DISPLAY && MAGICTOOL_ENABLE_LVGL
    ok = xTaskCreateAffinitySet(ui_lvgl_task,
                                "lvgl",
                                2048,
                                nullptr,
                                2,
                                CORE0_AFFINITY,
                                nullptr);
    configASSERT(ok == pdPASS);
#endif

    vTaskStartScheduler();

    for (;;) {
    }
}
