#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "config.h"

void load_config(struct mux_config *config) {
    char buffer[MAX_BUFFER_SIZE];

    CFG_STR_FIELD(config->SYSTEM.BUILD, CONF_CONFIG_PATH "system/build", "Unknown")
    CFG_STR_FIELD(config->SYSTEM.VERSION, CONF_CONFIG_PATH "system/version", "Edge")

    CFG_INT_FIELD(config->BACKUP.APPS, CONF_CONFIG_PATH "backup/application", 1)
    CFG_INT_FIELD(config->BACKUP.BIOS, CONF_CONFIG_PATH "backup/bios", 1)
    CFG_INT_FIELD(config->BACKUP.CATALOGUE, CONF_CONFIG_PATH "backup/catalogue", 1)
    CFG_INT_FIELD(config->BACKUP.CHEATS, CONF_CONFIG_PATH "backup/cheats", 1)
    CFG_INT_FIELD(config->BACKUP.COLLECTION, CONF_CONFIG_PATH "backup/collection", 1)
    CFG_INT_FIELD(config->BACKUP.CONFIG, CONF_CONFIG_PATH "backup/config", 1)
    CFG_INT_FIELD(config->BACKUP.HISTORY, CONF_CONFIG_PATH "backup/history", 1)
    CFG_INT_FIELD(config->BACKUP.INIT, CONF_CONFIG_PATH "backup/init", 1)
    CFG_INT_FIELD(config->BACKUP.MERGE, CONF_CONFIG_PATH "backup/merge", 1)
    CFG_INT_FIELD(config->BACKUP.MUSIC, CONF_CONFIG_PATH "backup/music", 1)
    CFG_INT_FIELD(config->BACKUP.NAME, CONF_CONFIG_PATH "backup/name", 1)
    CFG_INT_FIELD(config->BACKUP.NETWORK, CONF_CONFIG_PATH "backup/network", 1)
    CFG_INT_FIELD(config->BACKUP.OVERLAYS, CONF_CONFIG_PATH "backup/overlays", 1)
    CFG_INT_FIELD(config->BACKUP.OVERRIDE, CONF_CONFIG_PATH "backup/override", 1)
    CFG_INT_FIELD(config->BACKUP.PACKAGE, CONF_CONFIG_PATH "backup/package", 1)
    CFG_INT_FIELD(config->BACKUP.SAVE, CONF_CONFIG_PATH "backup/save", 1)
    CFG_INT_FIELD(config->BACKUP.SCREENSHOT, CONF_CONFIG_PATH "backup/screenshot", 1)
    CFG_INT_FIELD(config->BACKUP.SHADERS, CONF_CONFIG_PATH "backup/shaders", 1)
    CFG_INT_FIELD(config->BACKUP.SYNCTHING, CONF_CONFIG_PATH "backup/syncthing", 1)
    CFG_INT_FIELD(config->BACKUP.THEME, CONF_CONFIG_PATH "backup/theme", 1)
    CFG_INT_FIELD(config->BACKUP.TRACK, CONF_CONFIG_PATH "backup/track", 1)

    CFG_INT_FIELD(config->BOOT.FACTORY_RESET, CONF_CONFIG_PATH "boot/factory_reset", 0)
    CFG_INT_FIELD(config->BOOT.DEVICE_MODE, CONF_CONFIG_PATH "boot/device_mode", 0)

    CFG_INT_FIELD(config->CLOCK.NOTATION, CONF_CONFIG_PATH "clock/notation", 0)
    CFG_STR_FIELD(config->CLOCK.POOL, CONF_CONFIG_PATH "clock/pool", "pool.ntp.org")

    CFG_INT_FIELD(config->NETWORK.TYPE, CONF_CONFIG_PATH "network/type", 0)
    CFG_STR_FIELD(config->NETWORK.INTERFACE, CONF_CONFIG_PATH "network/interface", "wlan0")
    CFG_STR_FIELD(config->NETWORK.SSID, CONF_CONFIG_PATH "network/ssid", "")
    CFG_STR_FIELD(config->NETWORK.PASS, CONF_CONFIG_PATH "network/pass", "")
    CFG_INT_FIELD(config->NETWORK.SCAN, CONF_CONFIG_PATH "network/scan", 0)
    CFG_STR_FIELD(config->NETWORK.ADDRESS, CONF_CONFIG_PATH "network/address", "192.168.0.123")
    CFG_STR_FIELD(config->NETWORK.GATEWAY, CONF_CONFIG_PATH "network/gateway", "192.168.0.1")
    CFG_STR_FIELD(config->NETWORK.SUBNET, CONF_CONFIG_PATH "network/subnet", "24")
    CFG_STR_FIELD(config->NETWORK.DNS, CONF_CONFIG_PATH "network/dns", "1.1.1.1")

    CFG_INT_FIELD(config->THEME.FILTER.ALL_THEMES, CONF_CONFIG_PATH "theme/filter/allthemes", 0)
    CFG_INT_FIELD(config->THEME.FILTER.GRID, CONF_CONFIG_PATH "theme/filter/grid", 0)
    CFG_INT_FIELD(config->THEME.FILTER.HDMI, CONF_CONFIG_PATH "theme/filter/hdmi", 0)
    CFG_INT_FIELD(config->THEME.FILTER.LANGUAGE, CONF_CONFIG_PATH "theme/filter/language", 0)
    CFG_STR_FIELD(config->THEME.FILTER.LOOKUP, CONF_CONFIG_PATH "theme/filter/lookup", "")
    if (!config->THEME.FILTER.ALL_THEMES) {
        int width = read_line_int_from((CONF_DEVICE_PATH "mux/width"), 1);
        int height = read_line_int_from((CONF_DEVICE_PATH "mux/height"), 1);
        if (width == 640 && height == 480) {
            config->THEME.FILTER.RESOLUTION_640x480 = 1;
        } else if (width == 720 && height == 480) {
            config->THEME.FILTER.RESOLUTION_720x480 = 1;
        } else if (width == 720 && height == 720) {
            config->THEME.FILTER.RESOLUTION_720x720 = 1;
        } else if (width == 1024 && height == 768) {
            config->THEME.FILTER.RESOLUTION_1024x768 = 1;
        } else if (width == 1280 && height == 720) {
            config->THEME.FILTER.RESOLUTION_1280x720 = 1;
        }
    } else {
        config->THEME.FILTER.RESOLUTION_640x480 = 0;
        config->THEME.FILTER.RESOLUTION_720x480 = 0;
        config->THEME.FILTER.RESOLUTION_720x720 = 0;
        config->THEME.FILTER.RESOLUTION_1024x768 = 0;
        config->THEME.FILTER.RESOLUTION_1280x720 = 0;
    }

    CFG_STR_FIELD(config->EXTRA.DOWNLOAD.DATA, CONF_CONFIG_PATH "extra/download/data", "")
    CFG_STR_FIELD(config->EXTRA.LANGUAGE.DATA, CONF_CONFIG_PATH "extra/language/data", "")

    CFG_STR_FIELD(config->THEME.DOWNLOAD.DATA, CONF_CONFIG_PATH "theme/download/data", "")
    CFG_STR_FIELD(config->THEME.DOWNLOAD.PREVIEW, CONF_CONFIG_PATH "theme/download/preview", "")
    CFG_STR_FIELD(config->THEME.ACTIVE, CONF_CONFIG_PATH "theme/active", "MustardOS")
    CFG_STR_FIELD(config->THEME.DEFAULT_HASH, CONF_CONFIG_PATH "theme/default", "")
    snprintf(config->THEME.STORAGE_THEME, MAX_BUFFER_SIZE, RUN_STORAGE_PATH "theme/%s", config->THEME.ACTIVE);
    snprintf(config->THEME.THEME_CAT_PATH, MAX_BUFFER_SIZE, "%s/catalogue", config->THEME.STORAGE_THEME);

    CFG_INT_FIELD(config->SETTINGS.ADVANCED.ACCELERATE, CONF_CONFIG_PATH "settings/advanced/accelerate", 96)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.REPEAT_DELAY, CONF_CONFIG_PATH "settings/advanced/repeat_delay", 208)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.SWAP, CONF_CONFIG_PATH "settings/advanced/swap", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THERMAL, CONF_CONFIG_PATH "settings/advanced/thermal", 1)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.FONT, CONF_CONFIG_PATH "settings/advanced/font", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.OFFSET, CONF_CONFIG_PATH "settings/advanced/offset", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LOCK, CONF_CONFIG_PATH "settings/advanced/lock", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LED, CONF_CONFIG_PATH "settings/advanced/led", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THEME, CONF_CONFIG_PATH "settings/advanced/random_theme", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RETROWAIT, CONF_CONFIG_PATH "settings/advanced/retrowait", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RETROFREE, CONF_CONFIG_PATH "settings/advanced/retrofree", 0)
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.USBFUNCTION, CONF_CONFIG_PATH "settings/advanced/usb_function", "none")
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.VERBOSE, CONF_CONFIG_PATH "settings/advanced/verbose", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RUMBLE, CONF_CONFIG_PATH "settings/advanced/rumble", 0)
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.VOLUME, CONF_CONFIG_PATH "settings/advanced/volume", "previous")
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.BRIGHTNESS, CONF_CONFIG_PATH "settings/advanced/brightness", "previous")
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.USERINIT, CONF_CONFIG_PATH "settings/advanced/user_init", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.DPADSWAP, CONF_CONFIG_PATH "settings/advanced/dpad_swap", 1)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.OVERDRIVE, CONF_CONFIG_PATH "settings/advanced/overdrive", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.SWAPFILE, CONF_CONFIG_PATH "settings/advanced/swapfile", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.ZRAMFILE, CONF_CONFIG_PATH "settings/advanced/zramfile", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LIDSWITCH, CONF_CONFIG_PATH "settings/advanced/lidswitch", 1)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.DISPSUSPEND, CONF_CONFIG_PATH "settings/advanced/disp_suspend", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.INCBRIGHT, CONF_CONFIG_PATH "settings/advanced/incbright", 16)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.INCVOLUME, CONF_CONFIG_PATH "settings/advanced/incvolume", 8)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.MAXGPU, CONF_CONFIG_PATH "settings/advanced/maxgpu", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.AUDIOREADY, CONF_CONFIG_PATH "settings/advanced/audio_ready", 0)

    CFG_INT_FIELD(config->SETTINGS.GENERAL.HIDDEN, CONF_CONFIG_PATH "settings/general/hidden", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.SOUND, CONF_CONFIG_PATH "settings/general/sound", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.CHIME, CONF_CONFIG_PATH "settings/general/chime", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BGM, CONF_CONFIG_PATH "settings/general/bgm", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BGMVOL, CONF_CONFIG_PATH "settings/general/bgmvol", 35)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.COLOUR, CONF_CONFIG_PATH "settings/general/colour", 32)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BRIGHTNESS, CONF_CONFIG_PATH "settings/general/brightness", 90)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.VOLUME, CONF_CONFIG_PATH "settings/general/volume", 75)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.RGB, CONF_CONFIG_PATH "settings/general/rgb", 1)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.HKDPAD, CONF_CONFIG_PATH "settings/hotkey/dpad_toggle", 1)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.HKSHOT, CONF_CONFIG_PATH "settings/hotkey/screenshot", 0)
    CFG_STR_FIELD(config->SETTINGS.GENERAL.STARTUP, CONF_CONFIG_PATH "settings/general/startup", "launcher")
    CFG_STR_FIELD(config->SETTINGS.GENERAL.LANGUAGE, CONF_CONFIG_PATH "settings/general/language", "English")
    CFG_INT_FIELD(config->SETTINGS.GENERAL.THEME_RESOLUTION, CONF_CONFIG_PATH "settings/general/theme_resolution", 0)

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
    }

    CFG_INT_FIELD(config->SETTINGS.HDMI.RESOLUTION, CONF_CONFIG_PATH "settings/hdmi/resolution", 0)
    CFG_INT_FIELD(config->SETTINGS.HDMI.SPACE, CONF_CONFIG_PATH "settings/hdmi/space", 0)
    CFG_INT_FIELD(config->SETTINGS.HDMI.DEPTH, CONF_CONFIG_PATH "settings/hdmi/depth", 0)
    CFG_INT_FIELD(config->SETTINGS.HDMI.RANGE, CONF_CONFIG_PATH "settings/hdmi/range", 0)
    CFG_INT_FIELD(config->SETTINGS.HDMI.SCAN, CONF_CONFIG_PATH "settings/hdmi/scan", 0)
    CFG_INT_FIELD(config->SETTINGS.HDMI.AUDIO, CONF_CONFIG_PATH "settings/hdmi/audio", 0)

    CFG_INT_FIELD(config->SETTINGS.NETWORK.MONITOR, CONF_CONFIG_PATH "settings/network/monitor", 0)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.BOOT, CONF_CONFIG_PATH "settings/network/boot", 1)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.WAKE, CONF_CONFIG_PATH "settings/network/wake", 1)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.COMPAT, CONF_CONFIG_PATH "settings/network/compat", 0)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.ASYNCLOAD, CONF_CONFIG_PATH "settings/network/async_load", 1)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.WAIT, CONF_CONFIG_PATH "settings/network/wait_timer", 5)
    CFG_INT_FIELD(config->SETTINGS.NETWORK.RETRY, CONF_CONFIG_PATH "settings/network/compat_retry", 1)

    CFG_INT_FIELD(config->SETTINGS.POWER.LOW_BATTERY, CONF_CONFIG_PATH "settings/power/low_battery", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.SHUTDOWN, CONF_CONFIG_PATH "settings/power/shutdown", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.IDLE.DISPLAY, CONF_CONFIG_PATH "settings/power/idle_display", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.IDLE.SLEEP, CONF_CONFIG_PATH "settings/power/idle_sleep", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.IDLE.MUTE, CONF_CONFIG_PATH "settings/power/idle_mute", 1)
    CFG_STR_FIELD(config->SETTINGS.POWER.GOV.DEFAULT, CONF_DEVICE_PATH "cpu/default", "ondemand")
    CFG_STR_FIELD(config->SETTINGS.POWER.GOV.IDLE, CONF_CONFIG_PATH "settings/power/gov_idle", "powersave")

    CFG_INT_FIELD(config->VISUAL.BATTERY, CONF_CONFIG_PATH "visual/battery", 1)
    CFG_INT_FIELD(config->VISUAL.NETWORK, CONF_CONFIG_PATH "visual/network", 0)
    CFG_INT_FIELD(config->VISUAL.BLUETOOTH, CONF_CONFIG_PATH "visual/bluetooth", 0)
    CFG_INT_FIELD(config->VISUAL.CLOCK, CONF_CONFIG_PATH "visual/clock", 1)
    CFG_INT_FIELD(config->VISUAL.OVERLAY_IMAGE, CONF_CONFIG_PATH "visual/overlayimage", 1)
    CFG_INT_FIELD(config->VISUAL.OVERLAY_TRANSPARENCY, CONF_CONFIG_PATH "visual/overlaytransparency", 85)
    CFG_INT_FIELD(config->VISUAL.GRID_MODE_CONTENT, CONF_CONFIG_PATH "visual/gridmodecontent", 0)
    CFG_INT_FIELD(config->VISUAL.BOX_ART, CONF_CONFIG_PATH "visual/boxart", 0)
    CFG_INT_FIELD(config->VISUAL.BOX_ART_ALIGN, CONF_CONFIG_PATH "visual/boxartalign", 0)
    CFG_INT_FIELD(config->VISUAL.BOX_ART_HIDE, CONF_CONFIG_PATH "visual/boxarthide", 0)
    CFG_INT_FIELD(config->VISUAL.NAME, CONF_CONFIG_PATH "visual/name", 0)
    CFG_INT_FIELD(config->VISUAL.DASH, CONF_CONFIG_PATH "visual/dash", 0)
    CFG_INT_FIELD(config->VISUAL.LAUNCH_SWAP, CONF_CONFIG_PATH "visual/launch_swap", 0)
    CFG_INT_FIELD(config->VISUAL.SHUFFLE, CONF_CONFIG_PATH "visual/shuffle", 1)
    CFG_INT_FIELD(config->VISUAL.FRIENDLYFOLDER, CONF_CONFIG_PATH "visual/friendlyfolder", 1)
    CFG_INT_FIELD(config->VISUAL.THETITLEFORMAT, CONF_CONFIG_PATH "visual/thetitleformat", 0)
    CFG_INT_FIELD(config->VISUAL.TITLEINCLUDEROOTDRIVE, CONF_CONFIG_PATH "visual/titleincluderootdrive", 0)
    CFG_INT_FIELD(config->VISUAL.FOLDERITEMCOUNT, CONF_CONFIG_PATH "visual/folderitemcount", 0)
    CFG_INT_FIELD(config->VISUAL.FOLDEREMPTY, CONF_CONFIG_PATH "visual/folderempty", 0)
    CFG_INT_FIELD(config->VISUAL.COUNTERFOLDER, CONF_CONFIG_PATH "visual/counterfolder", 1)
    CFG_INT_FIELD(config->VISUAL.COUNTERFILE, CONF_CONFIG_PATH "visual/counterfile", 1)
    CFG_INT_FIELD(config->VISUAL.BACKGROUNDANIMATION, CONF_CONFIG_PATH "visual/backgroundanimation", 0)
    CFG_INT_FIELD(config->VISUAL.LAUNCHSPLASH, CONF_CONFIG_PATH "visual/launchsplash", 0)
    CFG_INT_FIELD(config->VISUAL.BLACKFADE, CONF_CONFIG_PATH "visual/blackfade", 1)
    CFG_INT_FIELD(config->VISUAL.CONTENTCOLLECT, CONF_CONFIG_PATH "visual/contentcollect", 0)
    CFG_INT_FIELD(config->VISUAL.CONTENTHISTORY, CONF_CONFIG_PATH "visual/contenthistory", 0)

    CFG_INT_FIELD(config->WEB.SSHD, CONF_CONFIG_PATH "web/sshd", 1)
    CFG_INT_FIELD(config->WEB.SFTPGO, CONF_CONFIG_PATH "web/sftpgo", 0)
    CFG_INT_FIELD(config->WEB.TTYD, CONF_CONFIG_PATH "web/ttyd", 0)
    CFG_INT_FIELD(config->WEB.SYNCTHING, CONF_CONFIG_PATH "web/syncthing", 0)
    CFG_INT_FIELD(config->WEB.NTP, CONF_CONFIG_PATH "web/ntp", 1)
    CFG_INT_FIELD(config->WEB.TAILSCALED, CONF_CONFIG_PATH "web/tailscaled", 0)

    CFG_INT_FIELD(config->DANGER.VMSWAP, CONF_CONFIG_PATH "danger/vmswap", 8)
    CFG_INT_FIELD(config->DANGER.DIRTYRATIO, CONF_CONFIG_PATH "danger/dirty_ratio", 16)
    CFG_INT_FIELD(config->DANGER.DIRTYBACK, CONF_CONFIG_PATH "danger/dirty_back_ratio", 4)
    CFG_INT_FIELD(config->DANGER.CACHE, CONF_CONFIG_PATH "danger/cache_pressure", 64)
    CFG_INT_FIELD(config->DANGER.MERGE, CONF_CONFIG_PATH "danger/nomerges", 0)
    CFG_INT_FIELD(config->DANGER.REQUESTS, CONF_CONFIG_PATH "danger/nr_requests", 128)
    CFG_INT_FIELD(config->DANGER.READAHEAD, CONF_CONFIG_PATH "danger/read_ahead", 4096)
    CFG_INT_FIELD(config->DANGER.PAGECLUSTER, CONF_CONFIG_PATH "danger/page_cluster", 3)
    CFG_INT_FIELD(config->DANGER.TIMESLICE, CONF_CONFIG_PATH "danger/time_slice", 10)
    CFG_INT_FIELD(config->DANGER.IOSTATS, CONF_CONFIG_PATH "danger/iostats", 0)
    CFG_INT_FIELD(config->DANGER.IDLEFLUSH, CONF_CONFIG_PATH "danger/idle_flush", 0)
    CFG_INT_FIELD(config->DANGER.CHILDFIRST, CONF_CONFIG_PATH "danger/child_first", 0)
    CFG_INT_FIELD(config->DANGER.TUNESCALE, CONF_CONFIG_PATH "danger/tune_scale", 1)
    CFG_STR_FIELD(config->DANGER.CARDMODE, CONF_CONFIG_PATH "danger/cardmode", "noop")
    CFG_STR_FIELD(config->DANGER.STATE, CONF_CONFIG_PATH "danger/state", "mem")
}
