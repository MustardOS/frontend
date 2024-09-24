#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "device.h"

void load_device(struct mux_device *device) {
    const char *base_path = "/run/muos/device";
    char buffer[MAX_BUFFER_SIZE];

#define DEV_INT_FIELD(field, path)                               \
    snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path);  \
    field = (int)({                                              \
        char *ep;                                                \
        long val = strtol(read_text_from_file(buffer), &ep, 10); \
        *ep ? 0 : val;                                           \
    });

#define DEV_STR_FIELD(field, path)                                    \
    snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path);       \
    strncpy(field, read_text_from_file(buffer), MAX_BUFFER_SIZE - 1); \
    field[MAX_BUFFER_SIZE - 1] = '\0';

#define DEV_MNT_FIELD(field, path)                                            \
    DEV_INT_FIELD(device->STORAGE.field.PARTITION, "storage/" path "/num"  ); \
    DEV_STR_FIELD(device->STORAGE.field.DEVICE,    "storage/" path "/dev"  ); \
    DEV_STR_FIELD(device->STORAGE.field.SEPARATOR, "storage/" path "/sep"  ); \
    DEV_STR_FIELD(device->STORAGE.field.MOUNT,     "storage/" path "/mount"); \
    DEV_STR_FIELD(device->STORAGE.field.TYPE,      "storage/" path "/type" );

#define DEV_ALG_FIELD(field, path)                                                           \
    DEV_INT_FIELD(device->RAW_INPUT.ANALOG.field.UP,    "input/code/analog/" path "/up"   ); \
    DEV_INT_FIELD(device->RAW_INPUT.ANALOG.field.DOWN,  "input/code/analog/" path "/down" ); \
    DEV_INT_FIELD(device->RAW_INPUT.ANALOG.field.LEFT,  "input/code/analog/" path "/left" ); \
    DEV_INT_FIELD(device->RAW_INPUT.ANALOG.field.RIGHT, "input/code/analog/" path "/right"); \
    DEV_INT_FIELD(device->RAW_INPUT.ANALOG.field.CLICK, "input/code/analog/" path "/click");

    DEV_INT_FIELD(device->DEVICE.HAS_NETWORK, "board/network")
    DEV_INT_FIELD(device->DEVICE.HAS_BLUETOOTH, "board/bluetooth")
    DEV_INT_FIELD(device->DEVICE.HAS_PORTMASTER, "board/portmaster")
    DEV_INT_FIELD(device->DEVICE.HAS_LID, "board/lid")
    DEV_INT_FIELD(device->DEVICE.HAS_HDMI, "board/hdmi")
    DEV_INT_FIELD(device->DEVICE.EVENT, "board/event")
    DEV_INT_FIELD(device->DEVICE.DEBUGFS, "board/debugfs")
    DEV_STR_FIELD(device->DEVICE.NAME, "board/name")
    DEV_STR_FIELD(device->DEVICE.RTC, "board/rtc")
    DEV_STR_FIELD(device->DEVICE.LED, "board/led")

    DEV_INT_FIELD(device->MUX.WIDTH, "mux/width")
    DEV_INT_FIELD(device->MUX.HEIGHT, "mux/height")

    DEV_MNT_FIELD(BOOT, "boot")
    DEV_MNT_FIELD(ROM, "rom")
    DEV_MNT_FIELD(ROOT, "root")
    DEV_MNT_FIELD(SDCARD, "sdcard")
    DEV_MNT_FIELD(USB, "usb")

    DEV_STR_FIELD(device->CPU.DEFAULT, "cpu/default")
    DEV_STR_FIELD(device->CPU.GOVERNOR, "cpu/governor")
    DEV_STR_FIELD(device->CPU.SCALER, "cpu/scaler")

    DEV_STR_FIELD(device->NETWORK.MODULE, "network/module")
    DEV_STR_FIELD(device->NETWORK.NAME, "network/name")
    DEV_STR_FIELD(device->NETWORK.TYPE, "network/type")
    DEV_STR_FIELD(device->NETWORK.INTERFACE, "network/iface")
    DEV_STR_FIELD(device->NETWORK.STATE, "network/state")

    DEV_INT_FIELD(device->SCREEN.BRIGHT, "screen/bright")
    DEV_INT_FIELD(device->SCREEN.WIDTH, "screen/width")
    DEV_INT_FIELD(device->SCREEN.HEIGHT, "screen/height")
    DEV_INT_FIELD(device->SCREEN.ROTATE, "screen/rotate")
    DEV_INT_FIELD(device->SCREEN.WAIT, "screen/wait")
    DEV_STR_FIELD(device->SCREEN.DEVICE, "screen/device")
    DEV_STR_FIELD(device->SCREEN.HDMI, "screen/hdmi")

    DEV_INT_FIELD(device->AUDIO.MIN, "audio/min")
    DEV_INT_FIELD(device->AUDIO.MAX, "audio/max")
    DEV_STR_FIELD(device->AUDIO.CONTROL, "audio/control")
    DEV_STR_FIELD(device->AUDIO.CHANNEL, "audio/channel")

    DEV_INT_FIELD(device->SDL.SCALER, "sdl/scaler")
    DEV_INT_FIELD(device->SDL.ROTATE, "sdl/rotate")

    DEV_STR_FIELD(device->BATTERY.CAPACITY, "battery/capacity")
    DEV_STR_FIELD(device->BATTERY.HEALTH, "battery/health")
    DEV_STR_FIELD(device->BATTERY.VOLTAGE, "battery/voltage")
    DEV_STR_FIELD(device->BATTERY.CHARGER, "battery/charger")

    DEV_INT_FIELD(device->INPUT.AXIS, "input/axis")
    DEV_STR_FIELD(device->INPUT.EV0, "input/ev0")
    DEV_STR_FIELD(device->INPUT.EV1, "input/ev1")

    DEV_INT_FIELD(device->RAW_INPUT.DPAD.UP, "input/code/dpad/up")
    DEV_INT_FIELD(device->RAW_INPUT.DPAD.DOWN, "input/code/dpad/down")
    DEV_INT_FIELD(device->RAW_INPUT.DPAD.LEFT, "input/code/dpad/left")
    DEV_INT_FIELD(device->RAW_INPUT.DPAD.RIGHT, "input/code/dpad/right")

    DEV_ALG_FIELD(LEFT, "left")
    DEV_ALG_FIELD(RIGHT, "right")

    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.A, "input/code/button/a")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.B, "input/code/button/b")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.C, "input/code/button/c")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.X, "input/code/button/x")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.Y, "input/code/button/y")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.Z, "input/code/button/z")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.L1, "input/code/button/l1")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.L2, "input/code/button/l2")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.L3, "input/code/button/l3")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.R1, "input/code/button/r1")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.R2, "input/code/button/r2")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.R3, "input/code/button/r3")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.MENU_SHORT, "input/code/button/menu_short")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.MENU_LONG, "input/code/button/menu_long")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.SELECT, "input/code/button/select")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.START, "input/code/button/start")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.POWER_SHORT, "input/code/button/power_short")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.POWER_LONG, "input/code/button/power_long")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.VOLUME_UP, "input/code/button/vol_up")
    DEV_INT_FIELD(device->RAW_INPUT.BUTTON.VOLUME_DOWN, "input/code/button/vol_down")

#undef DEV_INT_FIELD
#undef DEV_STR_FIELD
#undef DEV_MNT_FIELD
#undef DEV_ALG_FIELD
}
