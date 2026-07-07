#include <stdio.h>
#include "config.h"
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
        (field) = (int16_t) cfg_dir_int(&d, (name), 0);                                                                \
    } while (0)

#define DEV_FLO(field, name, fallback)                                                                                 \
    do {                                                                                                               \
        (field) = (float) cfg_dir_flo(&d, (name), (fallback));                                                         \
    } while (0)

    cfg_dir_scan(&d, CONF_DEVICE_PATH "board");
    DEV_STR(device->board.name, "name");
    DEV_INT(device->board.has_network, "network");
    DEV_INT(device->board.has_bluetooth, "bluetooth");
    DEV_INT(device->board.has_portmaster, "portmaster");
    DEV_INT(device->board.has_lid, "lid");
    DEV_INT(device->board.has_hdmi, "hdmi");
    DEV_INT(device->board.has_event, "event");
    DEV_INT(device->board.has_debugfs, "debugfs");
    DEV_INT(device->board.has_stick, "stick");
    DEV_INT(device->board.has_touch, "touch");
    DEV_STR(device->board.sdl_map, "sdl_map");
    DEV_STR(device->board.joy_hall, "hall");
    DEV_STR(device->board.led, "led");
    DEV_STR(device->board.rtc_clock, "rtc_clock");
    DEV_STR(device->board.rtc_wake, "rtc_wake");
    DEV_STR(device->board.rumble, "rumble");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "led");
    DEV_INT(device->board.has_rgb, "rgb");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "audio");
    DEV_INT(device->audio.min, "min");
    DEV_INT(device->audio.max, "max");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "mux");
    DEV_INT(device->mux.width, "width");
    DEV_INT(device->mux.height, "height");
    DEV_INT(device->mux.buffer, "buffer");

#define DEV_MNT(field, subdir)                                                                                         \
    do {                                                                                                               \
        cfg_dir_scan(&d, CONF_DEVICE_PATH "storage/" subdir);                                                          \
        DEV_INT(device->storage.field.partition, "num");                                                               \
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
    DEV_STR(device->cpu.dflt, "default");
    DEV_STR(device->cpu.available, "available");
    DEV_STR(device->cpu.governor, "governor");
    DEV_STR(device->cpu.scaler, "scaler");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "network");
    DEV_STR(device->network.module, "module");
    DEV_STR(device->network.name, "name");
    DEV_STR(device->network.type, "type");
    DEV_STR(device->network.interface, "iface");
    DEV_STR(device->network.state, "state");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen");
    DEV_INT(device->screen.bright, "bright");
    DEV_INT(device->screen.wait, "wait");
    DEV_STR(device->screen.device, "device");
    DEV_STR(device->screen.hdmi, "hdmi");
    DEV_INT(device->screen.width, "width");
    DEV_INT(device->screen.height, "height");

    DEV_INT(device->screen.rotate, "rotate");
    device->screen.rotate = (int16_t) cfg_dir_int(&d, "s_rotate", device->screen.rotate);

    DEV_INT(device->screen.rotate_pivot_x, "rotate_pivot_x");
    DEV_INT(device->screen.rotate_pivot_y, "rotate_pivot_y");
    DEV_INT(device->screen.render_offset_x, "render_offset_x");
    DEV_INT(device->screen.render_offset_y, "render_offset_y");

    DEV_FLO(device->screen.zoom, "zoom", 1.0);
    device->screen.zoom = (float) cfg_dir_flo(&d, "s_zoom", device->screen.zoom);

    device->screen.zoom_width = device->screen.zoom;
    device->screen.zoom_height = device->screen.zoom;

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/internal");
    DEV_INT(device->screen.internal.width, "width");
    DEV_INT(device->screen.internal.height, "height");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "screen/external");
    DEV_INT(device->screen.external.width, "width");
    DEV_INT(device->screen.external.height, "height");

    cfg_dir_scan(&d, CONF_DEVICE_PATH "sdl");
    DEV_INT(device->sdl.scaler, "scaler");
    DEV_INT(device->sdl.rotate, "rotate");

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
