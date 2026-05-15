#pragma once

#include <stdint.h>

#define MAGICTOOL_OUTPUT_COUNT 4u
#define MAGICTOOL_INPUT_COUNT 2u

static const uint8_t k_magictool_output_pins[MAGICTOOL_OUTPUT_COUNT] = {
    2u,
    3u,
    4u,
    5u,
};

static const uint8_t k_magictool_input_pins[MAGICTOOL_INPUT_COUNT] = {
    6u,
    7u,
};

#define MAGICTOOL_ST7735_SPI_PORT spi0
#define MAGICTOOL_ST7735_PIN_SCK 18u
#define MAGICTOOL_ST7735_PIN_MOSI 19u
#define MAGICTOOL_ST7735_PIN_CS 17u
#define MAGICTOOL_ST7735_PIN_DC 16u
#define MAGICTOOL_ST7735_PIN_RST 20u
#define MAGICTOOL_ST7735_PIN_BL 21u
#define MAGICTOOL_ST7735_BL_ON_LEVEL 1u

#define MAGICTOOL_ST7735_WIDTH 128u
#define MAGICTOOL_ST7735_HEIGHT 128u
#define MAGICTOOL_ST7735_X_OFFSET 2u
#define MAGICTOOL_ST7735_Y_OFFSET 3u
#define MAGICTOOL_ST7735_SPI_BAUD_HZ (24u * 1000u * 1000u)
