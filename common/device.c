#include "config_reader.h"
#include "device.h"
#include "options.h"

extern char mux_dim[15];

void load_device(struct mux_device *device) {
    cfg_dir_t d;

#define DEV_STR(field, name)                      \
    do {                                          \
        const char *_v = cfg_dir_get(&d, (name)); \
        snprintf((field), sizeof(field), "%s",    \
            (_v && *_v) ? _v : "");               \
    } while (0)

#define DEV_INT(field, name)                            \
    do {                                                \
        (field) = (int16_t) cfg_dir_int(&d, (name), 0); \
    } while (0)

#define DEV_FLO(field, name, fallback)                         \
    do {                                                       \
        (field) = (float) cfg_dir_flo(&d, (name), (fallback)); \
    } while (0)

    cfg_dir_scan(&d, CONF_DEVICE_PATH "board");
    DEV_STR(device->BOARD.NAME, "name");
    DEV_INT(device->BOARD.HASNETWORK, "network");
    DEV_INT(device->BOARD.HASBLUETOOTH, "bluetooth");
    DEV_INT(device->BOARD.HASPORTMASTER, "portmaster");
    DEV_INT(device->BOARD.HASLID, "lid");
    DEV_INT(device->BOARD.HASHDMI, "hdmi");
    DEV_INT(device->BOARD.HASEVENT, "event");
    DEV_INT(device->BOARD.HASDEBUGFS, "debugfs");
    DEV_INT(device->BOARD.HASSTICK, "stick");
    DEV_INT(device->BOARD.HASTOUCH, "touch");
    DEV_STR(device->BOARD.SDL_MAP, "sdl_map");
    DEV_STR(device->BOARD.JOY_HALL, "hall");
    DEV_STR(device->BOARD.LED, "led");
    DEV_STR(device->BOARD.RTC_CLOCK, "rtc_clock");
    DEV_STR(device->BOARD.RTC_WAKE, "rtc_wake");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "led");
    DEV_INT(device->BOARD.HASRGB, "rgb");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "audio");
    DEV_INT(device->AUDIO.MIN, "min");
    DEV_INT(device->AUDIO.MAX, "max");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "mux");
    DEV_INT(device->MUX.WIDTH, "width");
    DEV_INT(device->MUX.HEIGHT, "height");

#define DEV_MNT(field, subdir)                                \
    do {                                                      \
        cfg_dir_scan(&d, CONF_DEVICE_PATH "storage/" subdir); \
        DEV_INT(device->STORAGE.field.PARTITION, "num");      \
        DEV_STR(device->STORAGE.field.DEVICE, "dev");         \
        DEV_STR(device->STORAGE.field.SEPARATOR, "sep");      \
        DEV_STR(device->STORAGE.field.MOUNT, "mount");        \
        DEV_STR(device->STORAGE.field.TYPE, "type");          \
        DEV_STR(device->STORAGE.field.LABEL, "label");        \
    } while (0)

    DEV_MNT(BOOT, "boot");
    DEV_MNT(ROM, "rom");
    DEV_MNT(ROOT, "root");
    DEV_MNT(SDCARD, "sdcard");
    DEV_MNT(USB, "usb");

#undef DEV_MNT

    cfg_dir_scan(&d, CONF_DEVICE_PATH "cpu");
    DEV_STR(device->CPU.DEFAULT, "default");
    DEV_STR(device->CPU.AVAILABLE, "available");
    DEV_STR(device->CPU.GOVERNOR, "governor");
    DEV_STR(device->CPU.SCALER, "scaler");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "network");
    DEV_STR(device->NETWORK.MODULE, "module");
    DEV_STR(device->NETWORK.NAME, "name");
    DEV_STR(device->NETWORK.TYPE, "type");
    DEV_STR(device->NETWORK.INTERFACE, "iface");
    DEV_STR(device->NETWORK.STATE, "state");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen");
    DEV_INT(device->SCREEN.BRIGHT, "bright");
    DEV_INT(device->SCREEN.WAIT, "wait");
    DEV_STR(device->SCREEN.DEVICE, "device");
    DEV_STR(device->SCREEN.HDMI, "hdmi");
    DEV_INT(device->SCREEN.WIDTH, "width");
    DEV_INT(device->SCREEN.HEIGHT, "height");

    DEV_INT(device->SCREEN.ROTATE, "rotate");
    device->SCREEN.ROTATE = (int16_t) cfg_dir_int(&d, "s_rotate", device->SCREEN.ROTATE);

    DEV_INT(device->SCREEN.ROTATE_PIVOT_X, "rotate_pivot_x");
    DEV_INT(device->SCREEN.ROTATE_PIVOT_Y, "rotate_pivot_y");
    DEV_INT(device->SCREEN.RENDER_OFFSET_X, "render_offset_x");
    DEV_INT(device->SCREEN.RENDER_OFFSET_Y, "render_offset_y");

    DEV_FLO(device->SCREEN.ZOOM, "zoom", 1.0);
    device->SCREEN.ZOOM = (float) cfg_dir_flo(&d, "s_zoom", device->SCREEN.ZOOM);

    device->SCREEN.ZOOM_WIDTH = device->SCREEN.ZOOM;
    device->SCREEN.ZOOM_HEIGHT = device->SCREEN.ZOOM;

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/internal");
    DEV_INT(device->SCREEN.INTERNAL.WIDTH, "width");
    DEV_INT(device->SCREEN.INTERNAL.HEIGHT, "height");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/external");
    DEV_INT(device->SCREEN.EXTERNAL.WIDTH, "width");
    DEV_INT(device->SCREEN.EXTERNAL.HEIGHT, "height");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "sdl");
    DEV_INT(device->SDL.SCALER, "scaler");
    DEV_INT(device->SDL.ROTATE, "rotate");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "battery");
    DEV_STR(device->BATTERY.CAPACITY, "capacity");
    DEV_STR(device->BATTERY.HEALTH, "health");
    DEV_STR(device->BATTERY.VOLTAGE, "voltage");
    DEV_STR(device->BATTERY.CHARGER, "charger");
    DEV_INT(device->BATTERY.VOLT_MIN, "volt_min");
    DEV_INT(device->BATTERY.VOLT_MAX, "volt_max");
    DEV_INT(device->BATTERY.SIZE, "size");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "battery/curve");
    DEV_STR(device->BATTERY.CURVE.CHARGE, "charge");
    DEV_STR(device->BATTERY.CURVE.DISCHARGE, "discharge");

#undef DEV_STR
#undef DEV_INT
#undef DEV_FLO

    if (!device->MUX.WIDTH) device->MUX.WIDTH = 640;
    if (!device->MUX.HEIGHT) device->MUX.HEIGHT = 480;
    if (!device->SCREEN.WIDTH) device->SCREEN.WIDTH = 640;
    if (!device->SCREEN.HEIGHT) device->SCREEN.HEIGHT = 480;

    snprintf(mux_dim, sizeof(mux_dim), "%dx%d/", device->MUX.WIDTH, device->MUX.HEIGHT);
}
