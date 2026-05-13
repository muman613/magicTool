#include "display_st7735.h"

#include <string.h>

#include "board_config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define ST7735_SWRESET 0x01u
#define ST7735_SLPOUT 0x11u
#define ST7735_COLMOD 0x3Au
#define ST7735_MADCTL 0x36u
#define ST7735_CASET 0x2Au
#define ST7735_RASET 0x2Bu
#define ST7735_RAMWR 0x2Cu
#define ST7735_DISPON 0x29u
#define ST7735_INVOFF 0x20u

#define MADCTL_MY 0x80u
#define MADCTL_MX 0x40u
#define MADCTL_MV 0x20u
#define MADCTL_RGB 0x00u
#define MADCTL_BGR 0x08u

static st7735_config_t s_config = {
    .width = MAGICTOOL_ST7735_WIDTH,
    .height = MAGICTOOL_ST7735_HEIGHT,
    .x_offset = MAGICTOOL_ST7735_X_OFFSET,
    .y_offset = MAGICTOOL_ST7735_Y_OFFSET,
    .rotation = 0,
    .rgb_order = false,
};

static void st7735_select(void)
{
    gpio_put(MAGICTOOL_ST7735_PIN_CS, 0);
}

static void st7735_deselect(void)
{
    gpio_put(MAGICTOOL_ST7735_PIN_CS, 1);
}

static void st7735_cmd(uint8_t command)
{
    gpio_put(MAGICTOOL_ST7735_PIN_DC, 0);
    st7735_select();
    spi_write_blocking(MAGICTOOL_ST7735_SPI_PORT, &command, 1);
    st7735_deselect();
}

static void st7735_data(const uint8_t *data, size_t len)
{
    gpio_put(MAGICTOOL_ST7735_PIN_DC, 1);
    st7735_select();
    spi_write_blocking(MAGICTOOL_ST7735_SPI_PORT, data, len);
    st7735_deselect();
}

static void st7735_write_u16(uint16_t value)
{
    uint8_t data[] = {
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xffu),
    };
    st7735_data(data, sizeof data);
}

static void st7735_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    x0 += s_config.x_offset;
    x1 += s_config.x_offset;
    y0 += s_config.y_offset;
    y1 += s_config.y_offset;

    st7735_cmd(ST7735_CASET);
    st7735_write_u16(x0);
    st7735_write_u16(x1);

    st7735_cmd(ST7735_RASET);
    st7735_write_u16(y0);
    st7735_write_u16(y1);

    st7735_cmd(ST7735_RAMWR);
}

static uint8_t st7735_madctl(void)
{
    uint8_t value = s_config.rgb_order ? MADCTL_RGB : MADCTL_BGR;

    switch (s_config.rotation & 0x03u) {
    case 0:
        value |= MADCTL_MX | MADCTL_MY;
        break;
    case 1:
        value |= MADCTL_MY | MADCTL_MV;
        break;
    case 2:
        break;
    case 3:
        value |= MADCTL_MX | MADCTL_MV;
        break;
    }

    return value;
}

void st7735_init(const st7735_config_t *config)
{
    if (config) {
        s_config = *config;
    }

    spi_init(MAGICTOOL_ST7735_SPI_PORT, MAGICTOOL_ST7735_SPI_BAUD_HZ);
    gpio_set_function(MAGICTOOL_ST7735_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(MAGICTOOL_ST7735_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(MAGICTOOL_ST7735_PIN_CS);
    gpio_init(MAGICTOOL_ST7735_PIN_DC);
    gpio_init(MAGICTOOL_ST7735_PIN_RST);
    gpio_init(MAGICTOOL_ST7735_PIN_BL);
    gpio_set_dir(MAGICTOOL_ST7735_PIN_CS, GPIO_OUT);
    gpio_set_dir(MAGICTOOL_ST7735_PIN_DC, GPIO_OUT);
    gpio_set_dir(MAGICTOOL_ST7735_PIN_RST, GPIO_OUT);
    gpio_set_dir(MAGICTOOL_ST7735_PIN_BL, GPIO_OUT);
    st7735_deselect();
    gpio_put(MAGICTOOL_ST7735_PIN_BL, 1);

    gpio_put(MAGICTOOL_ST7735_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(MAGICTOOL_ST7735_PIN_RST, 1);
    sleep_ms(120);

    st7735_cmd(ST7735_SWRESET);
    sleep_ms(150);
    st7735_cmd(ST7735_SLPOUT);
    sleep_ms(150);

    uint8_t colmod = 0x05u;
    st7735_cmd(ST7735_COLMOD);
    st7735_data(&colmod, 1);

    uint8_t madctl = st7735_madctl();
    st7735_cmd(ST7735_MADCTL);
    st7735_data(&madctl, 1);

    st7735_cmd(ST7735_INVOFF);
    st7735_cmd(ST7735_DISPON);
    sleep_ms(120);

    st7735_fill(0x0000u);
}

void st7735_fill(uint16_t color565)
{
    st7735_set_addr_window(0, 0, s_config.width - 1u, s_config.height - 1u);

    uint8_t chunk[64];
    for (size_t i = 0; i < sizeof chunk; i += 2) {
        chunk[i] = (uint8_t)(color565 >> 8);
        chunk[i + 1u] = (uint8_t)(color565 & 0xffu);
    }

    gpio_put(MAGICTOOL_ST7735_PIN_DC, 1);
    st7735_select();
    uint32_t pixels = (uint32_t)s_config.width * (uint32_t)s_config.height;
    while (pixels) {
        uint32_t batch = pixels > 32u ? 32u : pixels;
        spi_write_blocking(MAGICTOOL_ST7735_SPI_PORT, chunk, batch * 2u);
        pixels -= batch;
    }
    st7735_deselect();
}

void st7735_flush_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t *pixels)
{
    if (!pixels || x1 < x0 || y1 < y0) {
        return;
    }

    st7735_set_addr_window(x0, y0, x1, y1);

    gpio_put(MAGICTOOL_ST7735_PIN_DC, 1);
    st7735_select();
    uint32_t count = ((uint32_t)x1 - x0 + 1u) * ((uint32_t)y1 - y0 + 1u);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t data[] = {
            (uint8_t)(pixels[i] >> 8),
            (uint8_t)(pixels[i] & 0xffu),
        };
        spi_write_blocking(MAGICTOOL_ST7735_SPI_PORT, data, sizeof data);
    }
    st7735_deselect();
}
