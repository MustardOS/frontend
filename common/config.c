#include "common.h"
#include "options.h"
#include "config.h"

void load_config(struct mux_config *config) {
    config->BOOT.FACTORY_RESET = atoi(read_text_from_file("/run/muos/global/boot/factory_reset"));

    config->CLOCK.NOTATION = atoi(read_text_from_file("/run/muos/global/clock/notation"));
    strncpy(config->CLOCK.POOL, read_text_from_file("/run/muos/global/clock/pool"), MAX_BUFFER_SIZE - 1);
    config->CLOCK.POOL[MAX_BUFFER_SIZE - 1] = '\0';

    config->NETWORK.ENABLED = atoi(read_text_from_file("/run/muos/global/network/enabled"));
    config->NETWORK.TYPE = atoi(read_text_from_file("/run/muos/global/network/type"));
    strncpy(config->NETWORK.INTERFACE, read_text_from_file("/run/muos/global/network/interface"), MAX_BUFFER_SIZE - 1);
    strncpy(config->NETWORK.SSID, read_text_from_file("/run/muos/global/network/ssid"), MAX_BUFFER_SIZE - 1);
    strncpy(config->NETWORK.ADDRESS, read_text_from_file("/run/muos/global/network/address"), MAX_BUFFER_SIZE - 1);
    strncpy(config->NETWORK.GATEWAY, read_text_from_file("/run/muos/global/network/gateway"), MAX_BUFFER_SIZE - 1);
    strncpy(config->NETWORK.SUBNET, read_text_from_file("/run/muos/global/network/subnet"), MAX_BUFFER_SIZE - 1);
    strncpy(config->NETWORK.DNS, read_text_from_file("/run/muos/global/network/dns"), MAX_BUFFER_SIZE - 1);
    config->NETWORK.INTERFACE[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.SSID[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.ADDRESS[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.GATEWAY[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.SUBNET[MAX_BUFFER_SIZE - 1] = '\0';
    config->NETWORK.DNS[MAX_BUFFER_SIZE - 1] = '\0';

    config->SETTINGS.GENERAL.HIDDEN = atoi(read_text_from_file("/run/muos/global/settings/general/hidden"));
    config->SETTINGS.GENERAL.SOUND = atoi(read_text_from_file("/run/muos/global/settings/general/sound"));
    config->SETTINGS.GENERAL.BGM = atoi(read_text_from_file("/run/muos/global/settings/general/bgm"));
    config->SETTINGS.GENERAL.POWER = atoi(read_text_from_file("/run/muos/global/settings/general/power"));
    config->SETTINGS.GENERAL.LOW_BATTERY = atoi(read_text_from_file("/run/muos/global/settings/general/low_battery"));
    config->SETTINGS.GENERAL.COLOUR = atoi(read_text_from_file("/run/muos/global/settings/general/colour"));
    config->SETTINGS.GENERAL.BRIGHTNESS = atoi(read_text_from_file("/run/muos/global/settings/general/brightness"));
    config->SETTINGS.GENERAL.HDMI = atoi(read_text_from_file("/run/muos/global/settings/general/hdmi"));
    config->SETTINGS.GENERAL.SHUTDOWN = atoi(read_text_from_file("/run/muos/global/settings/general/shutdown"));
    strncpy(config->SETTINGS.GENERAL.STARTUP, read_text_from_file("/run/muos/global/settings/general/startup"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.GENERAL.STARTUP[MAX_BUFFER_SIZE - 1] = '\0';
    strncpy(config->SETTINGS.GENERAL.LANGUAGE, read_text_from_file("/run/muos/global/settings/general/language"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.GENERAL.LANGUAGE[MAX_BUFFER_SIZE - 1] = '\0';

    config->SETTINGS.ADVANCED.SWAP = atoi(read_text_from_file("/run/muos/global/settings/advanced/swap"));
    config->SETTINGS.ADVANCED.THERMAL = atoi(read_text_from_file("/run/muos/global/settings/advanced/thermal"));
    config->SETTINGS.ADVANCED.FONT = atoi(read_text_from_file("/run/muos/global/settings/advanced/font"));
    config->SETTINGS.ADVANCED.OFFSET = atoi(read_text_from_file("/run/muos/global/settings/advanced/offset"));
    config->SETTINGS.ADVANCED.LOCK = atoi(read_text_from_file("/run/muos/global/settings/advanced/lock"));
    config->SETTINGS.ADVANCED.LED = atoi(read_text_from_file("/run/muos/global/settings/advanced/led"));
    config->SETTINGS.ADVANCED.THEME = atoi(read_text_from_file("/run/muos/global/settings/advanced/random_theme"));
    config->SETTINGS.ADVANCED.RETROWAIT = atoi(read_text_from_file("/run/muos/global/settings/advanced/retrowait"));
    config->SETTINGS.ADVANCED.ANDROID = atoi(read_text_from_file("/run/muos/global/settings/advanced/android"));
    config->SETTINGS.ADVANCED.VERBOSE = atoi(read_text_from_file("/run/muos/global/settings/advanced/verbose"));
    strncpy(config->SETTINGS.ADVANCED.VOLUME, read_text_from_file("/run/muos/global/settings/advanced/volume"),
            MAX_BUFFER_SIZE - 1);
    strncpy(config->SETTINGS.ADVANCED.BRIGHTNESS, read_text_from_file("/run/muos/global/settings/advanced/brightness"),
            MAX_BUFFER_SIZE - 1);
    strncpy(config->SETTINGS.ADVANCED.STATE, read_text_from_file("/run/muos/global/settings/advanced/state"),
            MAX_BUFFER_SIZE - 1);
    config->SETTINGS.ADVANCED.VOLUME[MAX_BUFFER_SIZE - 1] = '\0';
    config->SETTINGS.ADVANCED.BRIGHTNESS[MAX_BUFFER_SIZE - 1] = '\0';
    config->SETTINGS.ADVANCED.STATE[MAX_BUFFER_SIZE - 1] = '\0';

    config->VISUAL.BATTERY = atoi(read_text_from_file("/run/muos/global/visual/battery"));
    config->VISUAL.NETWORK = atoi(read_text_from_file("/run/muos/global/visual/network"));
    config->VISUAL.BLUETOOTH = atoi(read_text_from_file("/run/muos/global/visual/bluetooth"));
    config->VISUAL.CLOCK = atoi(read_text_from_file("/run/muos/global/visual/clock"));
    config->VISUAL.BOX_ART = atoi(read_text_from_file("/run/muos/global/visual/boxart"));
    config->VISUAL.NAME = atoi(read_text_from_file("/run/muos/global/visual/name"));
    config->VISUAL.DASH = atoi(read_text_from_file("/run/muos/global/visual/dash"));
    config->VISUAL.COUNTERFOLDER = atoi(read_text_from_file("/run/muos/global/visual/counterfolder"));
    config->VISUAL.COUNTERFILE = atoi(read_text_from_file("/run/muos/global/visual/counterfile"));

    config->WEB.SHELL = atoi(read_text_from_file("/run/muos/global/web/shell"));
    config->WEB.BROWSER = atoi(read_text_from_file("/run/muos/global/web/browser"));
    config->WEB.TERMINAL = atoi(read_text_from_file("/run/muos/global/web/terminal"));
    config->WEB.SYNCTHING = atoi(read_text_from_file("/run/muos/global/web/syncthing"));
    config->WEB.RESILIO = atoi(read_text_from_file("/run/muos/global/web/resilio"));
    config->WEB.NTP = atoi(read_text_from_file("/run/muos/global/web/ntp"));

    config->STORAGE.BIOS = atoi(read_text_from_file("/run/muos/global/storage/bios"));
    config->STORAGE.CONFIG = atoi(read_text_from_file("/run/muos/global/storage/config"));
    config->STORAGE.CATALOGUE = atoi(read_text_from_file("/run/muos/global/storage/catalogue"));
    config->STORAGE.CONTENT = atoi(read_text_from_file("/run/muos/global/storage/content"));
    config->STORAGE.MUSIC = atoi(read_text_from_file("/run/muos/global/storage/music"));
    config->STORAGE.SAVE = atoi(read_text_from_file("/run/muos/global/storage/save"));
    config->STORAGE.SCREENSHOT = atoi(read_text_from_file("/run/muos/global/storage/screenshot"));
    config->STORAGE.THEME = atoi(read_text_from_file("/run/muos/global/storage/theme"));
}
