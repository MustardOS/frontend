#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "options.h"

#define CFG_STR(field, d, name, fallback)                                                                              \
    do {                                                                                                               \
        const char *_v = cfg_dir_get((d), (name));                                                                     \
        snprintf((field), sizeof(field), "%s", (_v && *_v) ? _v : (fallback));                                         \
    } while (0)

#define CFG_INT(field, d, name, fallback)                                                                              \
    do {                                                                                                               \
        (field) = (int16_t) cfg_dir_int((d), (name), (fallback));                                                      \
    } while (0)

void load_config(struct mux_config *config) {
    cfg_dir_t d;

    // system/ - build, version, debug_mode
    cfg_dir_scan(&d, CONF_CONFIG_PATH "system");
    CFG_STR(config->system.build, &d, "build", "Unknown");
    CFG_STR(config->system.version, &d, "version", "Edge");
    CFG_INT(config->settings.advanced.debug_log, &d, "debug_mode", 0);

    // backup/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "backup");
    CFG_INT(config->backup.apps, &d, "application", 1);
    CFG_INT(config->backup.bios, &d, "bios", 1);
    CFG_INT(config->backup.catalogue, &d, "catalogue", 1);
    CFG_INT(config->backup.cheats, &d, "cheats", 1);
    CFG_INT(config->backup.collection, &d, "collection", 1);
    CFG_INT(config->backup.config, &d, "config", 1);
    CFG_INT(config->backup.content, &d, "content", 1);
    CFG_INT(config->backup.history, &d, "history", 1);
    CFG_INT(config->backup.init, &d, "init", 1);
    CFG_INT(config->backup.merge, &d, "merge", 1);
    CFG_INT(config->backup.music, &d, "music", 1);
    CFG_INT(config->backup.name, &d, "name", 1);
    CFG_INT(config->backup.network, &d, "network", 1);
    CFG_INT(config->backup.overlays, &d, "overlays", 1);
    CFG_INT(config->backup.override, &d, "override", 1);
    CFG_INT(config->backup.package, &d, "package", 1);
    CFG_INT(config->backup.save, &d, "save", 1);
    CFG_INT(config->backup.screenshot, &d, "screenshot", 1);
    CFG_INT(config->backup.shaders, &d, "shaders", 1);
    CFG_INT(config->backup.syncthing, &d, "syncthing", 1);
    CFG_INT(config->backup.theme, &d, "theme", 1);
    CFG_INT(config->backup.track, &d, "track", 1);
    CFG_INT(config->backup.start, &d, "start", 0);
    CFG_INT(config->backup.target, &d, "target", 0);

    // boot/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "boot");
    CFG_INT(config->boot.factory_reset, &d, "factory_reset", 0);
    CFG_INT(config->boot.device_mode, &d, "device_mode", 0);

    // clock/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "clock");
    CFG_INT(config->clock.notation, &d, "notation", 0);
    CFG_STR(config->clock.pool, &d, "pool", "pool.ntp.org");
    CFG_STR(config->clock.custom, &d, "custom", "%H%P %Z");

    // network/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "network");
    CFG_INT(config->network.type, &d, "type", 0);
    CFG_STR(config->network.interface, &d, "interface", "wlan0");
    CFG_STR(config->network.ssid, &d, "ssid", "");
    CFG_STR(config->network.pass, &d, "pass", "");
    CFG_STR(config->network.address, &d, "address", "192.168.0.123");
    CFG_STR(config->network.gateway, &d, "gateway", "192.168.0.1");
    CFG_STR(config->network.subnet, &d, "subnet", "24");
    CFG_STR(config->network.dns, &d, "dns", "1.1.1.1");

    // theme/ (top level - contains the "active" file; subdirs handled separately)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme");
    CFG_STR(config->theme.active, &d, "active", "MustardOS");
    snprintf(
        config->theme.storage_theme, sizeof(config->theme.storage_theme), RUN_STORAGE_PATH "theme/%s",
        config->theme.active
    );
    snprintf(
        config->theme.theme_cat_path, sizeof(config->theme.theme_cat_path), "%s/catalogue", config->theme.storage_theme
    );

    // theme/filter/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme/filter");
    CFG_INT(config->theme.filter.all_themes, &d, "allthemes", 0);
    CFG_INT(config->theme.filter.grid, &d, "grid", 0);
    CFG_INT(config->theme.filter.hdmi, &d, "hdmi", 0);
    CFG_INT(config->theme.filter.language, &d, "language", 0);
    CFG_STR(config->theme.filter.lookup, &d, "lookup", "");

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

    // theme/download/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme/download");
    CFG_STR(config->theme.download.data, &d, "data", "");
    CFG_STR(config->theme.download.preview, &d, "preview", "");

    // extra/download/ and extra/language/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "extra/download");
    CFG_STR(config->extra.download.data, &d, "data", "");

    cfg_dir_scan(&d, CONF_CONFIG_PATH "extra/language");
    CFG_STR(config->extra.language.data, &d, "data", "");

    // settings/advanced/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/advanced");
    CFG_INT(config->settings.advanced.accelerate, &d, "accelerate", 96);
    CFG_INT(config->settings.advanced.repeat_delay, &d, "repeat_delay", 208);
    CFG_INT(config->settings.advanced.stick_nav, &d, "sticknav", 0);
    CFG_INT(config->settings.advanced.thermal, &d, "thermal", 1);
    CFG_INT(config->settings.advanced.font, &d, "font", 0);
    CFG_INT(config->settings.advanced.led, &d, "led", 0);
    CFG_INT(config->settings.advanced.random_theme, &d, "random_theme", 0);
    CFG_INT(config->settings.advanced.retro_wait, &d, "retrowait", 0);
    CFG_INT(config->settings.advanced.retro_free, &d, "retrofree", 0);
    CFG_INT(config->settings.advanced.retro_cache, &d, "retrocache", 0);
    CFG_INT(config->settings.advanced.activity, &d, "activity", 1);
    CFG_INT(config->settings.advanced.usb_function, &d, "usb_function", 0);
    CFG_INT(config->settings.advanced.verbose, &d, "verbose", 0);
    CFG_INT(config->settings.advanced.rumble, &d, "rumble", 0);
    CFG_INT(config->settings.advanced.volume, &d, "volume", 0);
    CFG_INT(config->settings.advanced.brightness, &d, "brightness", 0);
    CFG_INT(config->settings.advanced.user_init, &d, "user_init", 0);
    CFG_INT(config->settings.advanced.dpad_swap, &d, "dpad_swap", 1);
    CFG_INT(config->settings.advanced.overdrive, &d, "overdrive", 0);
    CFG_INT(config->settings.advanced.swapfile, &d, "swapfile", 0);
    CFG_INT(config->settings.advanced.zramfile, &d, "zramfile", 0);
    CFG_INT(config->settings.advanced.lid_switch, &d, "lidswitch", 1);
    CFG_INT(config->settings.advanced.disp_suspend, &d, "disp_suspend", 0);
    CFG_INT(config->settings.advanced.stage_overlay, &d, "stage_overlay", 1);
    CFG_INT(config->settings.advanced.inc_bright, &d, "incbright", 16);
    CFG_INT(config->settings.advanced.inc_volume, &d, "incvolume", 8);
    CFG_INT(config->settings.advanced.max_gpu, &d, "maxgpu", 0);
    CFG_INT(config->settings.advanced.double_buffer, &d, "double_buffer", 0);
    CFG_INT(config->settings.advanced.audio_ready, &d, "audio_ready", 0);
    CFG_INT(config->settings.advanced.audio_swap, &d, "audio_swap", 0);
    CFG_INT(config->settings.advanced.audio_suspend, &d, "audio_suspend", 1);
    CFG_INT(config->settings.advanced.bt_scan_timeout, &d, "bt_scan_timeout", 20);
    CFG_INT(config->settings.advanced.usb_part, &d, "part_external", 0);
    CFG_INT(config->settings.advanced.second_part, &d, "part_secondary", 0);
    CFG_INT(config->settings.advanced.trust_modify, &d, "trust_modify", 0);
    CFG_INT(config->settings.advanced.trust_power, &d, "trust_power", 1);
    CFG_INT(config->settings.advanced.trust_remove, &d, "trust_remove", 0);
    CFG_INT(config->settings.advanced.box_art_pad_div, &d, "boxartpaddiv", 3);

    // settings/colour/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/colour");
    CFG_INT(config->settings.colour.schedule_mode, &d, "schedule_mode", 0);
    CFG_INT(config->settings.colour.sunrise_temp, &d, "sunrise_temp", DEFAULT_TEMPERATURE);
    CFG_INT(config->settings.colour.sunset_temp, &d, "sunset_temp", DEFAULT_TEMPERATURE);
    CFG_INT(config->settings.colour.sunrise_time, &d, "sunrise_time", 24);
    CFG_INT(config->settings.colour.sunset_time, &d, "sunset_time", 72);

    // settings/general/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/general");
    CFG_INT(config->settings.general.sound, &d, "sound", 0);
    CFG_INT(config->settings.general.soundvol, &d, "soundvol", 100);
    CFG_INT(config->settings.general.chime, &d, "chime", 0);
    CFG_INT(config->settings.general.bgm, &d, "bgm", 0);
    CFG_INT(config->settings.general.bgmvol, &d, "bgmvol", 35);
    CFG_INT(config->settings.general.brightness, &d, "brightness", 90);
    CFG_INT(config->settings.general.volume, &d, "volume", 75);
    CFG_INT(config->settings.general.rgb, &d, "rgb", 0);
    CFG_INT(config->settings.general.audiosink, &d, "audiosink", 0);
    CFG_INT(config->settings.general.theme_resolution, &d, "theme_resolution", 0);
    CFG_INT(config->settings.general.theme_scaling, &d, "theme_scaling", 1);
    CFG_STR(config->settings.general.startup, &d, "startup", "launcher");
    CFG_STR(config->settings.general.language, &d, "language", "English");

    // settings/hotkey/ (fields logically grouped under SETTINGS.general)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/hotkey");
    CFG_INT(config->settings.general.hkdpad, &d, "dpad_toggle", 1);
    CFG_INT(config->settings.general.hkshot, &d, "screenshot", 0);

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

    // settings/hdmi/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/hdmi");
    CFG_INT(config->settings.hdmi.resolution, &d, "resolution", 0);
    CFG_INT(config->settings.hdmi.space, &d, "space", 0);
    CFG_INT(config->settings.hdmi.depth, &d, "depth", 0);
    CFG_INT(config->settings.hdmi.range, &d, "range", 0);
    CFG_INT(config->settings.hdmi.scan, &d, "scan", 0);

    // settings/network/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/network");
    CFG_INT(config->settings.network.monitor, &d, "monitor", 0);
    CFG_INT(config->settings.network.boot, &d, "boot", 1);
    CFG_INT(config->settings.network.wake, &d, "wake", 1);
    CFG_INT(config->settings.network.compat, &d, "compat", 0);
    CFG_INT(config->settings.network.async_load, &d, "async_load", 1);
    CFG_INT(config->settings.network.con_retry, &d, "con_retry", 1);
    CFG_INT(config->settings.network.wait, &d, "wait_timer", 5);
    CFG_INT(config->settings.network.mod_retry, &d, "mod_retry", 1);
    CFG_INT(config->settings.network.proxy_enabled, &d, "proxy_enabled", 0);
    CFG_INT(config->settings.network.proxy_type, &d, "proxy_type", 0);
    CFG_STR(config->settings.network.proxy_server, &d, "proxy_server", "");
    CFG_STR(config->settings.network.proxy_noproxy, &d, "proxy_noproxy", "localhost,127.0.0.1,::1");

    // settings/overlay/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/overlay");
    CFG_INT(config->settings.overlay.gen_alpha, &d, "gen_alpha", 255);
    CFG_INT(config->settings.overlay.gen_anchor, &d, "gen_anchor", 4);
    CFG_INT(config->settings.overlay.gen_scale, &d, "gen_scale", 0);
    CFG_INT(config->settings.overlay.bat_alpha, &d, "bat_alpha", 255);
    CFG_INT(config->settings.overlay.bat_anchor, &d, "bat_anchor", 0);
    CFG_INT(config->settings.overlay.bat_scale, &d, "bat_scale", 0);
    CFG_INT(config->settings.overlay.vol_alpha, &d, "vol_alpha", 255);
    CFG_INT(config->settings.overlay.vol_anchor, &d, "vol_anchor", 2);
    CFG_INT(config->settings.overlay.vol_scale, &d, "vol_scale", 0);
    CFG_INT(config->settings.overlay.bri_alpha, &d, "bri_alpha", 255);
    CFG_INT(config->settings.overlay.bri_anchor, &d, "bri_anchor", 2);
    CFG_INT(config->settings.overlay.bri_scale, &d, "bri_scale", 0);

    // settings/power/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/power");
    CFG_INT(config->settings.power.low_battery, &d, "low_battery", 0);
    CFG_INT(config->settings.power.shutdown, &d, "shutdown", 0);
    CFG_INT(config->settings.power.idle.display, &d, "idle_display", 0);
    CFG_INT(config->settings.power.idle.sleep, &d, "idle_sleep", 0);
    CFG_INT(config->settings.power.idle.mute, &d, "idle_mute", 1);
    CFG_INT(config->settings.power.saver_type, &d, "saver_type", 0);
    CFG_INT(config->settings.power.saver_speed, &d, "saver_speed", 0);
    CFG_STR(config->settings.power.gov.idle, &d, "gov_idle", "powersave");

    // SETTINGS.power.gov.dflt lives in the device config, not user config
    cfg_dir_scan(&d, CONF_DEVICE_PATH "cpu");
    CFG_STR(config->settings.power.gov.dflt, &d, "default", "ondemand");

    // settings/rgb/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/rgb");
    CFG_INT(config->settings.rgb.mode, &d, "mode", 0);
    CFG_INT(config->settings.rgb.bright, &d, "bright", 0);
    CFG_INT(config->settings.rgb.breath_speed, &d, "breath_speed", 0);
    CFG_INT(config->settings.rgb.colour_l, &d, "colour_l", 0);
    CFG_INT(config->settings.rgb.colour_r, &d, "colour_r", 0);
    CFG_INT(config->settings.rgb.colour_m, &d, "colour_m", 0);
    CFG_INT(config->settings.rgb.colour_f1, &d, "colour_f1", 0);
    CFG_INT(config->settings.rgb.colour_f2, &d, "colour_f2", 0);
    CFG_INT(config->settings.rgb.combo, &d, "combo", 0);
    CFG_INT(config->settings.rgb.backend, &d, "backend", 0);

    // settings/font/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/font");
    CFG_INT(config->settings.font.list_size, &d, "list_size", 0);
    CFG_INT(config->settings.font.header_size, &d, "header_size", 0);
    CFG_INT(config->settings.font.footer_size, &d, "footer_size", 0);
    CFG_INT(config->settings.font.panel_size, &d, "panel_size", 0);
    CFG_STR(config->settings.font.name, &d, "name", "");

    // settings/theme/ (theme option overrides - distinct from theme/filter/)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/theme");
    CFG_INT(config->settings.themeopt.header_height, &d, "header_height", -1);
    CFG_INT(config->settings.themeopt.footer_height, &d, "footer_height", -1);
    CFG_INT(config->settings.themeopt.content_item_count, &d, "content_item_count", 0);
    CFG_INT(config->settings.themeopt.glyph_size_list, &d, "glyph_size_list", -2);
    CFG_INT(config->settings.themeopt.glyph_size_footer, &d, "glyph_size_footer", -2);
    CFG_INT(config->settings.themeopt.glyph_size_header, &d, "glyph_size_header", -2);
    CFG_INT(config->settings.themeopt.glyph_size_grid, &d, "glyph_size_grid", -2);
    CFG_INT(config->settings.themeopt.label_width, &d, "label_width", 0);

    // settings/remap/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/remap");
    CFG_INT(config->settings.remap.layout, &d, "layout", 0);

    // sort/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "sort");
    CFG_INT(config->sort.dflt, &d, "default", 0);
    CFG_INT(config->sort.collection, &d, "collection", 0);
    CFG_INT(config->sort.history, &d, "history", 0);

    // visual/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "visual");
    CFG_INT(config->visual.battery, &d, "battery", 0);
    CFG_INT(config->visual.network, &d, "network", 0);
    CFG_INT(config->visual.header_title, &d, "headertitle", 0);
    CFG_INT(config->visual.dialogue_transition, &d, "dialoguetransition", 1);
    CFG_INT(config->visual.bluetooth, &d, "bluetooth", 0);
    CFG_INT(config->visual.clock, &d, "clock", 1);
    CFG_INT(config->visual.overlay_image, &d, "overlayimage", 1);
    CFG_INT(config->visual.overlay_transparency, &d, "overlaytransparency", 85);
    CFG_INT(config->visual.grid_mode_content, &d, "gridmodecontent", 0);
    CFG_INT(config->visual.box_art, &d, "boxart", 0);
    CFG_INT(config->visual.box_art_align, &d, "boxartalign", 0);
    CFG_INT(config->visual.box_art_hide, &d, "boxarthide", 0);
    CFG_INT(config->visual.box_art_scale, &d, "boxartscale", 100);
    CFG_INT(config->visual.box_art_padding, &d, "boxartpadding", 15);
    CFG_INT(config->visual.box_art_placeholder, &d, "boxartplaceholder", 0);
    CFG_INT(config->visual.box_art_transition, &d, "boxarttransition", 1);
    CFG_INT(config->visual.video_preview, &d, "videopreview", 0);
    CFG_INT(config->visual.content_width, &d, "contentwidth", 0);
    CFG_INT(config->visual.name, &d, "name", 0);
    CFG_INT(config->visual.dash, &d, "dash", 0);
    CFG_INT(config->visual.launch_swap, &d, "launch_swap", 0);
    CFG_INT(config->visual.hidden, &d, "hidden", 0);
    CFG_INT(config->visual.shuffle, &d, "shuffle", 1);
    CFG_INT(config->visual.friendly_folder, &d, "friendlyfolder", 1);
    CFG_INT(config->visual.the_title_format, &d, "thetitleformat", 0);
    CFG_INT(config->visual.title_include_root_drive, &d, "titleincluderootdrive", 0);
    CFG_INT(config->visual.folder_item_count, &d, "folderitemcount", 0);
    CFG_INT(config->visual.display_empty_folder, &d, "folderempty", 0);
    CFG_INT(config->visual.menu_counter_folder, &d, "counterfolder", 1);
    CFG_INT(config->visual.menu_counter_file, &d, "counterfile", 1);
    CFG_INT(config->visual.video_wallpaper, &d, "video_wallpaper", 1);
    CFG_INT(config->visual.background_scale, &d, "background_scale", 2);
    CFG_INT(config->visual.launchsplash, &d, "launchsplash", 0);
    CFG_INT(config->visual.blackfade, &d, "blackfade", 1);
    CFG_INT(config->visual.content_collect, &d, "contentcollect", 0);
    CFG_INT(config->visual.content_history, &d, "contenthistory", 0);
    CFG_INT(config->visual.mixed_content, &d, "mixedcontent", 0);
    CFG_INT(config->visual.forward_history, &d, "forwardhistory", 1);
    CFG_INT(config->visual.name_scroll, &d, "namescroll", 1);
    CFG_INT(config->visual.label_scroll_speed, &d, "labelscrollspeed", 2);
    CFG_INT(config->visual.list_glyph, &d, "listglyph", 1);
    CFG_INT(config->visual.selection_animation, &d, "selectionanimation", 2);
    CFG_INT(config->visual.selection_style, &d, "selectionstyle", 4);
    CFG_INT(config->visual.render_shadows, &d, "shadow", 1);

    // bluetooth/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "bluetooth");
    CFG_INT(config->bluetooth.auto_connect, &d, "autoconnect", 0);

    // web/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "web");
    CFG_INT(config->web.sshd, &d, "sshd", 0);
    CFG_INT(config->web.sftp_go, &d, "sftpgo", 0);
    CFG_INT(config->web.ttyd, &d, "ttyd", 0);
    CFG_INT(config->web.syncthing, &d, "syncthing", 0);
    CFG_INT(config->web.tailscaled, &d, "tailscaled", 0);

    // danger/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "danger");
    CFG_INT(config->danger.vm_swap, &d, "vmswap", 8);
    CFG_INT(config->danger.dirty_ratio, &d, "dirty_ratio", 16);
    CFG_INT(config->danger.dirty_back, &d, "dirty_back_ratio", 4);
    CFG_INT(config->danger.cache, &d, "cache_pressure", 64);
    CFG_INT(config->danger.merge, &d, "nomerges", 0);
    CFG_INT(config->danger.requests, &d, "nr_requests", 128);
    CFG_INT(config->danger.read_ahead, &d, "read_ahead", 4096);
    CFG_INT(config->danger.page_cluster, &d, "page_cluster", 3);
    CFG_INT(config->danger.time_slice, &d, "time_slice", 10);
    CFG_INT(config->danger.io_stats, &d, "iostats", 0);
    CFG_INT(config->danger.idle_flush, &d, "idle_flush", 0);
    CFG_INT(config->danger.child_first, &d, "child_first", 0);
    CFG_INT(config->danger.tune_scale, &d, "tune_scale", 1);
    CFG_STR(config->danger.card_mode, &d, "cardmode", "noop");
    CFG_STR(config->danger.state, &d, "state", "mem");
}

#undef CFG_STR
#undef CFG_INT

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
