#include "common.h"
#include "options.h"
#include "config.h"
#include "mini/mini.h"

void load_config(struct mux_config *config) {
    static char config_file[MAX_BUFFER_SIZE];
    snprintf(config_file, sizeof(config_file),
             "%s/config/config.ini", INTERNAL_PATH);

    mini_t * muos_config = mini_try_load(config_file);

    config->BOOT.FACTORY_RESET = get_ini_int(muos_config, "boot", "factory_reset", 0);

    config->CLOCK.NOTATION = get_ini_int(muos_config, "clock", "notation", 0);
    strncpy(config->CLOCK.POOL, get_ini_string(muos_config, "clock", "pool", "pool.ntp.org"),
            MAX_BUFFER_SIZE - 1);
    config->CLOCK.POOL[MAX_BUFFER_SIZE - 1] = '\0';

    config->NETWORK.ENABLED = get_ini_int(muos_config, "network", "enabled", 0);
    strncpy(config->NETWORK.INTERFACE, get_ini_string(muos_config, "network", "interface", "wlan0"),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.INTERFACE[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.TYPE = get_ini_int(muos_config, "network", "type", 0);
    strncpy(config->NETWORK.SSID, get_ini_string(muos_config, "network", "ssid", ""),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.SSID[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->NETWORK.ADDRESS, get_ini_string(muos_config, "network", "address", "192.168.0.123"),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.ADDRESS[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->NETWORK.GATEWAY, get_ini_string(muos_config, "network", "gateway", "192.168.0.1"),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.GATEWAY[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->NETWORK.SUBNET, get_ini_string(muos_config, "network", "subnet", "24"),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.SUBNET[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->NETWORK.DNS, get_ini_string(muos_config, "network", "dns", "1.1.1.1"),
            MAX_BUFFER_SIZE - 1);
    config->NETWORK.DNS[MAX_BUFFER_SIZE - 1] = '\0';

    config->SETTINGS.GENERAL.HIDDEN = get_ini_int(muos_config, "settings.general", "hidden", 0);
    config->SETTINGS.GENERAL.SOUND = get_ini_int(muos_config, "settings.general", "sound", 0);
    config->SETTINGS.GENERAL.BGM = get_ini_int(muos_config, "settings.general", "bgm", 0);
    strncpy(config->SETTINGS.GENERAL.STARTUP, get_ini_string(muos_config, "settings.general", "startup", "launcher"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.GENERAL.STARTUP[MAX_BUFFER_SIZE - 1] = '\0';
    config->SETTINGS.GENERAL.POWER = get_ini_int(muos_config, "settings.general", "power", 0);
    config->SETTINGS.GENERAL.LOW_BATTERY = get_ini_int(muos_config, "settings.general", "low_battery", 0);
    config->SETTINGS.GENERAL.COLOUR = get_ini_int(muos_config, "settings.general", "colour", 9);
    config->SETTINGS.GENERAL.BRIGHTNESS = get_ini_int(muos_config, "settings.general", "brightness", 49);
    config->SETTINGS.GENERAL.HDMI = get_ini_int(muos_config, "settings.general", "hdmi", 0);
    config->SETTINGS.GENERAL.SHUTDOWN = get_ini_int(muos_config, "settings.general", "shutdown", -1);

    config->SETTINGS.ADVANCED.SWAP = get_ini_int(muos_config, "settings.advanced", "swap", 0);
    config->SETTINGS.ADVANCED.THERMAL = get_ini_int(muos_config, "settings.advanced", "thermal", 0);
    config->SETTINGS.ADVANCED.FONT = get_ini_int(muos_config, "settings.advanced", "font", 1);
    config->SETTINGS.ADVANCED.VERBOSE = get_ini_int(muos_config, "settings.advanced", "verbose", 0);
    strncpy(config->SETTINGS.ADVANCED.VOLUME, get_ini_string(muos_config, "settings.advanced", "volume", "previous"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.ADVANCED.VOLUME[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->SETTINGS.ADVANCED.BRIGHTNESS,
            get_ini_string(muos_config, "settings.advanced", "brightness", "previous"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.ADVANCED.BRIGHTNESS[MAX_BUFFER_SIZE - 1] = '\0';
    config->SETTINGS.ADVANCED.OFFSET = get_ini_int(muos_config, "settings.advanced", "offset", 50);
    config->SETTINGS.ADVANCED.LOCK = get_ini_int(muos_config, "settings.advanced", "lock", 0);
    config->SETTINGS.ADVANCED.LED = get_ini_int(muos_config, "settings.advanced", "led", 0);
    config->SETTINGS.ADVANCED.THEME = get_ini_int(muos_config, "settings.advanced", "random_theme", 0);
    config->SETTINGS.ADVANCED.RETROWAIT = get_ini_int(muos_config, "settings.advanced", "retrowait", 0);
    config->SETTINGS.ADVANCED.ANDROID = get_ini_int(muos_config, "settings.advanced", "android", 0);
    strncpy(config->SETTINGS.ADVANCED.STATE, get_ini_string(muos_config, "settings.advanced", "state", "mem"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.ADVANCED.STATE[MAX_BUFFER_SIZE - 1] = '\0';

    strncpy(config->THEME.NAME, get_ini_string(muos_config, "theme", "name", "muOS"),
            MAX_BUFFER_SIZE - 1);
    config->THEME.NAME[MAX_BUFFER_SIZE - 1] = '\0';

    config->VISUAL.BATTERY = get_ini_int(muos_config, "visual", "battery", 1);
    config->VISUAL.NETWORK = get_ini_int(muos_config, "visual", "network", 0);
    config->VISUAL.BLUETOOTH = get_ini_int(muos_config, "visual", "bluetooth", 0);
    config->VISUAL.CLOCK = get_ini_int(muos_config, "visual", "clock", 1);
    config->VISUAL.BOX_ART = get_ini_int(muos_config, "visual", "boxart", 1);
    config->VISUAL.NAME = get_ini_int(muos_config, "visual", "name", 0);
    config->VISUAL.DASH = get_ini_int(muos_config, "visual", "dash", 0);

    config->WEB.SHELL = get_ini_int(muos_config, "web", "shell", 1);
    config->WEB.BROWSER = get_ini_int(muos_config, "web", "browser", 0);
    config->WEB.TERMINAL = get_ini_int(muos_config, "web", "terminal", 0);
    config->WEB.SYNCTHING = get_ini_int(muos_config, "web", "syncthing", 0);
    config->WEB.NTP = get_ini_int(muos_config, "web", "ntp", 1);

    config->STORAGE.BIOS = get_ini_int(muos_config, "storage", "bios", 0);
    config->STORAGE.CONFIG = get_ini_int(muos_config, "storage", "config", 0);
    config->STORAGE.CATALOGUE = get_ini_int(muos_config, "storage", "catalogue", 0);
    config->STORAGE.FAV = get_ini_int(muos_config, "storage", "fav", 0);
    config->STORAGE.MUSIC = get_ini_int(muos_config, "storage", "music", 0);
    config->STORAGE.SAVE = get_ini_int(muos_config, "storage", "save", 0);
    config->STORAGE.SCREENSHOT = get_ini_int(muos_config, "storage", "screenshot", 0);
    config->STORAGE.THEME = get_ini_int(muos_config, "storage", "theme", 0);

    mini_free(muos_config);
}
