#include <stdio.h>
#include "config.h"
#include "config_value.h"
#include "device.h"
#include "options.h"

extern char mux_dim[15];

void load_device(struct mux_device *device) {
    cfg_dir_t d;

#define DEV_STR(field, name)                                                                                           \
    do {                                                                                                               \
        const char *_v = cfg_dir_get(&d, (name));                                                                      \
        snprintf((field), sizeof(field), "%s", (_v && *_v) ? _v : "");                                                 \
    } while (0)

#define DEV_INT(field, name)                                                                                           \
    do {                                                                                                               \
        (field) = config_i16_value(cfg_dir_get(&d, (name)), 0, 0, 0, 0);                                               \
    } while (0)

#define DEV_INT_RANGE(field, name, fallback, minimum, maximum)                                                         \
    do {                                                                                                               \
        (field) = config_i16_value(cfg_dir_get(&d, (name)), (fallback), 1, (minimum), (maximum));                      \
    } while (0)

#define DEV_FLO(field, name, fallback)                                                                                 \
    do {                                                                                                               \
        (field) = (float) config_float_value(cfg_dir_get(&d, (name)), (fallback), 1, 0.01, 64.0);                      \
    } while (0)

    cfg_dir_scan(&d, CONF_DEVICE_PATH "board");
    DEV_STR(device->board.name, "name");
    DEV_INT_RANGE(device->board.has_network, "network", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_bluetooth, "bluetooth", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_portmaster, "portmaster", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_lid, "lid", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_hdmi, "hdmi", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_event, "event", 0, 0, 32);
    DEV_INT_RANGE(device->board.has_debugfs, "debugfs", 0, 0, 1);
    DEV_INT_RANGE(device->board.has_stick, "stick", 0, 0, 4);
    DEV_INT_RANGE(device->board.has_touch, "touch", 0, 0, 1);
    DEV_STR(device->board.sdl_map, "sdl_map");
    DEV_STR(device->board.joy_hall, "hall");
    DEV_STR(device->board.led, "led");
    DEV_STR(device->board.rtc_clock, "rtc_clock");
    DEV_STR(device->board.rtc_wake, "rtc_wake");
    DEV_STR(device->board.rumble, "rumble");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "led");
    DEV_INT_RANGE(device->board.has_rgb, "rgb", 0, 0, 1);

    cfg_dir_scan(&d, CONF_DEVICE_PATH "audio");
    DEV_INT_RANGE(device->audio.min, "min", 0, 0, 100);
    DEV_INT_RANGE(device->audio.max, "max", 100, 0, 100);

    cfg_dir_scan(&d, CONF_DEVICE_PATH "mux");
    DEV_INT_RANGE(device->mux.width, "width", 0, 1, 16384);
    DEV_INT_RANGE(device->mux.height, "height", 0, 1, 16384);
    DEV_INT_RANGE(device->mux.buffer, "buffer", 0, 0, 16384);

#define DEV_MNT(field, subdir)                                                                                         \
    do {                                                                                                               \
        cfg_dir_scan(&d, CONF_DEVICE_PATH "storage/" subdir);                                                          \
        DEV_INT_RANGE(device->storage.field.partition, "num", 0, 0, 127);                                              \
        DEV_STR(device->storage.field.device, "dev");                                                                  \
        DEV_STR(device->storage.field.separator, "sep");                                                               \
        DEV_STR(device->storage.field.mount, "mount");                                                                 \
        DEV_STR(device->storage.field.type, "type");                                                                   \
        DEV_STR(device->storage.field.label, "label");                                                                 \
    } while (0)

    DEV_MNT(boot, "boot");
    DEV_MNT(rom, "rom");
    DEV_MNT(root, "root");
    DEV_MNT(sdcard, "sdcard");
    DEV_MNT(usb, "usb");

#undef DEV_MNT

    cfg_dir_scan(&d, CONF_DEVICE_PATH "cpu");
    DEV_INT_RANGE(device->cpu.cores, "cores", 1, 1, INT16_MAX);
    DEV_STR(device->cpu.dflt, "default");
    DEV_STR(device->cpu.available, "available");
    DEV_STR(device->cpu.governor, "governor");
    DEV_STR(device->cpu.scaler, "scaler");
    DEV_STR(device->cpu.max_freq, "max_freq");
    DEV_STR(device->cpu.max_freq_default, "max_freq_default");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "gpu");
    DEV_STR(device->gpu.max_freq_default, "max_freq_default");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "network");
    DEV_STR(device->network.module, "module");
    DEV_STR(device->network.name, "name");
    DEV_STR(device->network.type, "type");
    DEV_STR(device->network.interface, "iface");
    DEV_STR(device->network.state, "state");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen");
    DEV_INT_RANGE(device->screen.bright, "bright", 0, 1, INT16_MAX);
    DEV_INT_RANGE(device->screen.wait, "wait", 0, 0, INT16_MAX);
    DEV_STR(device->screen.device, "device");
    DEV_STR(device->screen.hdmi, "hdmi");
    DEV_INT_RANGE(device->screen.width, "width", 0, 1, 16384);
    DEV_INT_RANGE(device->screen.height, "height", 0, 1, 16384);

    DEV_INT_RANGE(device->screen.rotate, "rotate", 0, 0, 359);
    device->screen.rotate = config_i16_value(cfg_dir_get(&d, "s_rotate"), device->screen.rotate, 1, 0, 359);

    DEV_INT(device->screen.rotate_pivot_x, "rotate_pivot_x");
    DEV_INT(device->screen.rotate_pivot_y, "rotate_pivot_y");
    DEV_INT(device->screen.render_offset_x, "render_offset_x");
    DEV_INT(device->screen.render_offset_y, "render_offset_y");

    DEV_FLO(device->screen.zoom, "zoom", 1.0);
    device->screen.zoom = (float) config_float_value(cfg_dir_get(&d, "s_zoom"), device->screen.zoom, 1, 0.01, 64.0);

    device->screen.zoom_width = device->screen.zoom;
    device->screen.zoom_height = device->screen.zoom;

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/internal");
    DEV_INT_RANGE(device->screen.internal.width, "width", 0, 0, 16384);
    DEV_INT_RANGE(device->screen.internal.height, "height", 0, 0, 16384);

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/external");
    DEV_INT_RANGE(device->screen.external.width, "width", 0, 0, 16384);
    DEV_INT_RANGE(device->screen.external.height, "height", 0, 0, 16384);

    cfg_dir_scan(&d, CONF_DEVICE_PATH "sdl");
    DEV_INT_RANGE(device->sdl.scaler, "scaler", 0, 0, 1);
    DEV_INT_RANGE(device->sdl.rotate, "rotate", 0, 0, 3);

    cfg_dir_scan(&d, CONF_DEVICE_PATH "colour");
    DEV_INT(device->colour.red, "red");
    DEV_INT(device->colour.green, "green");
    DEV_INT(device->colour.blue, "blue");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "battery");
    DEV_STR(device->battery.capacity, "capacity");
    DEV_STR(device->battery.health, "health");
    DEV_STR(device->battery.voltage, "voltage");
    DEV_STR(device->battery.charger, "charger");
    DEV_INT(device->battery.volt_min, "volt_min");
    DEV_INT(device->battery.volt_max, "volt_max");
    DEV_INT(device->battery.size, "size");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "battery/curve");
    DEV_STR(device->battery.curve.charge, "charge");
    DEV_STR(device->battery.curve.discharge, "discharge");

#undef DEV_STR
#undef DEV_INT
#undef DEV_INT_RANGE
#undef DEV_FLO

    if (!device->mux.width) device->mux.width = 640;
    if (!device->mux.height) device->mux.height = 480;
    if (!device->mux.buffer) {
        int lines = 1024 * 1024 / (device->mux.width * 4);
        if (lines < 10) lines = 10;
        if (lines > device->mux.height) lines = device->mux.height;
        device->mux.buffer = (int16_t) lines;
    }
    if (!device->screen.width) device->screen.width = 640;
    if (!device->screen.height) device->screen.height = 480;

    snprintf(mux_dim, sizeof(mux_dim), "%dx%d/", device->mux.width, device->mux.height);
}
