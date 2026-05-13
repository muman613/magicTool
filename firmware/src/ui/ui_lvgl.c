#include "ui_lvgl.h"

#if MAGICTOOL_ENABLE_LVGL

#include <stdbool.h>

#include "FreeRTOS.h"
#include "board_config.h"
#include "display_st7735.h"
#include "lvgl.h"
#include "pico/time.h"
#include "task.h"

#define LED_SIZE 22

static volatile uint8_t s_output_state;
static lv_obj_t *s_leds[MAGICTOOL_OUTPUT_COUNT];
static lv_obj_t *s_labels[MAGICTOOL_OUTPUT_COUNT];

#if LVGL_VERSION_MAJOR >= 9
static lv_display_t *s_display;
static uint16_t s_draw_buf_1[MAGICTOOL_ST7735_WIDTH * 16];
static uint16_t s_draw_buf_2[MAGICTOOL_ST7735_WIDTH * 16];

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    st7735_flush_rect((uint16_t)area->x1,
                      (uint16_t)area->y1,
                      (uint16_t)area->x2,
                      (uint16_t)area->y2,
                      (const uint16_t *)px_map);
    lv_display_flush_ready(display);
}
#else
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_draw_buf_1[MAGICTOOL_ST7735_WIDTH * 16];
static lv_color_t s_draw_buf_2[MAGICTOOL_ST7735_WIDTH * 16];
static lv_disp_drv_t s_disp_drv;

static void flush_cb(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *color_p)
{
    st7735_flush_rect((uint16_t)area->x1,
                      (uint16_t)area->y1,
                      (uint16_t)area->x2,
                      (uint16_t)area->y2,
                      (const uint16_t *)color_p);
    lv_disp_flush_ready(display);
}
#endif

static void set_led_style(lv_obj_t *led, bool on)
{
    lv_obj_set_style_bg_color(led, lv_color_hex(on ? 0x38e06f : 0x19222d), 0);
    lv_obj_set_style_border_color(led, lv_color_hex(on ? 0xc8ffd9 : 0x4d5a66), 0);
    lv_obj_set_style_shadow_width(led, on ? 10 : 0, 0);
    lv_obj_set_style_shadow_color(led, lv_color_hex(0x38e06f), 0);
}

static void refresh_leds(lv_timer_t *timer)
{
    (void)timer;

    const uint8_t state = s_output_state;
    for (uint8_t i = 0; i < MAGICTOOL_OUTPUT_COUNT; ++i) {
        const bool on = ((state >> i) & 0x1u) != 0;
        set_led_style(s_leds[i], on);
        lv_obj_set_style_text_color(s_labels[i], lv_color_hex(on ? 0xf4fff7 : 0x98a4ad), 0);
    }
}

static void create_ui(void)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *screen = lv_screen_active();
#else
    lv_obj_t *screen = lv_scr_act();
#endif
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x071018), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xf4f7fb), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "magicTool v0.2.0");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 120, 54);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 10);

    for (uint8_t i = 0; i < MAGICTOOL_OUTPUT_COUNT; ++i) {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, 28, 48);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_leds[i] = lv_obj_create(col);
        lv_obj_remove_style_all(s_leds[i]);
        lv_obj_set_size(s_leds[i], LED_SIZE, LED_SIZE);
        lv_obj_set_style_radius(s_leds[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_leds[i], 2, 0);

        s_labels[i] = lv_label_create(col);
        char label[] = "D0";
        label[1] = (char)('0' + i);
        lv_label_set_text(s_labels[i], label);
        lv_obj_set_style_text_font(s_labels[i], &lv_font_montserrat_12, 0);
    }

    refresh_leds(NULL);
    lv_timer_create(refresh_leds, 50, NULL);
}

static void ui_lvgl_init(void)
{
    lv_init();

    st7735_config_t config = {
        .width = MAGICTOOL_ST7735_WIDTH,
        .height = MAGICTOOL_ST7735_HEIGHT,
        .x_offset = MAGICTOOL_ST7735_X_OFFSET,
        .y_offset = MAGICTOOL_ST7735_Y_OFFSET,
        .rotation = 0,
        .rgb_order = false,
    };
    st7735_init(&config);

#if LVGL_VERSION_MAJOR >= 9
    s_display = lv_display_create(MAGICTOOL_ST7735_WIDTH, MAGICTOOL_ST7735_HEIGHT);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_display,
                           s_draw_buf_1,
                           s_draw_buf_2,
                           sizeof s_draw_buf_1,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, flush_cb);
#else
    lv_disp_draw_buf_init(&s_draw_buf,
                          s_draw_buf_1,
                          s_draw_buf_2,
                          MAGICTOOL_ST7735_WIDTH * 16);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = MAGICTOOL_ST7735_WIDTH;
    s_disp_drv.ver_res = MAGICTOOL_ST7735_HEIGHT;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);
#endif

    create_ui();
}

void ui_lvgl_task(void *params)
{
    (void)params;

    ui_lvgl_init();

    uint64_t last_tick_us = time_us_64();
    for (;;) {
        uint64_t now_us = time_us_64();
        uint32_t elapsed_ms = (uint32_t)((now_us - last_tick_us) / 1000u);
        if (elapsed_ms > 0) {
            lv_tick_inc(elapsed_ms);
            last_tick_us += (uint64_t)elapsed_ms * 1000u;
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void ui_lvgl_set_output_state(uint8_t output_state)
{
    s_output_state = output_state & 0x0fu;
}

#else

void ui_lvgl_task(void *params) { (void)params; }
void ui_lvgl_set_output_state(uint8_t output_state) { (void)output_state; }

#endif
