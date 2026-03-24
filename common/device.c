#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "device.h"

void load_device(struct mux_device *device) {
    char buffer[MAX_BUFFER_SIZE];

#define DEV_INT_FIELD(field, path)                                       \
    do {                                                                 \
        snprintf(buffer, sizeof(buffer), (CONF_DEVICE_PATH "%s"), path); \
        field = (int)({                                                  \
            char *ep;                                                    \
            long val = strtol(read_all_char_from(buffer), &ep, 10);      \
            *ep ? 0 : val;                                               \
        });                                                              \
    } while (0);

#define DEV_FLO_FIELD(field, path)                                       \
    do {                                                                 \
        snprintf(buffer, sizeof(buffer), (CONF_DEVICE_PATH "%s"), path); \
        field = (float)({                                                \
            char *ep;                                                    \
            double val = strtod(read_all_char_from(buffer), &ep);        \
            *ep ? 1.0 : val;                                             \
        });                                                              \
    } while (0);

#define DEV_STR_FIELD(field, path)                                       \
    do {                                                                 \
        snprintf(buffer, sizeof(buffer), (CONF_DEVICE_PATH "%s"), path); \
        strncpy(field, read_all_char_from(buffer), MAX_BUFFER_SIZE - 1); \
        field[MAX_BUFFER_SIZE - 1] = '\0';                               \
    } while (0);

#define DEV_MNT_FIELD(field, path)                                            \
    DEV_INT_FIELD(device->STORAGE.field.PARTITION, "storage/" path "/num"  ); \
    DEV_STR_FIELD(device->STORAGE.field.DEVICE,    "storage/" path "/dev"  ); \
    DEV_STR_FIELD(device->STORAGE.field.SEPARATOR, "storage/" path "/sep"  ); \
    DEV_STR_FIELD(device->STORAGE.field.MOUNT,     "storage/" path "/mount"); \
    DEV_STR_FIELD(device->STORAGE.field.TYPE,      "storage/" path "/type" ); \
    DEV_STR_FIELD(device->STORAGE.field.LABEL,     "storage/" path "/label");

    DEV_STR_FIELD(device->BOARD.NAME, "board/name")
    DEV_INT_FIELD(device->BOARD.HASNETWORK, "board/network")
    DEV_INT_FIELD(device->BOARD.HASBLUETOOTH, "board/bluetooth")
    DEV_INT_FIELD(device->BOARD.HASPORTMASTER, "board/portmaster")
    DEV_INT_FIELD(device->BOARD.HASLID, "board/lid")
    DEV_INT_FIELD(device->BOARD.HASHDMI, "board/hdmi")
    DEV_INT_FIELD(device->BOARD.HASEVENT, "board/event")
    DEV_INT_FIELD(device->BOARD.HASDEBUGFS, "board/debugfs")
    DEV_INT_FIELD(device->BOARD.HASRGB, "led/rgb")
    DEV_INT_FIELD(device->BOARD.HASSTICK, "board/stick")
    DEV_STR_FIELD(device->BOARD.JOY_HALL, "board/hall")
    DEV_STR_FIELD(device->BOARD.LED, "board/led")
    DEV_STR_FIELD(device->BOARD.RTC_CLOCK, "board/rtc_clock")
    DEV_STR_FIELD(device->BOARD.RTC_WAKE, "board/rtc_wake")

    DEV_INT_FIELD(device->AUDIO.MIN, "audio/min")
    DEV_INT_FIELD(device->AUDIO.MAX, "audio/max")

    DEV_INT_FIELD(device->MUX.WIDTH, "mux/width")
    DEV_INT_FIELD(device->MUX.HEIGHT, "mux/height")

    DEV_MNT_FIELD(BOOT, "boot")
    DEV_MNT_FIELD(ROM, "rom")
    DEV_MNT_FIELD(ROOT, "root")
    DEV_MNT_FIELD(SDCARD, "sdcard")
    DEV_MNT_FIELD(USB, "usb")

    DEV_STR_FIELD(device->CPU.DEFAULT, "cpu/default")
    DEV_STR_FIELD(device->CPU.AVAILABLE, "cpu/available")
    DEV_STR_FIELD(device->CPU.GOVERNOR, "cpu/governor")
    DEV_STR_FIELD(device->CPU.SCALER, "cpu/scaler")

    DEV_STR_FIELD(device->NETWORK.MODULE, "network/module")
    DEV_STR_FIELD(device->NETWORK.NAME, "network/name")
    DEV_STR_FIELD(device->NETWORK.TYPE, "network/type")
    DEV_STR_FIELD(device->NETWORK.INTERFACE, "network/iface")
    DEV_STR_FIELD(device->NETWORK.STATE, "network/state")

    DEV_INT_FIELD(device->SCREEN.BRIGHT, "screen/bright")
    DEV_INT_FIELD(device->SCREEN.WAIT, "screen/wait")
    DEV_STR_FIELD(device->SCREEN.DEVICE, "screen/device")
    DEV_STR_FIELD(device->SCREEN.HDMI, "screen/hdmi")
    DEV_INT_FIELD(device->SCREEN.WIDTH, "screen/width")
    DEV_INT_FIELD(device->SCREEN.HEIGHT, "screen/height")

    DEV_INT_FIELD(device->SCREEN.ROTATE, "screen/rotate")
    if (file_exist(CONF_DEVICE_PATH "screen/s_rotate")) DEV_INT_FIELD(device->SCREEN.ROTATE, "screen/s_rotate")

    DEV_INT_FIELD(device->SCREEN.ROTATE_PIVOT_X, "screen/rotate_pivot_x")
    DEV_INT_FIELD(device->SCREEN.ROTATE_PIVOT_Y, "screen/rotate_pivot_y")
    DEV_INT_FIELD(device->SCREEN.RENDER_OFFSET_X, "screen/render_offset_x")
    DEV_INT_FIELD(device->SCREEN.RENDER_OFFSET_Y, "screen/render_offset_y")

    DEV_FLO_FIELD(device->SCREEN.ZOOM, "screen/zoom")
    if (file_exist(CONF_DEVICE_PATH "screen/s_zoom")) DEV_FLO_FIELD(device->SCREEN.ZOOM, "screen/s_zoom")
    device->SCREEN.ZOOM_WIDTH = device->SCREEN.ZOOM;
    device->SCREEN.ZOOM_HEIGHT = device->SCREEN.ZOOM;

    DEV_INT_FIELD(device->SCREEN.INTERNAL.WIDTH, "screen/internal/width")
    DEV_INT_FIELD(device->SCREEN.INTERNAL.HEIGHT, "screen/internal/height")
    DEV_INT_FIELD(device->SCREEN.EXTERNAL.WIDTH, "screen/external/width")
    DEV_INT_FIELD(device->SCREEN.EXTERNAL.HEIGHT, "screen/external/height")

    DEV_INT_FIELD(device->SDL.SCALER, "sdl/scaler")
    DEV_INT_FIELD(device->SDL.ROTATE, "sdl/rotate")

    DEV_STR_FIELD(device->BATTERY.CAPACITY, "battery/capacity")
    DEV_STR_FIELD(device->BATTERY.HEALTH, "battery/health")
    DEV_STR_FIELD(device->BATTERY.VOLTAGE, "battery/voltage")
    DEV_STR_FIELD(device->BATTERY.CHARGER, "battery/charger")
    DEV_INT_FIELD(device->BATTERY.VOLT_MIN, "battery/volt_min")
    DEV_INT_FIELD(device->BATTERY.VOLT_MAX, "battery/volt_max")
    DEV_INT_FIELD(device->BATTERY.SIZE, "battery/size")
    DEV_STR_FIELD(device->BATTERY.CURVE.CHARGE, "battery/curve/charge")
    DEV_STR_FIELD(device->BATTERY.CURVE.DISCHARGE, "battery/curve/discharge")

#undef DEV_INT_FIELD
#undef DEV_STR_FIELD
#undef DEV_MNT_FIELD

    snprintf(mux_dim, sizeof(mux_dim), "%dx%d/", device->MUX.WIDTH, device->MUX.HEIGHT);
}
