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

#define DEV_ALG_FIELD(input, field, method, path)                                              \
    DEV_INT_FIELD(device->input.ANALOG.field.UP,    "input/" method "/analog/" path "/up"   ); \
    DEV_INT_FIELD(device->input.ANALOG.field.DOWN,  "input/" method "/analog/" path "/down" ); \
    DEV_INT_FIELD(device->input.ANALOG.field.LEFT,  "input/" method "/analog/" path "/left" ); \
    DEV_INT_FIELD(device->input.ANALOG.field.RIGHT, "input/" method "/analog/" path "/right"); \
    DEV_INT_FIELD(device->input.ANALOG.field.CLICK, "input/" method "/analog/" path "/click");

#define DEV_DPA_FIELD(input, method)                                        \
    DEV_INT_FIELD(device->input.DPAD.UP,    "input/" method "/dpad/up"   ); \
    DEV_INT_FIELD(device->input.DPAD.DOWN,  "input/" method "/dpad/down" ); \
    DEV_INT_FIELD(device->input.DPAD.LEFT,  "input/" method "/dpad/left" ); \
    DEV_INT_FIELD(device->input.DPAD.RIGHT, "input/" method "/dpad/right");

#define DEV_BTN_FIELD(input, method)                                                        \
    DEV_INT_FIELD(device->input.BUTTON.A,           "input/" method "/button/a"          ); \
    DEV_INT_FIELD(device->input.BUTTON.B,           "input/" method "/button/b"          ); \
    DEV_INT_FIELD(device->input.BUTTON.C,           "input/" method "/button/c"          ); \
    DEV_INT_FIELD(device->input.BUTTON.X,           "input/" method "/button/x"          ); \
    DEV_INT_FIELD(device->input.BUTTON.Y,           "input/" method "/button/y"          ); \
    DEV_INT_FIELD(device->input.BUTTON.Z,           "input/" method "/button/z"          ); \
    DEV_INT_FIELD(device->input.BUTTON.L1,          "input/" method "/button/l1"         ); \
    DEV_INT_FIELD(device->input.BUTTON.L2,          "input/" method "/button/l2"         ); \
    DEV_INT_FIELD(device->input.BUTTON.L3,          "input/" method "/button/l3"         ); \
    DEV_INT_FIELD(device->input.BUTTON.R1,          "input/" method "/button/r1"         ); \
    DEV_INT_FIELD(device->input.BUTTON.R2,          "input/" method "/button/r2"         ); \
    DEV_INT_FIELD(device->input.BUTTON.R3,          "input/" method "/button/r3"         ); \
    DEV_INT_FIELD(device->input.BUTTON.MENU_SHORT,  "input/" method "/button/menu_short" ); \
    DEV_INT_FIELD(device->input.BUTTON.MENU_LONG,   "input/" method "/button/menu_long"  ); \
    DEV_INT_FIELD(device->input.BUTTON.SELECT,      "input/" method "/button/select"     ); \
    DEV_INT_FIELD(device->input.BUTTON.START,       "input/" method "/button/start"      ); \
    DEV_INT_FIELD(device->input.BUTTON.SWITCH,      "input/" method "/button/switch"     ); \
    DEV_INT_FIELD(device->input.BUTTON.POWER_SHORT, "input/" method "/button/power_short"); \
    DEV_INT_FIELD(device->input.BUTTON.POWER_LONG,  "input/" method "/button/power_long" ); \
    DEV_INT_FIELD(device->input.BUTTON.VOLUME_UP,   "input/" method "/button/vol_up"     ); \
    DEV_INT_FIELD(device->input.BUTTON.VOLUME_DOWN, "input/" method "/button/vol_down"   );

    DEV_INT_FIELD(device->DEVICE.HAS_NETWORK, "board/network")
    DEV_INT_FIELD(device->DEVICE.HAS_BLUETOOTH, "board/bluetooth")
    DEV_INT_FIELD(device->DEVICE.HAS_PORTMASTER, "board/portmaster")
    DEV_INT_FIELD(device->DEVICE.HAS_LID, "board/lid")
    DEV_INT_FIELD(device->DEVICE.HAS_HDMI, "board/hdmi")
    DEV_INT_FIELD(device->DEVICE.EVENT, "board/event")
    DEV_INT_FIELD(device->DEVICE.DEBUGFS, "board/debugfs")
    DEV_STR_FIELD(device->DEVICE.NAME, "board/name")
    DEV_STR_FIELD(device->DEVICE.RTC_CLOCK, "board/rtc_clock")
    DEV_STR_FIELD(device->DEVICE.RTC_WAKE, "board/rtc_wake")
    DEV_STR_FIELD(device->DEVICE.LED, "board/led")

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

    DEV_FLO_FIELD(device->SCREEN.ZOOM, "screen/zoom")
    if (file_exist(CONF_DEVICE_PATH "screen/s_zoom")) DEV_FLO_FIELD(device->SCREEN.ZOOM, "screen/s_zoom")

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

    DEV_INT_FIELD(device->INPUT_EVENT.AXIS, "input/axis")
    DEV_STR_FIELD(device->INPUT_EVENT.JOY_GENERAL, "input/general")
    DEV_STR_FIELD(device->INPUT_EVENT.JOY_POWER, "input/power")
    DEV_STR_FIELD(device->INPUT_EVENT.JOY_VOLUME, "input/volume")
    DEV_STR_FIELD(device->INPUT_EVENT.JOY_EXTRA, "input/extra")

    DEV_INT_FIELD(device->INPUT_CODE.DPAD.UP, "input/code/dpad/up")
    DEV_INT_FIELD(device->INPUT_CODE.DPAD.DOWN, "input/code/dpad/down")
    DEV_INT_FIELD(device->INPUT_CODE.DPAD.LEFT, "input/code/dpad/left")
    DEV_INT_FIELD(device->INPUT_CODE.DPAD.RIGHT, "input/code/dpad/right")

    DEV_ALG_FIELD(INPUT_CODE, LEFT, "code", "left")
    DEV_ALG_FIELD(INPUT_CODE, RIGHT, "code", "right")

    DEV_DPA_FIELD(INPUT_CODE, "code")
    DEV_BTN_FIELD(INPUT_CODE, "code")

    DEV_ALG_FIELD(INPUT_TYPE, LEFT, "type", "left")
    DEV_ALG_FIELD(INPUT_TYPE, RIGHT, "type", "right")

    DEV_DPA_FIELD(INPUT_TYPE, "type")
    DEV_BTN_FIELD(INPUT_TYPE, "type")

#undef DEV_INT_FIELD
#undef DEV_STR_FIELD
#undef DEV_MNT_FIELD
#undef DEV_ALG_FIELD
#undef DEV_DPA_FIELD
#undef DEV_BTN_FIELD

    snprintf(mux_dimension, sizeof(mux_dimension), "%dx%d/", device->MUX.WIDTH, device->MUX.HEIGHT);
}
