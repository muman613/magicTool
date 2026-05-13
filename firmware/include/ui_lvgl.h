#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_lvgl_task(void *params);
void ui_lvgl_set_output_state(uint8_t output_state);

#ifdef __cplusplus
}
#endif
