#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "options.h"

typedef struct {
    const char *dir;
    const char *key;
    size_t offset;
    int is_str; // 0 = int16_t, 1 = char
    union {
        int16_t i;
        const char *s;
    } fallback;
} cfg_field;

#define CFG_OFF(member) offsetof(struct mux_config, member)

// clang-format off
static const cfg_field cfg_fields[] = {
    // system/
    {CONF_CONFIG_PATH "system", "build", CFG_OFF(system.build), 1, {.s = "Unknown"}},
    {CONF_CONFIG_PATH "system", "version", CFG_OFF(system.version), 1, {.s = "Edge"}},
    {CONF_CONFIG_PATH "system", "debug_mode", CFG_OFF(settings.advanced.debug_log), 0, {.i = 0}},

    // backup/
    {CONF_CONFIG_PATH "backup", "application", CFG_OFF(backup.apps), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "bios", CFG_OFF(backup.bios), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "catalogue", CFG_OFF(backup.catalogue), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "cheats", CFG_OFF(backup.cheats), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "collection", CFG_OFF(backup.collection), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "config", CFG_OFF(backup.config), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "content", CFG_OFF(backup.content), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "history", CFG_OFF(backup.history), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "init", CFG_OFF(backup.init), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "merge", CFG_OFF(backup.merge), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "music", CFG_OFF(backup.music), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "name", CFG_OFF(backup.name), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "network", CFG_OFF(backup.network), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "overlays", CFG_OFF(backup.overlays), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "override", CFG_OFF(backup.override), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "package", CFG_OFF(backup.package), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "retro", CFG_OFF(backup.retro), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "save", CFG_OFF(backup.save), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "screenshot", CFG_OFF(backup.screenshot), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "shaders", CFG_OFF(backup.shaders), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "syncthing", CFG_OFF(backup.syncthing), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "theme", CFG_OFF(backup.theme), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "track", CFG_OFF(backup.track), 0, {.i = 1}},
    {CONF_CONFIG_PATH "backup", "start", CFG_OFF(backup.start), 0, {.i = 0}},
    {CONF_CONFIG_PATH "backup", "target", CFG_OFF(backup.target), 0, {.i = 0}},

    // boot/
    {CONF_CONFIG_PATH "boot", "factory_reset", CFG_OFF(boot.factory_reset), 0, {.i = 0}},
    {CONF_CONFIG_PATH "boot", "device_mode", CFG_OFF(boot.device_mode), 0, {.i = 0}},

    // clock/
    {CONF_CONFIG_PATH "clock", "notation", CFG_OFF(clock.notation), 0, {.i = 0}},
    {CONF_CONFIG_PATH "clock", "pool", CFG_OFF(clock.pool), 1, {.s = "pool.ntp.org"}},
    {CONF_CONFIG_PATH "clock", "custom", CFG_OFF(clock.custom), 1, {.s = "%H%P %Z"}},

    // network/
    {CONF_CONFIG_PATH "network", "type", CFG_OFF(network.type), 0, {.i = 0}},
    {CONF_CONFIG_PATH "network", "interface", CFG_OFF(network.interface), 1, {.s = "wlan0"}},
    {CONF_CONFIG_PATH "network", "ssid", CFG_OFF(network.ssid), 1, {.s = ""}},
    {CONF_CONFIG_PATH "network", "pass", CFG_OFF(network.pass), 1, {.s = ""}},
    {CONF_CONFIG_PATH "network", "address", CFG_OFF(network.address), 1, {.s = "192.168.0.123"}},
    {CONF_CONFIG_PATH "network", "gateway", CFG_OFF(network.gateway), 1, {.s = "192.168.0.1"}},
    {CONF_CONFIG_PATH "network", "subnet", CFG_OFF(network.subnet), 1, {.s = "24"}},
    {CONF_CONFIG_PATH "network", "dns", CFG_OFF(network.dns), 1, {.s = "1.1.1.1"}},

    // theme/
    {CONF_CONFIG_PATH "theme", "active", CFG_OFF(theme.active), 1, {.s = "MustardOS"}},

    // theme/filter/
    {CONF_CONFIG_PATH "theme/filter", "allthemes", CFG_OFF(theme.filter.all_themes), 0, {.i = 0}},
    {CONF_CONFIG_PATH "theme/filter", "grid", CFG_OFF(theme.filter.grid), 0, {.i = 0}},
    {CONF_CONFIG_PATH "theme/filter", "hdmi", CFG_OFF(theme.filter.hdmi), 0, {.i = 0}},
    {CONF_CONFIG_PATH "theme/filter", "language", CFG_OFF(theme.filter.language), 0, {.i = 0}},
    {CONF_CONFIG_PATH "theme/filter", "lookup", CFG_OFF(theme.filter.lookup), 1, {.s = ""}},

    // theme/download/
    {CONF_CONFIG_PATH "theme/download", "data", CFG_OFF(theme.download.data), 1, {.s = ""}},
    {CONF_CONFIG_PATH "theme/download", "preview", CFG_OFF(theme.download.preview), 1, {.s = ""}},

    // extra/download/
    {CONF_CONFIG_PATH "extra/download", "data", CFG_OFF(extra.download.data), 1, {.s = ""}},

    // extra/language/
    {CONF_CONFIG_PATH "extra/language", "data", CFG_OFF(extra.language.data), 1, {.s = ""}},

    // settings/advanced/
    {CONF_CONFIG_PATH "settings/advanced", "accelerate", CFG_OFF(settings.advanced.accelerate), 0, {.i = 96}},
    {CONF_CONFIG_PATH "settings/advanced", "repeat_delay", CFG_OFF(settings.advanced.repeat_delay), 0, {.i = 208}},
    {CONF_CONFIG_PATH "settings/advanced", "sticknav", CFG_OFF(settings.advanced.stick_nav), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "thermal", CFG_OFF(settings.advanced.thermal), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "font", CFG_OFF(settings.advanced.font), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "led", CFG_OFF(settings.advanced.led), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "random_theme", CFG_OFF(settings.advanced.random_theme), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "retrowait", CFG_OFF(settings.advanced.retro_wait), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "retrofree", CFG_OFF(settings.advanced.retro_free), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "retrocache", CFG_OFF(settings.advanced.retro_cache), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "activity", CFG_OFF(settings.advanced.activity), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "usb_function", CFG_OFF(settings.advanced.usb_function), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "verbose", CFG_OFF(settings.advanced.verbose), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "rumble", CFG_OFF(settings.advanced.rumble), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "volume", CFG_OFF(settings.advanced.volume), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "brightness", CFG_OFF(settings.advanced.brightness), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "user_init", CFG_OFF(settings.advanced.user_init), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "dpad_swap", CFG_OFF(settings.advanced.dpad_swap), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "overdrive", CFG_OFF(settings.advanced.overdrive), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "swapfile", CFG_OFF(settings.advanced.swapfile), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "zramfile", CFG_OFF(settings.advanced.zramfile), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "lidswitch", CFG_OFF(settings.advanced.lid_switch), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "disp_suspend", CFG_OFF(settings.advanced.disp_suspend), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "stage_overlay", CFG_OFF(settings.advanced.stage_overlay), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "incbright", CFG_OFF(settings.advanced.inc_bright), 0, {.i = 16}},
    {CONF_CONFIG_PATH "settings/advanced", "incvolume", CFG_OFF(settings.advanced.inc_volume), 0, {.i = 8}},
    {CONF_CONFIG_PATH "settings/advanced", "maxgpu", CFG_OFF(settings.advanced.max_gpu), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "double_buffer", CFG_OFF(settings.advanced.double_buffer), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "audio_ready", CFG_OFF(settings.advanced.audio_ready), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "audio_swap", CFG_OFF(settings.advanced.audio_swap), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "audio_suspend", CFG_OFF(settings.advanced.audio_suspend), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "bt_scan_timeout", CFG_OFF(settings.advanced.bt_scan_timeout), 0, {.i = 20}},
    {CONF_CONFIG_PATH "settings/advanced", "part_external", CFG_OFF(settings.advanced.usb_part), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "part_secondary", CFG_OFF(settings.advanced.second_part), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "trust_modify", CFG_OFF(settings.advanced.trust_modify), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "trust_power", CFG_OFF(settings.advanced.trust_power), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/advanced", "trust_remove", CFG_OFF(settings.advanced.trust_remove), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/advanced", "boxartpaddiv", CFG_OFF(settings.advanced.box_art_pad_div), 0, {.i = 3}},

    // settings/colour/
    {CONF_CONFIG_PATH "settings/colour", "schedule_mode", CFG_OFF(settings.colour.schedule_mode), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/colour", "sunrise_temp", CFG_OFF(settings.colour.sunrise_temp), 0, {.i = DEFAULT_TEMPERATURE}},
    {CONF_CONFIG_PATH "settings/colour", "sunset_temp", CFG_OFF(settings.colour.sunset_temp), 0, {.i = DEFAULT_TEMPERATURE}},
    {CONF_CONFIG_PATH "settings/colour", "sunrise_time", CFG_OFF(settings.colour.sunrise_time), 0, {.i = 24}},
    {CONF_CONFIG_PATH "settings/colour", "sunset_time", CFG_OFF(settings.colour.sunset_time), 0, {.i = 72}},

    // settings/general/
    {CONF_CONFIG_PATH "settings/general", "sound", CFG_OFF(settings.general.sound), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "soundvol", CFG_OFF(settings.general.soundvol), 0, {.i = 100}},
    {CONF_CONFIG_PATH "settings/general", "chime", CFG_OFF(settings.general.chime), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "bgm", CFG_OFF(settings.general.bgm), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "bgmvol", CFG_OFF(settings.general.bgmvol), 0, {.i = 35}},
    {CONF_CONFIG_PATH "settings/general", "brightness", CFG_OFF(settings.general.brightness), 0, {.i = 90}},
    {CONF_CONFIG_PATH "settings/general", "volume", CFG_OFF(settings.general.volume), 0, {.i = 75}},
    {CONF_CONFIG_PATH "settings/general", "rgb", CFG_OFF(settings.general.rgb), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "audiosink", CFG_OFF(settings.general.audiosink), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "theme_resolution", CFG_OFF(settings.general.theme_resolution), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/general", "theme_scaling", CFG_OFF(settings.general.theme_scaling), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/general", "startup", CFG_OFF(settings.general.startup), 1, {.s = "launcher"}},
    {CONF_CONFIG_PATH "settings/general", "language", CFG_OFF(settings.general.language), 1, {.s = "English"}},

    // settings/hotkey/
    {CONF_CONFIG_PATH "settings/hotkey", "dpad_toggle", CFG_OFF(settings.general.hkdpad), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/hotkey", "screenshot", CFG_OFF(settings.general.hkshot), 0, {.i = 0}},

    // settings/hdmi/
    {CONF_CONFIG_PATH "settings/hdmi", "resolution", CFG_OFF(settings.hdmi.resolution), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/hdmi", "space", CFG_OFF(settings.hdmi.space), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/hdmi", "depth", CFG_OFF(settings.hdmi.depth), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/hdmi", "range", CFG_OFF(settings.hdmi.range), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/hdmi", "scan", CFG_OFF(settings.hdmi.scan), 0, {.i = 0}},

    // settings/network/
    {CONF_CONFIG_PATH "settings/network", "monitor", CFG_OFF(settings.network.monitor), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/network", "boot", CFG_OFF(settings.network.boot), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/network", "wake", CFG_OFF(settings.network.wake), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/network", "compat", CFG_OFF(settings.network.compat), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/network", "async_load", CFG_OFF(settings.network.async_load), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/network", "con_retry", CFG_OFF(settings.network.con_retry), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/network", "wait_timer", CFG_OFF(settings.network.wait), 0, {.i = 5}},
    {CONF_CONFIG_PATH "settings/network", "mod_retry", CFG_OFF(settings.network.mod_retry), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/network", "proxy_enabled", CFG_OFF(settings.network.proxy_enabled), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/network", "proxy_type", CFG_OFF(settings.network.proxy_type), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/network", "proxy_server", CFG_OFF(settings.network.proxy_server), 1, {.s = ""}},
    {CONF_CONFIG_PATH "settings/network", "proxy_noproxy", CFG_OFF(settings.network.proxy_noproxy), 1, {.s = "localhost,127.0.0.1,::1"}},
    {CONF_CONFIG_PATH "settings/network", "system_dns", CFG_OFF(settings.network.system_dns), 0, {.i = 0}},

    // settings/overlay/
    {CONF_CONFIG_PATH "settings/overlay", "gen_alpha", CFG_OFF(settings.overlay.gen_alpha), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/overlay", "gen_anchor", CFG_OFF(settings.overlay.gen_anchor), 0, {.i = 4}},
    {CONF_CONFIG_PATH "settings/overlay", "gen_scale", CFG_OFF(settings.overlay.gen_scale), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/overlay", "bat_alpha", CFG_OFF(settings.overlay.bat_alpha), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/overlay", "bat_anchor", CFG_OFF(settings.overlay.bat_anchor), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/overlay", "bat_scale", CFG_OFF(settings.overlay.bat_scale), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/overlay", "vol_alpha", CFG_OFF(settings.overlay.vol_alpha), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/overlay", "vol_anchor", CFG_OFF(settings.overlay.vol_anchor), 0, {.i = 2}},
    {CONF_CONFIG_PATH "settings/overlay", "vol_scale", CFG_OFF(settings.overlay.vol_scale), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/overlay", "bri_alpha", CFG_OFF(settings.overlay.bri_alpha), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/overlay", "bri_anchor", CFG_OFF(settings.overlay.bri_anchor), 0, {.i = 2}},
    {CONF_CONFIG_PATH "settings/overlay", "bri_scale", CFG_OFF(settings.overlay.bri_scale), 0, {.i = 0}},

    // settings/power/
    {CONF_CONFIG_PATH "settings/power", "low_battery", CFG_OFF(settings.power.low_battery), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "shutdown", CFG_OFF(settings.power.shutdown), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "idle_display", CFG_OFF(settings.power.idle.display), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "idle_sleep", CFG_OFF(settings.power.idle.sleep), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "idle_mute", CFG_OFF(settings.power.idle.mute), 0, {.i = 1}},
    {CONF_CONFIG_PATH "settings/power", "saver_type", CFG_OFF(settings.power.saver_type), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "saver_speed", CFG_OFF(settings.power.saver_speed), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/power", "gov_idle", CFG_OFF(settings.power.gov.idle), 1, {.s = "powersave"}},

    // cpu/ (SETTINGS.power.gov.dflt lives in the device config, not user config)
    {CONF_DEVICE_PATH "cpu", "default", CFG_OFF(settings.power.gov.dflt), 1, {.s = "ondemand"}},

    // settings/rgb/
    {CONF_CONFIG_PATH "settings/rgb", "mode", CFG_OFF(settings.rgb.mode), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_theme", CFG_OFF(settings.rgb.bright_theme), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_l", CFG_OFF(settings.rgb.colour_l), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_l", CFG_OFF(settings.rgb.bright_l), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_r", CFG_OFF(settings.rgb.colour_r), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_r", CFG_OFF(settings.rgb.bright_r), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_m", CFG_OFF(settings.rgb.colour_m), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_m", CFG_OFF(settings.rgb.bright_m), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_f1", CFG_OFF(settings.rgb.colour_f1), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_f1", CFG_OFF(settings.rgb.bright_f1), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_f2", CFG_OFF(settings.rgb.colour_f2), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_f2", CFG_OFF(settings.rgb.bright_f2), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_rs1", CFG_OFF(settings.rgb.colour_rs1), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_rs1", CFG_OFF(settings.rgb.bright_rs1), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_rs2", CFG_OFF(settings.rgb.colour_rs2), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_rs2", CFG_OFF(settings.rgb.bright_rs2), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_shl1", CFG_OFF(settings.rgb.colour_shl1), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_shl1", CFG_OFF(settings.rgb.bright_shl1), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_shl2", CFG_OFF(settings.rgb.colour_shl2), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_shl2", CFG_OFF(settings.rgb.bright_shl2), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_shr2", CFG_OFF(settings.rgb.colour_shr2), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_shr2", CFG_OFF(settings.rgb.bright_shr2), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "colour_shr1", CFG_OFF(settings.rgb.colour_shr1), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/rgb", "bright_shr1", CFG_OFF(settings.rgb.bright_shr1), 0, {.i = 255}},
    {CONF_CONFIG_PATH "settings/rgb", "backend", CFG_OFF(settings.rgb.backend), 0, {.i = 0}},

    // settings/font/
    {CONF_CONFIG_PATH "settings/font", "list_size", CFG_OFF(settings.font.list_size), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/font", "header_size", CFG_OFF(settings.font.header_size), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/font", "footer_size", CFG_OFF(settings.font.footer_size), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/font", "panel_size", CFG_OFF(settings.font.panel_size), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/font", "name", CFG_OFF(settings.font.name), 1, {.s = ""}},

    // settings/theme/ (theme option overrides - distinct from theme/filter/)
    {CONF_CONFIG_PATH "settings/theme", "header_height", CFG_OFF(settings.themeopt.header_height), 0, {.i = -1}},
    {CONF_CONFIG_PATH "settings/theme", "footer_height", CFG_OFF(settings.themeopt.footer_height), 0, {.i = -1}},
    {CONF_CONFIG_PATH "settings/theme", "content_item_count", CFG_OFF(settings.themeopt.content_item_count), 0, {.i = 0}},
    {CONF_CONFIG_PATH "settings/theme", "glyph_size_list", CFG_OFF(settings.themeopt.glyph_size_list), 0, {.i = -2}},
    {CONF_CONFIG_PATH "settings/theme", "glyph_size_footer", CFG_OFF(settings.themeopt.glyph_size_footer), 0, {.i = -2}},
    {CONF_CONFIG_PATH "settings/theme", "glyph_size_header", CFG_OFF(settings.themeopt.glyph_size_header), 0, {.i = -2}},
    {CONF_CONFIG_PATH "settings/theme", "glyph_size_grid", CFG_OFF(settings.themeopt.glyph_size_grid), 0, {.i = -2}},
    {CONF_CONFIG_PATH "settings/theme", "label_width", CFG_OFF(settings.themeopt.label_width), 0, {.i = 0}},

    // settings/remap/
    {CONF_CONFIG_PATH "settings/remap", "layout", CFG_OFF(settings.remap.layout), 0, {.i = 0}},

    // sort/
    {CONF_CONFIG_PATH "sort", "default", CFG_OFF(sort.dflt), 0, {.i = 0}},
    {CONF_CONFIG_PATH "sort", "collection", CFG_OFF(sort.collection), 0, {.i = 0}},
    {CONF_CONFIG_PATH "sort", "history", CFG_OFF(sort.history), 0, {.i = 0}},

    // visual/
    {CONF_CONFIG_PATH "visual", "battery", CFG_OFF(visual.battery), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "network", CFG_OFF(visual.network), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "headertitle", CFG_OFF(visual.header_title), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "dialoguetransition", CFG_OFF(visual.dialogue_transition), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "bluetooth", CFG_OFF(visual.bluetooth), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "clock", CFG_OFF(visual.clock), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "overlayimage", CFG_OFF(visual.overlay_image), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "overlaytransparency", CFG_OFF(visual.overlay_transparency), 0, {.i = 85}},
    {CONF_CONFIG_PATH "visual", "gridmodecontent", CFG_OFF(visual.grid_mode_content), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "boxart", CFG_OFF(visual.box_art), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "boxartalign", CFG_OFF(visual.box_art_align), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "boxarthide", CFG_OFF(visual.box_art_hide), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "boxartscale", CFG_OFF(visual.box_art_scale), 0, {.i = 100}},
    {CONF_CONFIG_PATH "visual", "boxartpadding", CFG_OFF(visual.box_art_padding), 0, {.i = 15}},
    {CONF_CONFIG_PATH "visual", "boxartplaceholder", CFG_OFF(visual.box_art_placeholder), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "boxarttransition", CFG_OFF(visual.box_art_transition), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "videopreview", CFG_OFF(visual.video_preview), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "contentwidth", CFG_OFF(visual.content_width), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "name", CFG_OFF(visual.name), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "dash", CFG_OFF(visual.dash), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "launch_swap", CFG_OFF(visual.launch_swap), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "hidden", CFG_OFF(visual.hidden), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "shuffle", CFG_OFF(visual.shuffle), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "friendlyfolder", CFG_OFF(visual.friendly_folder), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "thetitleformat", CFG_OFF(visual.the_title_format), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "titleincluderootdrive", CFG_OFF(visual.title_include_root_drive), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "folderitemcount", CFG_OFF(visual.folder_item_count), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "folderempty", CFG_OFF(visual.display_empty_folder), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "counterfolder", CFG_OFF(visual.menu_counter_folder), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "counterfile", CFG_OFF(visual.menu_counter_file), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "video_wallpaper", CFG_OFF(visual.video_wallpaper), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "background_scale", CFG_OFF(visual.background_scale), 0, {.i = 2}},
    {CONF_CONFIG_PATH "visual", "launchsplash", CFG_OFF(visual.launchsplash), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "blackfade", CFG_OFF(visual.blackfade), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "contentcollect", CFG_OFF(visual.content_collect), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "contenthistory", CFG_OFF(visual.content_history), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "mixedcontent", CFG_OFF(visual.mixed_content), 0, {.i = 0}},
    {CONF_CONFIG_PATH "visual", "forwardhistory", CFG_OFF(visual.forward_history), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "namescroll", CFG_OFF(visual.name_scroll), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "labelscrollspeed", CFG_OFF(visual.label_scroll_speed), 0, {.i = 2}},
    {CONF_CONFIG_PATH "visual", "listglyph", CFG_OFF(visual.list_glyph), 0, {.i = 1}},
    {CONF_CONFIG_PATH "visual", "selectionanimation", CFG_OFF(visual.selection_animation), 0, {.i = 2}},
    {CONF_CONFIG_PATH "visual", "selectionstyle", CFG_OFF(visual.selection_style), 0, {.i = 4}},
    {CONF_CONFIG_PATH "visual", "shadow", CFG_OFF(visual.render_shadows), 0, {.i = 1}},

    // bluetooth/
    {CONF_CONFIG_PATH "bluetooth", "autoconnect", CFG_OFF(bluetooth.auto_connect), 0, {.i = 0}},

    // web/
    {CONF_CONFIG_PATH "web", "sshd", CFG_OFF(web.sshd), 0, {.i = 0}},
    {CONF_CONFIG_PATH "web", "sftpgo", CFG_OFF(web.sftp_go), 0, {.i = 0}},
    {CONF_CONFIG_PATH "web", "ttyd", CFG_OFF(web.ttyd), 0, {.i = 0}},
    {CONF_CONFIG_PATH "web", "syncthing", CFG_OFF(web.syncthing), 0, {.i = 0}},
    {CONF_CONFIG_PATH "web", "tailscaled", CFG_OFF(web.tailscaled), 0, {.i = 0}},

    // danger/
    {CONF_CONFIG_PATH "danger", "vmswap", CFG_OFF(danger.vm_swap), 0, {.i = 8}},
    {CONF_CONFIG_PATH "danger", "dirty_ratio", CFG_OFF(danger.dirty_ratio), 0, {.i = 16}},
    {CONF_CONFIG_PATH "danger", "dirty_back_ratio", CFG_OFF(danger.dirty_back), 0, {.i = 4}},
    {CONF_CONFIG_PATH "danger", "cache_pressure", CFG_OFF(danger.cache), 0, {.i = 64}},
    {CONF_CONFIG_PATH "danger", "nomerges", CFG_OFF(danger.merge), 0, {.i = 0}},
    {CONF_CONFIG_PATH "danger", "nr_requests", CFG_OFF(danger.requests), 0, {.i = 128}},
    {CONF_CONFIG_PATH "danger", "read_ahead", CFG_OFF(danger.read_ahead), 0, {.i = 4096}},
    {CONF_CONFIG_PATH "danger", "page_cluster", CFG_OFF(danger.page_cluster), 0, {.i = 3}},
    {CONF_CONFIG_PATH "danger", "time_slice", CFG_OFF(danger.time_slice), 0, {.i = 10}},
    {CONF_CONFIG_PATH "danger", "iostats", CFG_OFF(danger.io_stats), 0, {.i = 0}},
    {CONF_CONFIG_PATH "danger", "idle_flush", CFG_OFF(danger.idle_flush), 0, {.i = 0}},
    {CONF_CONFIG_PATH "danger", "child_first", CFG_OFF(danger.child_first), 0, {.i = 0}},
    {CONF_CONFIG_PATH "danger", "tune_scale", CFG_OFF(danger.tune_scale), 0, {.i = 1}},
    {CONF_CONFIG_PATH "danger", "cardmode", CFG_OFF(danger.card_mode), 1, {.s = "noop"}},
    {CONF_CONFIG_PATH "danger", "state", CFG_OFF(danger.state), 1, {.s = "mem"}},
};
// clang-format on

#undef CFG_OFF

void load_config(struct mux_config *config) {
    cfg_dir_t d;
    const char *cur_dir = NULL;

    for (size_t i = 0; i < sizeof(cfg_fields) / sizeof(cfg_fields[0]); i++) {
        const cfg_field *f = &cfg_fields[i];

        if (!cur_dir || strcmp(f->dir, cur_dir) != 0) {
            cfg_dir_scan(&d, f->dir);
            cur_dir = f->dir;
        }

        void *field_ptr = (char *) config + f->offset;
        if (f->is_str) {
            const char *v = cfg_dir_get(&d, f->key);
            snprintf((char *) field_ptr, MAX_BUFFER_SIZE, "%s", v && *v ? v : f->fallback.s);
        } else {
            *(int16_t *) field_ptr = (int16_t) cfg_dir_int(&d, f->key, f->fallback.i);
        }
    }

    // theme/ - storage_theme and theme_cat_path are built from the "active" theme name
    snprintf(
        config->theme.storage_theme, sizeof(config->theme.storage_theme), RUN_STORAGE_PATH "theme/%s",
        config->theme.active
    );
    snprintf(
        config->theme.theme_cat_path, sizeof(config->theme.theme_cat_path), "%s/catalogue", config->theme.storage_theme
    );

    // theme/filter/ - resolution flags are derived from the active device resolution
    if (!config->theme.filter.all_themes) {
        cfg_dir_scan(&d, CONF_DEVICE_PATH "mux");
        const int width = cfg_dir_int(&d, "width", 0);
        const int height = cfg_dir_int(&d, "height", 0);
        config->theme.filter.resolution_640_x480 = width == 640 && height == 480 ? 1 : 0;
        config->theme.filter.resolution_720_x480 = width == 720 && height == 480 ? 1 : 0;
        config->theme.filter.resolution_720_x720 = width == 720 && height == 720 ? 1 : 0;
        config->theme.filter.resolution_1024_x768 = width == 1024 && height == 768 ? 1 : 0;
        config->theme.filter.resolution_1280_x720 = width == 1280 && height == 720 ? 1 : 0;
        config->theme.filter.resolution_1920_x1080 = width == 1920 && height == 1080 ? 1 : 0;
    } else {
        config->theme.filter.resolution_640_x480 = 0;
        config->theme.filter.resolution_720_x480 = 0;
        config->theme.filter.resolution_720_x720 = 0;
        config->theme.filter.resolution_1024_x768 = 0;
        config->theme.filter.resolution_1280_x720 = 0;
        config->theme.filter.resolution_1920_x1080 = 0;
    }

    // Compute theme resolution dimensions
    config->settings.general.theme_resolution_width = 0;
    config->settings.general.theme_resolution_height = 0;
    switch (config->settings.general.theme_resolution) {
        case 1:
            config->settings.general.theme_resolution_width = 640;
            config->settings.general.theme_resolution_height = 480;
            break;
        case 2:
            config->settings.general.theme_resolution_width = 720;
            config->settings.general.theme_resolution_height = 480;
            break;
        case 3:
            config->settings.general.theme_resolution_width = 720;
            config->settings.general.theme_resolution_height = 576;
            break;
        case 4:
            config->settings.general.theme_resolution_width = 720;
            config->settings.general.theme_resolution_height = 720;
            break;
        case 5:
            config->settings.general.theme_resolution_width = 1024;
            config->settings.general.theme_resolution_height = 768;
            break;
        case 6:
            config->settings.general.theme_resolution_width = 1280;
            config->settings.general.theme_resolution_height = 720;
            break;
        case 7:
            config->settings.general.theme_resolution_width = 1920;
            config->settings.general.theme_resolution_height = 1080;
            break;
        default:
            break;
    }
}

void cfg_dir_scan(cfg_dir_t *d, const char *dir_path) {
    d->count = 0;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && d->count < CFG_DIR_MAX) {
        if (ent->d_name[0] == '.') continue;

        if (ent->d_type != DT_UNKNOWN && ent->d_type != DT_REG) continue;

        char path[512];
        const int n = snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        if (n <= 0 || (size_t) n >= sizeof(path)) continue;

        if (ent->d_type == DT_UNKNOWN) {
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        }

        FILE *f = fopen(path, "r");
        if (!f) continue;

        cfg_kv_t *kv = &d->entries[d->count];

        const size_t nlen = strnlen(ent->d_name, CFG_NAME_MAX - 1);
        memcpy(kv->name, ent->d_name, nlen);
        kv->name[nlen] = '\0';

        kv->value[0] = '\0';
        if (fgets(kv->value, sizeof(kv->value), f)) {
            size_t vlen = strlen(kv->value);
            while (vlen > 0 && (kv->value[vlen - 1] == '\n' || kv->value[vlen - 1] == '\r')) {
                kv->value[--vlen] = '\0';
            }
        }

        fclose(f);
        d->count++;
    }

    closedir(dir);
}

const char *cfg_dir_get(const cfg_dir_t *d, const char *name) {
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].name, name) == 0) return d->entries[i].value;
    }

    return NULL;
}

int cfg_dir_int(const cfg_dir_t *d, const char *name, const int fallback) {
    const char *v = cfg_dir_get(d, name);
    if (!v || !*v) return fallback;

    char *end;
    const long val = strtol(v, &end, 10);

    return end != v && *end == '\0' ? (int) val : fallback;
}

double cfg_dir_flo(const cfg_dir_t *d, const char *name, const double fallback) {
    const char *v = cfg_dir_get(d, name);
    if (!v || !*v) return fallback;

    char *end;
    const double val = strtod(v, &end);

    return end != v && *end == '\0' ? val : fallback;
}
