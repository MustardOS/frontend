#include "common.h"
#include "options.h"
#include "device.h"

void load_device(struct mux_device *device) {
    device->DEVICE.HAS_NETWORK = atoi(read_text_from_file("/run/muos/device/board/network"));
    device->DEVICE.HAS_BLUETOOTH = atoi(read_text_from_file("/run/muos/device/board/bluetooth"));
    device->DEVICE.HAS_PORTMASTER = atoi(read_text_from_file("/run/muos/device/board/portmaster"));
    device->DEVICE.HAS_LID = atoi(read_text_from_file("/run/muos/device/board/lid"));
    device->DEVICE.HAS_HDMI = atoi(read_text_from_file("/run/muos/device/board/hdmi"));
    device->DEVICE.EVENT = atoi(read_text_from_file("/run/muos/device/board/event"));
    device->DEVICE.DEBUGFS = atoi(read_text_from_file("/run/muos/device/board/debugfs"));
    strncpy(device->DEVICE.NAME, read_text_from_file("/run/muos/device/board/name"), MAX_BUFFER_SIZE - 1);
    strncpy(device->DEVICE.RTC, read_text_from_file("/run/muos/device/board/rtc"), MAX_BUFFER_SIZE - 1);
    strncpy(device->DEVICE.LED, read_text_from_file("/run/muos/device/board/led"), MAX_BUFFER_SIZE - 1);
    device->DEVICE.NAME[MAX_BUFFER_SIZE - 1] = '\0';
    device->DEVICE.RTC[MAX_BUFFER_SIZE - 1] = '\0';
    device->DEVICE.LED[MAX_BUFFER_SIZE - 1] = '\0';

    device->MUX.WIDTH = atoi(read_text_from_file("/run/muos/device/mux/width"));
    device->MUX.HEIGHT = atoi(read_text_from_file("/run/muos/device/mux/height"));
    device->MUX.ITEM.COUNT = atoi(read_text_from_file("/run/muos/device/mux/item_count"));
    device->MUX.ITEM.HEIGHT = atoi(read_text_from_file("/run/muos/device/mux/item_height"));
    device->MUX.ITEM.PANEL = atoi(read_text_from_file("/run/muos/device/mux/item_panel"));

    device->STORAGE.BOOT.PARTITION = atoi(read_text_from_file("/run/muos/device/storage/boot/num"));
    strncpy(device->STORAGE.BOOT.DEVICE, read_text_from_file("/run/muos/device/storage/boot/dev"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.BOOT.SEPARATOR, read_text_from_file("/run/muos/device/storage/boot/sep"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.BOOT.MOUNT, read_text_from_file("/run/muos/device/storage/boot/mount"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.BOOT.TYPE, read_text_from_file("/run/muos/device/storage/boot/type"), MAX_BUFFER_SIZE - 1);
    device->STORAGE.BOOT.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.BOOT.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.BOOT.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    device->STORAGE.ROM.PARTITION = atoi(read_text_from_file("/run/muos/device/storage/rom/num"));
    strncpy(device->STORAGE.ROM.DEVICE, read_text_from_file("/run/muos/device/storage/rom/dev"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROM.SEPARATOR, read_text_from_file("/run/muos/device/storage/rom/sep"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROM.MOUNT, read_text_from_file("/run/muos/device/storage/rom/mount"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROM.TYPE, read_text_from_file("/run/muos/device/storage/rom/type"), MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROM.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROM.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROM.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    device->STORAGE.ROOT.PARTITION = atoi(read_text_from_file("/run/muos/device/storage/root/num"));
    strncpy(device->STORAGE.ROOT.DEVICE, read_text_from_file("/run/muos/device/storage/root/dev"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROOT.SEPARATOR, read_text_from_file("/run/muos/device/storage/root/sep"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROOT.MOUNT, read_text_from_file("/run/muos/device/storage/root/mount"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.ROOT.TYPE, read_text_from_file("/run/muos/device/storage/root/type"), MAX_BUFFER_SIZE - 1);
    device->STORAGE.ROOT.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROOT.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.ROOT.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    device->STORAGE.SDCARD.PARTITION = atoi(read_text_from_file("/run/muos/device/storage/sdcard/num"));
    strncpy(device->STORAGE.SDCARD.DEVICE, read_text_from_file("/run/muos/device/storage/sdcard/dev"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.SDCARD.SEPARATOR, read_text_from_file("/run/muos/device/storage/sdcard/sep"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.SDCARD.MOUNT, read_text_from_file("/run/muos/device/storage/sdcard/mount"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.SDCARD.TYPE, read_text_from_file("/run/muos/device/storage/sdcard/type"), MAX_BUFFER_SIZE - 1);
    device->STORAGE.SDCARD.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.SDCARD.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.SDCARD.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    device->STORAGE.USB.PARTITION = atoi(read_text_from_file("/run/muos/device/storage/usb/num"));
    strncpy(device->STORAGE.USB.DEVICE, read_text_from_file("/run/muos/device/storage/usb/dev"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.USB.SEPARATOR, read_text_from_file("/run/muos/device/storage/usb/sep"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.USB.MOUNT, read_text_from_file("/run/muos/device/storage/usb/mount"), MAX_BUFFER_SIZE - 1);
    strncpy(device->STORAGE.USB.TYPE, read_text_from_file("/run/muos/device/storage/usb/type"), MAX_BUFFER_SIZE - 1);
    device->STORAGE.USB.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.USB.MOUNT[MAX_BUFFER_SIZE - 1] = '\0';
    device->STORAGE.USB.TYPE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->CPU.GOVERNOR, read_text_from_file("/run/muos/device/cpu/governor"), MAX_BUFFER_SIZE - 1);
    strncpy(device->CPU.SCALER, read_text_from_file("/run/muos/device/cpu/scaler"), MAX_BUFFER_SIZE - 1);
    device->CPU.GOVERNOR[MAX_BUFFER_SIZE - 1] = '\0';
    device->CPU.SCALER[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(device->NETWORK.MODULE, read_text_from_file("/run/muos/device/network/module"), MAX_BUFFER_SIZE - 1);
    strncpy(device->NETWORK.NAME, read_text_from_file("/run/muos/device/network/name"), MAX_BUFFER_SIZE - 1);
    strncpy(device->NETWORK.TYPE, read_text_from_file("/run/muos/device/network/type"), MAX_BUFFER_SIZE - 1);
    strncpy(device->NETWORK.INTERFACE, read_text_from_file("/run/muos/device/network/iface"), MAX_BUFFER_SIZE - 1);
    strncpy(device->NETWORK.STATE, read_text_from_file("/run/muos/device/network/state"), MAX_BUFFER_SIZE - 1);
    device->NETWORK.MODULE[MAX_BUFFER_SIZE - 1] = '\0';
    device->NETWORK.NAME[MAX_BUFFER_SIZE - 1] = '\0';
    device->NETWORK.TYPE[MAX_BUFFER_SIZE - 1] = '\0';
    device->NETWORK.INTERFACE[MAX_BUFFER_SIZE - 1] = '\0';
    device->NETWORK.STATE[MAX_BUFFER_SIZE - 1] = '\0';
    
    device->SCREEN.BRIGHT = atoi(read_text_from_file("/run/muos/device/screen/bright"));
    device->SCREEN.BUFFER = atoi(read_text_from_file("/run/muos/device/screen/buffer"));
    device->SCREEN.WIDTH = atoi(read_text_from_file("/run/muos/device/screen/width"));
    device->SCREEN.HEIGHT = atoi(read_text_from_file("/run/muos/device/screen/height"));
    device->SCREEN.ROTATE = atoi(read_text_from_file("/run/muos/device/screen/rotate"));
    device->SCREEN.WAIT = atoi(read_text_from_file("/run/muos/device/screen/wait"));
    strncpy(device->SCREEN.DEVICE, read_text_from_file("/run/muos/device/screen/device"), MAX_BUFFER_SIZE - 1);
    strncpy(device->SCREEN.HDMI, read_text_from_file("/run/muos/device/screen/hdmi"), MAX_BUFFER_SIZE - 1);
    device->SCREEN.DEVICE[MAX_BUFFER_SIZE - 1] = '\0';
    device->SCREEN.HDMI[MAX_BUFFER_SIZE - 1] = '\0';
    
    device->AUDIO.MIN = atoi(read_text_from_file("/run/muos/device/audio/min"));
    device->AUDIO.MAX = atoi(read_text_from_file("/run/muos/device/audio/max"));
    strncpy(device->AUDIO.CONTROL, read_text_from_file("/run/muos/device/audio/control"), MAX_BUFFER_SIZE - 1);
    strncpy(device->AUDIO.CHANNEL, read_text_from_file("/run/muos/device/audio/channel"), MAX_BUFFER_SIZE - 1);
    device->AUDIO.CONTROL[MAX_BUFFER_SIZE - 1] = '\0';
    device->AUDIO.CHANNEL[MAX_BUFFER_SIZE - 1] = '\0';

    device->SDL.SCALER = atoi(read_text_from_file("/run/muos/device/sdl/scaler"));
    device->SDL.ROTATE = atoi(read_text_from_file("/run/muos/device/sdl/rotate"));

    strncpy(device->BATTERY.CAPACITY, read_text_from_file("/run/muos/device/battery/capacity"), MAX_BUFFER_SIZE - 1);
    strncpy(device->BATTERY.HEALTH, read_text_from_file("/run/muos/device/battery/health"), MAX_BUFFER_SIZE - 1);
    strncpy(device->BATTERY.VOLTAGE, read_text_from_file("/run/muos/device/battery/voltage"), MAX_BUFFER_SIZE - 1);
    strncpy(device->BATTERY.CHARGER, read_text_from_file("/run/muos/device/battery/charger"), MAX_BUFFER_SIZE - 1);
    device->BATTERY.CAPACITY[MAX_BUFFER_SIZE - 1] = '\0';
    device->BATTERY.HEALTH[MAX_BUFFER_SIZE - 1] = '\0';
    device->BATTERY.VOLTAGE[MAX_BUFFER_SIZE - 1] = '\0';
    device->BATTERY.CHARGER[MAX_BUFFER_SIZE - 1] = '\0';

    device->INPUT.AXIS_MIN = atoi(read_text_from_file("/run/muos/device/input/axis_min"));
    device->INPUT.AXIS_MAX = atoi(read_text_from_file("/run/muos/device/input/axis_max"));
    strncpy(device->INPUT.EV0, read_text_from_file("/run/muos/device/input/ev0"), MAX_BUFFER_SIZE - 1);
    strncpy(device->INPUT.EV1, read_text_from_file("/run/muos/device/input/ev1"), MAX_BUFFER_SIZE - 1);
    device->INPUT.EV0[MAX_BUFFER_SIZE - 1] = '\0';
    device->INPUT.EV1[MAX_BUFFER_SIZE - 1] = '\0';

    device->RAW_INPUT.DPAD.UP = atoi(read_text_from_file("/run/muos/device/input/dpad/up"));
    device->RAW_INPUT.DPAD.DOWN = atoi(read_text_from_file("/run/muos/device/input/dpad/down"));
    device->RAW_INPUT.DPAD.LEFT = atoi(read_text_from_file("/run/muos/device/input/dpad/left"));
    device->RAW_INPUT.DPAD.RIGHT = atoi(read_text_from_file("/run/muos/device/input/dpad/right"));

    device->RAW_INPUT.ANALOG.LEFT.UP = atoi(read_text_from_file("/run/muos/device/input/analog/left/up"));
    device->RAW_INPUT.ANALOG.LEFT.DOWN = atoi(read_text_from_file("/run/muos/device/input/analog/left/down"));
    device->RAW_INPUT.ANALOG.LEFT.LEFT = atoi(read_text_from_file("/run/muos/device/input/analog/left/left"));
    device->RAW_INPUT.ANALOG.LEFT.RIGHT = atoi(read_text_from_file("/run/muos/device/input/analog/left/right"));
    device->RAW_INPUT.ANALOG.LEFT.CLICK = atoi(read_text_from_file("/run/muos/device/input/analog/left/click"));

    device->RAW_INPUT.ANALOG.RIGHT.UP = atoi(read_text_from_file("/run/muos/device/input/analog/right/up"));
    device->RAW_INPUT.ANALOG.RIGHT.DOWN = atoi(read_text_from_file("/run/muos/device/input/analog/right/down"));
    device->RAW_INPUT.ANALOG.RIGHT.LEFT = atoi(read_text_from_file("/run/muos/device/input/analog/right/left"));
    device->RAW_INPUT.ANALOG.RIGHT.RIGHT = atoi(read_text_from_file("/run/muos/device/input/analog/right/right"));
    device->RAW_INPUT.ANALOG.RIGHT.CLICK = atoi(read_text_from_file("/run/muos/device/input/analog/right/click"));

    device->RAW_INPUT.BUTTON.A = atoi(read_text_from_file("/run/muos/device/input/button/a"));
    device->RAW_INPUT.BUTTON.B = atoi(read_text_from_file("/run/muos/device/input/button/b"));
    device->RAW_INPUT.BUTTON.C = atoi(read_text_from_file("/run/muos/device/input/button/c"));
    device->RAW_INPUT.BUTTON.X = atoi(read_text_from_file("/run/muos/device/input/button/x"));
    device->RAW_INPUT.BUTTON.Y = atoi(read_text_from_file("/run/muos/device/input/button/y"));
    device->RAW_INPUT.BUTTON.Z = atoi(read_text_from_file("/run/muos/device/input/button/z"));
    device->RAW_INPUT.BUTTON.L1 = atoi(read_text_from_file("/run/muos/device/input/button/l1"));
    device->RAW_INPUT.BUTTON.L2 = atoi(read_text_from_file("/run/muos/device/input/button/l2"));
    device->RAW_INPUT.BUTTON.L3 = atoi(read_text_from_file("/run/muos/device/input/button/l3"));
    device->RAW_INPUT.BUTTON.R1 = atoi(read_text_from_file("/run/muos/device/input/button/r1"));
    device->RAW_INPUT.BUTTON.R2 = atoi(read_text_from_file("/run/muos/device/input/button/r2"));
    device->RAW_INPUT.BUTTON.R3 = atoi(read_text_from_file("/run/muos/device/input/button/r3"));
    device->RAW_INPUT.BUTTON.MENU_SHORT = atoi(read_text_from_file("/run/muos/device/input/button/menu_short"));
    device->RAW_INPUT.BUTTON.MENU_LONG = atoi(read_text_from_file("/run/muos/device/input/button/menu_long"));
    device->RAW_INPUT.BUTTON.SELECT = atoi(read_text_from_file("/run/muos/device/input/button/select"));
    device->RAW_INPUT.BUTTON.START = atoi(read_text_from_file("/run/muos/device/input/button/start"));
    device->RAW_INPUT.BUTTON.POWER_SHORT = atoi(read_text_from_file("/run/muos/device/input/button/power_short"));
    device->RAW_INPUT.BUTTON.POWER_LONG = atoi(read_text_from_file("/run/muos/device/input/button/power_long"));
    device->RAW_INPUT.BUTTON.VOLUME_UP = atoi(read_text_from_file("/run/muos/device/input/button/vol_up"));
    device->RAW_INPUT.BUTTON.VOLUME_DOWN = atoi(read_text_from_file("/run/muos/device/input/button/vol_down"));
}
