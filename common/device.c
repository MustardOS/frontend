#include "common.h"
#include "options.h"
#include "device.h"
#include "mini/mini.h"

void load_device(struct mux_device *device) {
    static char device_file[MAX_BUFFER_SIZE];
    snprintf(device_file, sizeof(device_file), "%s/config/device.txt", INTERNAL_PATH);

    static char device_config[MAX_BUFFER_SIZE];
    snprintf(device_config, sizeof(device_config), "%s/device/%s/config.ini",
             INTERNAL_PATH, str_tolower(read_text_from_file(device_file)));

    mini_t * muos_device = mini_try_load(device_config);

    strncpy(device->DEVICE.NAME, get_ini_string(muos_device, "device", "name", "Unknown"),
            MAX_BUFFER_SIZE - 1);
    device->DEVICE.NAME[MAX_BUFFER_SIZE - 1] = '\0';
    device->DEVICE.HAS_NETWORK = get_ini_int(muos_device, "device", "network", 0);
    device->DEVICE.HAS_BLUETOOTH = get_ini_int(muos_device, "device", "bluetooth", 0);
    device->DEVICE.HAS_PORTMASTER = get_ini_int(muos_device, "device", "portmaster", 0);
    device->DEVICE.HAS_LID = get_ini_int(muos_device, "device", "lid", 0);
    device->DEVICE.HAS_HDMI = get_ini_int(muos_device, "device", "hdmi", 0);
    device->DEVICE.EVENT = get_ini_int(muos_device, "device", "event", 4);
    device->DEVICE.DEBUGFS = get_ini_int(muos_device, "device", "debugfs", 0);
    strncpy(device->DEVICE.RTC, get_ini_string(muos_device, "device", "rtc", "?"),
            MAX_BUFFER_SIZE - 1);
    device->DEVICE.RTC[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->DEVICE.LED, get_ini_string(muos_device, "device", "led", "?"),
            MAX_BUFFER_SIZE - 1);
    device->DEVICE.LED[MAX_BUFFER_SIZE - 1] = '\0';

    device->MUX.WIDTH = get_ini_int(muos_device, "mux", "width", 640);
    device->MUX.HEIGHT = get_ini_int(muos_device, "mux", "height", 480);
    device->MUX.ITEM.COUNT = get_ini_int(muos_device, "mux", "item_count", 13);
    device->MUX.ITEM.HEIGHT = get_ini_int(muos_device, "mux", "item_height", 28);
    device->MUX.ITEM.PANEL = get_ini_int(muos_device, "mux", "item_panel", 30);
    device->MUX.ITEM.PREV_LOW = get_ini_int(muos_device, "mux", "item_prev_low", 5);
    device->MUX.ITEM.PREV_HIGH = get_ini_int(muos_device, "mux", "item_prev_high", 7);
    device->MUX.ITEM.NEXT_LOW = get_ini_int(muos_device, "mux", "item_next_low", 6);
    device->MUX.ITEM.NEXT_HIGH = get_ini_int(muos_device, "mux", "item_next_high", 7);

    strncpy(device->FIRMWARE.BOOT.OUT, get_ini_string(muos_device, "firmware.boot", "out", "?"),
            MAX_BUFFER_SIZE - 1);
    device->FIRMWARE.BOOT.OUT[MAX_BUFFER_SIZE - 1] = '\0';
    device->FIRMWARE.BOOT.SEEK = get_ini_int(muos_device, "firmware.boot", "seek", 0);

    strncpy(device->FIRMWARE.PACKAGE.OUT, get_ini_string(muos_device, "firmware.package", "out", "?"),
            MAX_BUFFER_SIZE - 1);
    device->FIRMWARE.PACKAGE.OUT[MAX_BUFFER_SIZE - 1] = '\0';
    device->FIRMWARE.PACKAGE.SEEK = get_ini_int(muos_device, "firmware.package", "seek", 0);

    strncpy(device->STORAGE.BOOT.DEVICE, get_ini_string(muos_device, "storage.boot", "dev", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.BOOT.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.BOOT.PARTITION = get_ini_int(muos_device, "storage.boot", "num", 1);
    strncpy(device->STORAGE.BOOT.MOUNT, get_ini_string(muos_device, "storage.boot", "mount", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.BOOT.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->STORAGE.BOOT.TYPE, get_ini_string(muos_device, "storage.boot", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.BOOT.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->STORAGE.ROM.DEVICE, get_ini_string(muos_device, "storage.rom", "dev", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROM.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROM.PARTITION = get_ini_int(muos_device, "storage.rom", "num", 1);
    strncpy(device->STORAGE.ROM.MOUNT, get_ini_string(muos_device, "storage.rom", "mount", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROM.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->STORAGE.ROM.TYPE, get_ini_string(muos_device, "storage.rom", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROM.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->STORAGE.ROOT.DEVICE, get_ini_string(muos_device, "storage.root", "dev", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROOT.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROOT.PARTITION = get_ini_int(muos_device, "storage.root", "num", 1);
    strncpy(device->STORAGE.ROOT.MOUNT, get_ini_string(muos_device, "storage.root", "mount", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROOT.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->STORAGE.ROOT.TYPE, get_ini_string(muos_device, "storage.root", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROOT.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->STORAGE.SDCARD.DEVICE, get_ini_string(muos_device, "storage.sdcard", "dev", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.SDCARD.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.SDCARD.PARTITION = get_ini_int(muos_device, "storage.sdcard", "num", 1);
    strncpy(device->STORAGE.SDCARD.MOUNT, get_ini_string(muos_device, "storage.sdcard", "mount", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.SDCARD.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->STORAGE.SDCARD.TYPE, get_ini_string(muos_device, "storage.sdcard", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.SDCARD.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->STORAGE.USB.DEVICE, get_ini_string(muos_device, "storage.usb", "dev", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.USB.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.USB.PARTITION = get_ini_int(muos_device, "storage.usb", "num", 1);
    strncpy(device->STORAGE.USB.MOUNT, get_ini_string(muos_device, "storage.usb", "mount", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.USB.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->STORAGE.USB.TYPE, get_ini_string(muos_device, "storage.usb", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->STORAGE.USB.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->CPU.GOVERNOR, get_ini_string(muos_device, "cpu", "governor", "?"),
            MAX_BUFFER_SIZE - 1);
    device->CPU.GOVERNOR[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->CPU.SCALER, get_ini_string(muos_device, "cpu", "scaler", "?"),
            MAX_BUFFER_SIZE - 1);
    device->CPU.SCALER[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->NETWORK.MODULE, get_ini_string(muos_device, "network", "module", "?"),
            MAX_BUFFER_SIZE - 1);
    device->NETWORK.MODULE[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->NETWORK.NAME, get_ini_string(muos_device, "network", "name", "?"),
            MAX_BUFFER_SIZE - 1);
    device->NETWORK.NAME[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->NETWORK.TYPE, get_ini_string(muos_device, "network", "type", "?"),
            MAX_BUFFER_SIZE - 1);
    device->NETWORK.TYPE[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->NETWORK.INTERFACE, get_ini_string(muos_device, "network", "iface", "?"),
            MAX_BUFFER_SIZE - 1);
    device->NETWORK.INTERFACE[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->NETWORK.STATE, get_ini_string(muos_device, "network", "state", "?"),
            MAX_BUFFER_SIZE - 1);
    device->NETWORK.STATE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->SCREEN.DEVICE, get_ini_string(muos_device, "screen", "device", "/dev/fb0"),
            MAX_BUFFER_SIZE - 1);
    device->SCREEN.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->SCREEN.BRIGHT = get_ini_int(muos_device, "screen", "bright", 90);
    device->SCREEN.BUFFER = get_ini_hex(muos_device, "screen", "buffer");
    device->SCREEN.WIDTH = get_ini_int(muos_device, "screen", "width", 640);
    device->SCREEN.HEIGHT = get_ini_int(muos_device, "screen", "height", 480);
    device->SCREEN.ROTATE = get_ini_int(muos_device, "screen", "rotate", 0);
    device->SCREEN.WAIT = get_ini_int(muos_device, "screen", "wait", 255);

    strncpy(device->AUDIO.CONTROL, get_ini_string(muos_device, "audio", "control", "?"),
            MAX_BUFFER_SIZE - 1);
    device->AUDIO.CONTROL[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->AUDIO.CHANNEL, get_ini_string(muos_device, "audio", "channel", "?"),
            MAX_BUFFER_SIZE - 1);
    device->AUDIO.CHANNEL[MAX_BUFFER_SIZE - 1] = '\0';
    device->AUDIO.MIN = get_ini_int(muos_device, "audio", "min", 0);
    device->AUDIO.MAX = get_ini_int(muos_device, "audio", "max", 255);

    device->SDL.SCALER = get_ini_int(muos_device, "sdl", "scaler", 0);
    device->SDL.ROTATE = get_ini_int(muos_device, "sdl", "rotate", 0);

    strncpy(device->BATTERY.CAPACITY, get_ini_string(muos_device, "battery", "capacity", "?"),
            MAX_BUFFER_SIZE - 1);
    device->BATTERY.CAPACITY[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->BATTERY.HEALTH, get_ini_string(muos_device, "battery", "health", "?"),
            MAX_BUFFER_SIZE - 1);
    device->BATTERY.HEALTH[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->BATTERY.VOLTAGE, get_ini_string(muos_device, "battery", "voltage", "?"),
            MAX_BUFFER_SIZE - 1);
    device->BATTERY.VOLTAGE[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->BATTERY.CHARGER, get_ini_string(muos_device, "battery", "charger", "?"),
            MAX_BUFFER_SIZE - 1);
    device->BATTERY.CHARGER[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->INPUT.EV0, get_ini_string(muos_device, "input", "ev0", "/dev/input/event0"),
            MAX_BUFFER_SIZE - 1);
    device->INPUT.EV0[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(device->INPUT.EV1, get_ini_string(muos_device, "input", "ev1", "/dev/input/event1"),
            MAX_BUFFER_SIZE - 1);
    device->INPUT.EV1[MAX_BUFFER_SIZE - 1] = '\0';
    device->INPUT.AXIS_MIN = get_ini_hex(muos_device, "input", "axis_min");
    device->INPUT.AXIS_MAX = get_ini_hex(muos_device, "input", "axis_max");

    device->RAW_INPUT.DPAD.UP = get_ini_int(muos_device, "raw_input.dpad", "dp_up", 0);
    device->RAW_INPUT.DPAD.DOWN = get_ini_int(muos_device, "raw_input.dpad", "dp_down", 0);
    device->RAW_INPUT.DPAD.LEFT = get_ini_int(muos_device, "raw_input.dpad", "dp_left", 0);
    device->RAW_INPUT.DPAD.RIGHT = get_ini_int(muos_device, "raw_input.dpad", "dp_right", 0);

    device->RAW_INPUT.ANALOG.LEFT.UP = get_ini_int(muos_device, "raw_input.analog.left", "an_left_up", 0);
    device->RAW_INPUT.ANALOG.LEFT.DOWN = get_ini_int(muos_device, "raw_input.analog.left", "an_left_down", 0);
    device->RAW_INPUT.ANALOG.LEFT.LEFT = get_ini_int(muos_device, "raw_input.analog.left", "an_left_left", 0);
    device->RAW_INPUT.ANALOG.LEFT.RIGHT = get_ini_int(muos_device, "raw_input.analog.left", "an_left_right", 0);
    device->RAW_INPUT.ANALOG.LEFT.CLICK = get_ini_int(muos_device, "raw_input.analog.left", "an_left_click", 0);

    device->RAW_INPUT.ANALOG.RIGHT.UP = get_ini_int(muos_device, "raw_input.analog.right", "an_right_up", 0);
    device->RAW_INPUT.ANALOG.RIGHT.DOWN = get_ini_int(muos_device, "raw_input.analog.right", "an_right_down", 0);
    device->RAW_INPUT.ANALOG.RIGHT.LEFT = get_ini_int(muos_device, "raw_input.analog.right", "an_right_left", 0);
    device->RAW_INPUT.ANALOG.RIGHT.RIGHT = get_ini_int(muos_device, "raw_input.analog.right", "an_right_right", 0);
    device->RAW_INPUT.ANALOG.RIGHT.CLICK = get_ini_int(muos_device, "raw_input.analog.right", "an_right_click", 0);

    device->RAW_INPUT.BUTTON.A = get_ini_int(muos_device, "raw_input.button", "a", 0);
    device->RAW_INPUT.BUTTON.B = get_ini_int(muos_device, "raw_input.button", "b", 0);
    device->RAW_INPUT.BUTTON.C = get_ini_int(muos_device, "raw_input.button", "c", 0);
    device->RAW_INPUT.BUTTON.X = get_ini_int(muos_device, "raw_input.button", "x", 0);
    device->RAW_INPUT.BUTTON.Y = get_ini_int(muos_device, "raw_input.button", "y", 0);
    device->RAW_INPUT.BUTTON.Z = get_ini_int(muos_device, "raw_input.button", "z", 0);
    device->RAW_INPUT.BUTTON.L1 = get_ini_int(muos_device, "raw_input.button", "l1", 0);
    device->RAW_INPUT.BUTTON.L2 = get_ini_int(muos_device, "raw_input.button", "l2", 0);
    device->RAW_INPUT.BUTTON.L3 = get_ini_int(muos_device, "raw_input.button", "l3", 0);
    device->RAW_INPUT.BUTTON.R1 = get_ini_int(muos_device, "raw_input.button", "r1", 0);
    device->RAW_INPUT.BUTTON.R2 = get_ini_int(muos_device, "raw_input.button", "r2", 0);
    device->RAW_INPUT.BUTTON.R3 = get_ini_int(muos_device, "raw_input.button", "r3", 0);
    device->RAW_INPUT.BUTTON.MENU_SHORT = get_ini_int(muos_device, "raw_input.button", "menu_short", 0);
    device->RAW_INPUT.BUTTON.MENU_LONG = get_ini_int(muos_device, "raw_input.button", "menu_long", 0);
    device->RAW_INPUT.BUTTON.SELECT = get_ini_int(muos_device, "raw_input.button", "select", 0);
    device->RAW_INPUT.BUTTON.START = get_ini_int(muos_device, "raw_input.button", "start", 0);
    device->RAW_INPUT.BUTTON.POWER_SHORT = get_ini_int(muos_device, "raw_input.button", "power_short", 0);
    device->RAW_INPUT.BUTTON.POWER_LONG = get_ini_int(muos_device, "raw_input.button", "power_long", 0);
    device->RAW_INPUT.BUTTON.VOLUME_UP = get_ini_int(muos_device, "raw_input.button", "vol_up", 0);
    device->RAW_INPUT.BUTTON.VOLUME_DOWN = get_ini_int(muos_device, "raw_input.button", "vol_down", 0);

    mini_free(muos_device);
}
