#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#define USB_VID 0xCafe
#define USB_PID 0x4000
#define USB_BCD 0x0200

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

static tusb_desc_device_t const kDeviceDescriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = STRID_MANUFACTURER,
    .iProduct = STRID_PRODUCT,
    .iSerialNumber = STRID_SERIAL,
    .bNumConfigurations = 0x01
};

static uint8_t const kFsConfigurationDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

static char const *const kStringDescriptors[] = {
    (const char[]){0x09, 0x04},
    "debug_tool",
    "Pico Debug Tool",
    NULL,
    "Debug CDC",
};

static uint16_t g_string_descriptor[32 + 1];

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&kDeviceDescriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return kFsConfigurationDescriptor;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    size_t chr_count = 0;
    if (index == STRID_LANGID) {
        memcpy(&g_string_descriptor[1], kStringDescriptors[0], 2);
        chr_count = 1;
    } else if (index == STRID_SERIAL) {
        chr_count = board_usb_get_serial(g_string_descriptor + 1, 32);
    } else {
        if (index >= (sizeof(kStringDescriptors) / sizeof(kStringDescriptors[0]))) {
            return NULL;
        }

        const char *str = kStringDescriptors[index];
        if (!str) {
            return NULL;
        }

        chr_count = strlen(str);
        if (chr_count > 32) {
            chr_count = 32;
        }

        for (size_t i = 0; i < chr_count; ++i) {
            g_string_descriptor[i + 1] = (uint8_t)str[i];
        }
    }

    g_string_descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return g_string_descriptor;
}
