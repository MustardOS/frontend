#include "common.h"
#include "options.h"
#include "config.h"
#include "config_reader.h"


#define CFG_STR(field, d, name, fallback)          \
    do {                                           \
        const char *_v = cfg_dir_get((d), (name)); \
        snprintf((field), sizeof(field), "%s",     \
            (_v && *_v) ? _v : (fallback));        \
    } while (0)

#define CFG_INT(field, d, name, fallback)                         \
    do {                                                          \
        (field) = (int16_t) cfg_dir_int((d), (name), (fallback)); \
    } while (0)

void load_config(struct mux_config *config) {
    cfg_dir_t d;

    // system/ - build, version, debug_mode
    cfg_dir_scan(&d, CONF_CONFIG_PATH "system");
    CFG_STR(config->SYSTEM.BUILD, &d, "build", "Unknown");
    CFG_STR(config->SYSTEM.VERSION, &d, "version", "Edge");
    CFG_INT(config->SETTINGS.ADVANCED.DEBUGLOG, &d, "debug_mode", 0);

    // backup/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "backup");
    CFG_INT(config->BACKUP.APPS, &d, "application", 1);
    CFG_INT(config->BACKUP.BIOS, &d, "bios", 1);
    CFG_INT(config->BACKUP.CATALOGUE, &d, "catalogue", 1);
    CFG_INT(config->BACKUP.CHEATS, &d, "cheats", 1);
    CFG_INT(config->BACKUP.COLLECTION, &d, "collection", 1);
    CFG_INT(config->BACKUP.CONFIG, &d, "config", 1);
    CFG_INT(config->BACKUP.CONTENT, &d, "content", 1);
    CFG_INT(config->BACKUP.HISTORY, &d, "history", 1);
    CFG_INT(config->BACKUP.INIT, &d, "init", 1);
    CFG_INT(config->BACKUP.MERGE, &d, "merge", 1);
    CFG_INT(config->BACKUP.MUSIC, &d, "music", 1);
    CFG_INT(config->BACKUP.NAME, &d, "name", 1);
    CFG_INT(config->BACKUP.NETWORK, &d, "network", 1);
    CFG_INT(config->BACKUP.OVERLAYS, &d, "overlays", 1);
    CFG_INT(config->BACKUP.OVERRIDE, &d, "override", 1);
    CFG_INT(config->BACKUP.PACKAGE, &d, "package", 1);
    CFG_INT(config->BACKUP.SAVE, &d, "save", 1);
    CFG_INT(config->BACKUP.SCREENSHOT, &d, "screenshot", 1);
    CFG_INT(config->BACKUP.SHADERS, &d, "shaders", 1);
    CFG_INT(config->BACKUP.SYNCTHING, &d, "syncthing", 1);
    CFG_INT(config->BACKUP.THEME, &d, "theme", 1);
    CFG_INT(config->BACKUP.TRACK, &d, "track", 1);
    CFG_INT(config->BACKUP.START, &d, "start", 0);
    CFG_INT(config->BACKUP.TARGET, &d, "target", 0);

    // boot/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "boot");
    CFG_INT(config->BOOT.FACTORY_RESET, &d, "factory_reset", 0);
    CFG_INT(config->BOOT.DEVICE_MODE, &d, "device_mode", 0);

    // clock/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "clock");
    CFG_INT(config->CLOCK.NOTATION, &d, "notation", 0);
    CFG_STR(config->CLOCK.POOL, &d, "pool", "pool.ntp.org");
    CFG_STR(config->CLOCK.CUSTOM, &d, "custom", "%H%P %Z");

    // network/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "network");
    CFG_INT(config->NETWORK.TYPE, &d, "type", 0);
    CFG_STR(config->NETWORK.INTERFACE, &d, "interface", "wlan0");
    CFG_STR(config->NETWORK.SSID, &d, "ssid", "");
    CFG_STR(config->NETWORK.PASS, &d, "pass", "");
    CFG_STR(config->NETWORK.ADDRESS, &d, "address", "192.168.0.123");
    CFG_STR(config->NETWORK.GATEWAY, &d, "gateway", "192.168.0.1");
    CFG_STR(config->NETWORK.SUBNET, &d, "subnet", "24");
    CFG_STR(config->NETWORK.DNS, &d, "dns", "1.1.1.1");

    // theme/ (top level - contains the "active" file; subdirs handled separately)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme");
    CFG_STR(config->THEME.ACTIVE, &d, "active", "MustardOS");
    snprintf(config->THEME.STORAGE_THEME, sizeof(config->THEME.STORAGE_THEME),
             RUN_STORAGE_PATH "theme/%s", config->THEME.ACTIVE);
    snprintf(config->THEME.THEME_CAT_PATH, sizeof(config->THEME.THEME_CAT_PATH),
             "%s/catalogue", config->THEME.STORAGE_THEME);

    // theme/filter/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme/filter");
    CFG_INT(config->THEME.FILTER.ALLTHEMES, &d, "allthemes", 0);
    CFG_INT(config->THEME.FILTER.GRID, &d, "grid", 0);
    CFG_INT(config->THEME.FILTER.HDMI, &d, "hdmi", 0);
    CFG_INT(config->THEME.FILTER.LANGUAGE, &d, "language", 0);
    CFG_STR(config->THEME.FILTER.LOOKUP, &d, "lookup", "");

    if (!config->THEME.FILTER.ALLTHEMES) {
        cfg_dir_scan(&d, CONF_DEVICE_PATH "mux");
        int width = cfg_dir_int(&d, "width", 0);
        int height = cfg_dir_int(&d, "height", 0);
        config->THEME.FILTER.RESOLUTION_640x480 = (width == 640 && height == 480) ? 1 : 0;
        config->THEME.FILTER.RESOLUTION_720x480 = (width == 720 && height == 480) ? 1 : 0;
        config->THEME.FILTER.RESOLUTION_720x720 = (width == 720 && height == 720) ? 1 : 0;
        config->THEME.FILTER.RESOLUTION_1024x768 = (width == 1024 && height == 768) ? 1 : 0;
        config->THEME.FILTER.RESOLUTION_1280x720 = (width == 1280 && height == 720) ? 1 : 0;
        config->THEME.FILTER.RESOLUTION_1920x1080 = (width == 1920 && height == 1080) ? 1 : 0;
    } else {
        config->THEME.FILTER.RESOLUTION_640x480 = 0;
        config->THEME.FILTER.RESOLUTION_720x480 = 0;
        config->THEME.FILTER.RESOLUTION_720x720 = 0;
        config->THEME.FILTER.RESOLUTION_1024x768 = 0;
        config->THEME.FILTER.RESOLUTION_1280x720 = 0;
        config->THEME.FILTER.RESOLUTION_1920x1080 = 0;
    }

    // theme/download/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "theme/download");
    CFG_STR(config->THEME.DOWNLOAD.DATA, &d, "data", "");
    CFG_STR(config->THEME.DOWNLOAD.PREVIEW, &d, "preview", "");

    // extra/download/ and extra/language/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "extra/download");
    CFG_STR(config->EXTRA.DOWNLOAD.DATA, &d, "data", "");

    cfg_dir_scan(&d, CONF_CONFIG_PATH "extra/language");
    CFG_STR(config->EXTRA.LANGUAGE.DATA, &d, "data", "");

    // settings/advanced/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/advanced");
    CFG_INT(config->SETTINGS.ADVANCED.ACCELERATE, &d, "accelerate", 96);
    CFG_INT(config->SETTINGS.ADVANCED.REPEATDELAY, &d, "repeat_delay", 208);
    CFG_INT(config->SETTINGS.ADVANCED.STICKNAV, &d, "sticknav", 0);
    CFG_INT(config->SETTINGS.ADVANCED.THERMAL, &d, "thermal", 1);
    CFG_INT(config->SETTINGS.ADVANCED.FONT, &d, "font", 0);
    CFG_INT(config->SETTINGS.ADVANCED.LED, &d, "led", 0);
    CFG_INT(config->SETTINGS.ADVANCED.RANDOMTHEME, &d, "random_theme", 0);
    CFG_INT(config->SETTINGS.ADVANCED.RETROWAIT, &d, "retrowait", 0);
    CFG_INT(config->SETTINGS.ADVANCED.RETROFREE, &d, "retrofree", 0);
    CFG_INT(config->SETTINGS.ADVANCED.RETROCACHE, &d, "retrocache", 0);
    CFG_INT(config->SETTINGS.ADVANCED.ACTIVITY, &d, "activity", 1);
    CFG_INT(config->SETTINGS.ADVANCED.USBFUNCTION, &d, "usb_function", 0);
    CFG_INT(config->SETTINGS.ADVANCED.VERBOSE, &d, "verbose", 0);
    CFG_INT(config->SETTINGS.ADVANCED.RUMBLE, &d, "rumble", 0);
    CFG_INT(config->SETTINGS.ADVANCED.VOLUME, &d, "volume", 0);
    CFG_INT(config->SETTINGS.ADVANCED.BRIGHTNESS, &d, "brightness", 0);
    CFG_INT(config->SETTINGS.ADVANCED.USERINIT, &d, "user_init", 0);
    CFG_INT(config->SETTINGS.ADVANCED.DPADSWAP, &d, "dpad_swap", 1);
    CFG_INT(config->SETTINGS.ADVANCED.OVERDRIVE, &d, "overdrive", 0);
    CFG_INT(config->SETTINGS.ADVANCED.SWAPFILE, &d, "swapfile", 0);
    CFG_INT(config->SETTINGS.ADVANCED.ZRAMFILE, &d, "zramfile", 0);
    CFG_INT(config->SETTINGS.ADVANCED.LIDSWITCH, &d, "lidswitch", 1);
    CFG_INT(config->SETTINGS.ADVANCED.DISPSUSPEND, &d, "disp_suspend", 0);
    CFG_INT(config->SETTINGS.ADVANCED.INCBRIGHT, &d, "incbright", 16);
    CFG_INT(config->SETTINGS.ADVANCED.INCVOLUME, &d, "incvolume", 8);
    CFG_INT(config->SETTINGS.ADVANCED.MAXGPU, &d, "maxgpu", 0);
    CFG_INT(config->SETTINGS.ADVANCED.AUDIOREADY, &d, "audio_ready", 0);
    CFG_INT(config->SETTINGS.ADVANCED.AUDIOSWAP, &d, "audio_swap", 0);
    CFG_INT(config->SETTINGS.ADVANCED.USBPART, &d, "part_external", 0);
    CFG_INT(config->SETTINGS.ADVANCED.SECONDPART, &d, "part_secondary", 0);
    CFG_INT(config->SETTINGS.ADVANCED.TRUSTMODIFY, &d, "trust_modify", 0);
    CFG_INT(config->SETTINGS.ADVANCED.TRUSTPOWER, &d, "trust_power", 1);
    CFG_INT(config->SETTINGS.ADVANCED.TRUSTREMOVE, &d, "trust_remove", 0);

    // settings/colour/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/colour");
    CFG_INT(config->SETTINGS.COLOUR.TEMPERATURE, &d, "temperature", DEFAULT_TEMPERATURE);
    CFG_INT(config->SETTINGS.COLOUR.BRIGHTNESS, &d, "brightness", 0);
    CFG_INT(config->SETTINGS.COLOUR.CONTRAST, &d, "contrast", 100);
    CFG_INT(config->SETTINGS.COLOUR.SATURATION, &d, "saturation", 100);
    CFG_INT(config->SETTINGS.COLOUR.HUESHIFT, &d, "hueshift", 0);
    CFG_INT(config->SETTINGS.COLOUR.GAMMA, &d, "gamma", 100);

    // settings/general/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/general");
    CFG_INT(config->SETTINGS.GENERAL.SOUND, &d, "sound", 0);
    CFG_INT(config->SETTINGS.GENERAL.SOUNDVOL, &d, "soundvol", 100);
    CFG_INT(config->SETTINGS.GENERAL.CHIME, &d, "chime", 0);
    CFG_INT(config->SETTINGS.GENERAL.BGM, &d, "bgm", 0);
    CFG_INT(config->SETTINGS.GENERAL.BGMVOL, &d, "bgmvol", 35);
    CFG_INT(config->SETTINGS.GENERAL.BRIGHTNESS, &d, "brightness", 90);
    CFG_INT(config->SETTINGS.GENERAL.VOLUME, &d, "volume", 75);
    CFG_INT(config->SETTINGS.GENERAL.RGB, &d, "rgb", 0);
    CFG_INT(config->SETTINGS.GENERAL.AUDIOSINK, &d, "audiosink", 0);
    CFG_INT(config->SETTINGS.GENERAL.THEME_RESOLUTION, &d, "theme_resolution", 0);
    CFG_INT(config->SETTINGS.GENERAL.THEME_SCALING, &d, "theme_scaling", 1);
    CFG_STR(config->SETTINGS.GENERAL.STARTUP, &d, "startup", "launcher");
    CFG_STR(config->SETTINGS.GENERAL.LANGUAGE, &d, "language", "English");

    // settings/hotkey/ (fields logically grouped under SETTINGS.GENERAL)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/hotkey");
    CFG_INT(config->SETTINGS.GENERAL.HKDPAD, &d, "dpad_toggle", 1);
    CFG_INT(config->SETTINGS.GENERAL.HKSHOT, &d, "screenshot", 0);

    // Compute theme resolution dimensions
    config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 0;
    config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 0;
    switch (config->SETTINGS.GENERAL.THEME_RESOLUTION) {
        case 1:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 640;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 480;
            break;
        case 2:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 720;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 480;
            break;
        case 3:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 720;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 576;
            break;
        case 4:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 720;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 720;
            break;
        case 5:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 1024;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 768;
            break;
        case 6:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 1280;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 720;
            break;
        case 7:
            config->SETTINGS.GENERAL.THEME_RESOLUTION_WIDTH = 1920;
            config->SETTINGS.GENERAL.THEME_RESOLUTION_HEIGHT = 1080;
            break;
    }

    // settings/hdmi/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/hdmi");
    CFG_INT(config->SETTINGS.HDMI.RESOLUTION, &d, "resolution", 0);
    CFG_INT(config->SETTINGS.HDMI.SPACE, &d, "space", 0);
    CFG_INT(config->SETTINGS.HDMI.DEPTH, &d, "depth", 0);
    CFG_INT(config->SETTINGS.HDMI.RANGE, &d, "range", 0);
    CFG_INT(config->SETTINGS.HDMI.SCAN, &d, "scan", 0);
    CFG_INT(config->SETTINGS.HDMI.AUDIO, &d, "audio", 0);

    // settings/network/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/network");
    CFG_INT(config->SETTINGS.NETWORK.MONITOR, &d, "monitor", 0);
    CFG_INT(config->SETTINGS.NETWORK.BOOT, &d, "boot", 1);
    CFG_INT(config->SETTINGS.NETWORK.WAKE, &d, "wake", 1);
    CFG_INT(config->SETTINGS.NETWORK.COMPAT, &d, "compat", 0);
    CFG_INT(config->SETTINGS.NETWORK.ASYNCLOAD, &d, "async_load", 1);
    CFG_INT(config->SETTINGS.NETWORK.CONRETRY, &d, "con_retry", 1);
    CFG_INT(config->SETTINGS.NETWORK.WAIT, &d, "wait_timer", 5);
    CFG_INT(config->SETTINGS.NETWORK.MODRETRY, &d, "mod_retry", 1);

    // settings/overlay/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/overlay");
    CFG_INT(config->SETTINGS.OVERLAY.GENALPHA, &d, "gen_alpha", 255);
    CFG_INT(config->SETTINGS.OVERLAY.GENANCHOR, &d, "gen_anchor", 4);
    CFG_INT(config->SETTINGS.OVERLAY.GENSCALE, &d, "gen_scale", 0);
    CFG_INT(config->SETTINGS.OVERLAY.BATALPHA, &d, "bat_alpha", 255);
    CFG_INT(config->SETTINGS.OVERLAY.BATANCHOR, &d, "bat_anchor", 0);
    CFG_INT(config->SETTINGS.OVERLAY.BATSCALE, &d, "bat_scale", 0);
    CFG_INT(config->SETTINGS.OVERLAY.VOLALPHA, &d, "vol_alpha", 255);
    CFG_INT(config->SETTINGS.OVERLAY.VOLANCHOR, &d, "vol_anchor", 2);
    CFG_INT(config->SETTINGS.OVERLAY.VOLSCALE, &d, "vol_scale", 0);
    CFG_INT(config->SETTINGS.OVERLAY.BRIALPHA, &d, "bri_alpha", 255);
    CFG_INT(config->SETTINGS.OVERLAY.BRIANCHOR, &d, "bri_anchor", 2);
    CFG_INT(config->SETTINGS.OVERLAY.BRISCALE, &d, "bri_scale", 0);

    // settings/power/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/power");
    CFG_INT(config->SETTINGS.POWER.LOW_BATTERY, &d, "low_battery", 0);
    CFG_INT(config->SETTINGS.POWER.SHUTDOWN, &d, "shutdown", 0);
    CFG_INT(config->SETTINGS.POWER.IDLE.DISPLAY, &d, "idle_display", 0);
    CFG_INT(config->SETTINGS.POWER.IDLE.SLEEP, &d, "idle_sleep", 0);
    CFG_INT(config->SETTINGS.POWER.IDLE.MUTE, &d, "idle_mute", 1);
    CFG_INT(config->SETTINGS.POWER.SAVERTYPE, &d, "saver_type", 0);
    CFG_INT(config->SETTINGS.POWER.SAVERSPEED, &d, "saver_speed", 0);
    CFG_STR(config->SETTINGS.POWER.GOV.IDLE, &d, "gov_idle", "powersave");

    // SETTINGS.POWER.GOV.DEFAULT lives in the device config, not user config
    cfg_dir_scan(&d, CONF_DEVICE_PATH "cpu");
    CFG_STR(config->SETTINGS.POWER.GOV.DEFAULT, &d, "default", "ondemand");

    // settings/rgb/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/rgb");
    CFG_INT(config->SETTINGS.RGB.MODE, &d, "mode", 0);
    CFG_INT(config->SETTINGS.RGB.BRIGHT, &d, "bright", 0);
    CFG_INT(config->SETTINGS.RGB.BREATH_SPEED, &d, "breath_speed", 0);
    CFG_INT(config->SETTINGS.RGB.COLOURL, &d, "colour_l", 0);
    CFG_INT(config->SETTINGS.RGB.COLOURR, &d, "colour_r", 0);
    CFG_INT(config->SETTINGS.RGB.COLOURM, &d, "colour_m", 0);
    CFG_INT(config->SETTINGS.RGB.COLOURF1, &d, "colour_f1", 0);
    CFG_INT(config->SETTINGS.RGB.COLOURF2, &d, "colour_f2", 0);
    CFG_INT(config->SETTINGS.RGB.COMBO, &d, "combo", 0);
    CFG_INT(config->SETTINGS.RGB.BACKEND, &d, "backend", 0);

    // settings/font/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/font");
    CFG_INT(config->SETTINGS.FONT.LIST_SIZE, &d, "list_size", 0);
    CFG_INT(config->SETTINGS.FONT.HEADER_SIZE, &d, "header_size", 0);
    CFG_INT(config->SETTINGS.FONT.FOOTER_SIZE, &d, "footer_size", 0);
    CFG_INT(config->SETTINGS.FONT.PANEL_SIZE, &d, "panel_size", 0);
    CFG_STR(config->SETTINGS.FONT.NAME, &d, "name", "");

    // settings/theme/ (theme option overrides - distinct from theme/filter/)
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/theme");
    CFG_INT(config->SETTINGS.THEMEOPT.HEADER_HEIGHT, &d, "header_height", -1);
    CFG_INT(config->SETTINGS.THEMEOPT.FOOTER_HEIGHT, &d, "footer_height", -1);
    CFG_INT(config->SETTINGS.THEMEOPT.CONTENT_ITEM_COUNT, &d, "content_item_count", 0);
    CFG_INT(config->SETTINGS.THEMEOPT.GLYPH_SIZE, &d, "glyph_size", -2);

    // settings/remap/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "settings/remap");
    CFG_INT(config->SETTINGS.REMAP.LAYOUT, &d, "layout", 0);

    // sort/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "sort");
    CFG_INT(config->SORT.DEFAULT, &d, "default", 0);
    CFG_INT(config->SORT.COLLECTION, &d, "collection", 0);
    CFG_INT(config->SORT.HISTORY, &d, "history", 0);

    // visual/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "visual");
    CFG_INT(config->VISUAL.BATTERY, &d, "battery", 0);
    CFG_INT(config->VISUAL.NETWORK, &d, "network", 0);
    CFG_INT(config->VISUAL.HEADERTITLE, &d, "headertitle", 0);
    CFG_INT(config->VISUAL.BLUETOOTH, &d, "bluetooth", 0);
    CFG_INT(config->VISUAL.CLOCK, &d, "clock", 1);
    CFG_INT(config->VISUAL.OVERLAYIMAGE, &d, "overlayimage", 1);
    CFG_INT(config->VISUAL.OVERLAYTRANSPARENCY, &d, "overlaytransparency", 85);
    CFG_INT(config->VISUAL.GRID_MODE_CONTENT, &d, "gridmodecontent", 0);
    CFG_INT(config->VISUAL.BOX_ART, &d, "boxart", 0);
    CFG_INT(config->VISUAL.BOX_ART_ALIGN, &d, "boxartalign", 0);
    CFG_INT(config->VISUAL.BOX_ART_HIDE, &d, "boxarthide", 0);
    CFG_INT(config->VISUAL.BOX_ART_SCALE, &d, "boxartscale", 99);
    CFG_INT(config->VISUAL.CONTENT_WIDTH, &d, "contentwidth", 0);
    CFG_INT(config->VISUAL.NAME, &d, "name", 0);
    CFG_INT(config->VISUAL.DASH, &d, "dash", 0);
    CFG_INT(config->VISUAL.LAUNCH_SWAP, &d, "launch_swap", 0);
    CFG_INT(config->VISUAL.HIDDEN, &d, "hidden", 0);
    CFG_INT(config->VISUAL.SHUFFLE, &d, "shuffle", 1);
    CFG_INT(config->VISUAL.FRIENDLYFOLDER, &d, "friendlyfolder", 1);
    CFG_INT(config->VISUAL.THETITLEFORMAT, &d, "thetitleformat", 0);
    CFG_INT(config->VISUAL.TITLEINCLUDEROOTDRIVE, &d, "titleincluderootdrive", 0);
    CFG_INT(config->VISUAL.FOLDERITEMCOUNT, &d, "folderitemcount", 0);
    CFG_INT(config->VISUAL.DISPLAYEMPTYFOLDER, &d, "folderempty", 0);
    CFG_INT(config->VISUAL.MENUCOUNTERFOLDER, &d, "counterfolder", 1);
    CFG_INT(config->VISUAL.MENUCOUNTERFILE, &d, "counterfile", 1);
    CFG_INT(config->VISUAL.BACKGROUNDANIMATION, &d, "backgroundanimation", 0);
    CFG_INT(config->VISUAL.LAUNCHSPLASH, &d, "launchsplash", 0);
    CFG_INT(config->VISUAL.BLACKFADE, &d, "blackfade", 1);
    CFG_INT(config->VISUAL.CONTENTCOLLECT, &d, "contentcollect", 0);
    CFG_INT(config->VISUAL.CONTENTHISTORY, &d, "contenthistory", 0);
    CFG_INT(config->VISUAL.MIXEDCONTENT, &d, "mixedcontent", 0);
    CFG_INT(config->VISUAL.FORWARDHISTORY, &d, "forwardhistory", 1);
    CFG_INT(config->VISUAL.NAMESCROLL, &d, "namescroll", 1);

    // bluetooth/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "bluetooth");
    CFG_INT(config->BLUETOOTH.AUTOCONNECT, &d, "autoconnect", 0);

    // web/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "web");
    CFG_INT(config->WEB.SSHD, &d, "sshd", 0);
    CFG_INT(config->WEB.SFTPGO, &d, "sftpgo", 0);
    CFG_INT(config->WEB.TTYD, &d, "ttyd", 0);
    CFG_INT(config->WEB.SYNCTHING, &d, "syncthing", 0);
    CFG_INT(config->WEB.TAILSCALED, &d, "tailscaled", 0);

    // danger/
    cfg_dir_scan(&d, CONF_CONFIG_PATH "danger");
    CFG_INT(config->DANGER.VMSWAP, &d, "vmswap", 8);
    CFG_INT(config->DANGER.DIRTYRATIO, &d, "dirty_ratio", 16);
    CFG_INT(config->DANGER.DIRTYBACK, &d, "dirty_back_ratio", 4);
    CFG_INT(config->DANGER.CACHE, &d, "cache_pressure", 64);
    CFG_INT(config->DANGER.MERGE, &d, "nomerges", 0);
    CFG_INT(config->DANGER.REQUESTS, &d, "nr_requests", 128);
    CFG_INT(config->DANGER.READAHEAD, &d, "read_ahead", 4096);
    CFG_INT(config->DANGER.PAGECLUSTER, &d, "page_cluster", 3);
    CFG_INT(config->DANGER.TIMESLICE, &d, "time_slice", 10);
    CFG_INT(config->DANGER.IOSTATS, &d, "iostats", 0);
    CFG_INT(config->DANGER.IDLEFLUSH, &d, "idle_flush", 0);
    CFG_INT(config->DANGER.CHILDFIRST, &d, "child_first", 0);
    CFG_INT(config->DANGER.TUNESCALE, &d, "tune_scale", 1);
    CFG_STR(config->DANGER.CARDMODE, &d, "cardmode", "noop");
    CFG_STR(config->DANGER.STATE, &d, "state", "mem");
}

#undef CFG_STR
#undef CFG_INT
