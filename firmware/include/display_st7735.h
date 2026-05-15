#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t rotation;
    bool rgb_order;
} st7735_config_t;

#ifdef __cplusplus
extern "C" {
#endif

void st7735_init(const st7735_config_t *config);
void st7735_set_backlight(bool on);
void st7735_set_display_on(bool on);
void st7735_fill(uint16_t color565);
void st7735_flush_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t *pixels);

#ifdef __cplusplus
}
#endif
