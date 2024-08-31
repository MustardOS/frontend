#include "common.h"
#include "options.h"
#include "config.h"

void load_config(struct mux_config *config) {
    const char *base_path = "/run/muos/global";
    char buffer[MAX_BUFFER_SIZE];

#define CFG_INT_FIELD(field, path)                                  \
        snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path); \
        field = atoi(read_text_from_file(buffer));

#define CFG_STR_FIELD(field, path)                                        \
        snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path);       \
        strncpy(field, read_text_from_file(buffer), MAX_BUFFER_SIZE - 1); \
        field[MAX_BUFFER_SIZE - 1] = '\0';

    CFG_INT_FIELD(config->BOOT.FACTORY_RESET, "boot/factory_reset");

    CFG_INT_FIELD(config->CLOCK.NOTATION, "clock/notation");
    CFG_STR_FIELD(config->CLOCK.POOL, "clock/pool");

    CFG_INT_FIELD(config->NETWORK.ENABLED, "network/enabled");
    CFG_INT_FIELD(config->NETWORK.TYPE, "network/type");
    CFG_STR_FIELD(config->NETWORK.INTERFACE, "network/interface");
    CFG_STR_FIELD(config->NETWORK.SSID, "network/ssid");
    CFG_STR_FIELD(config->NETWORK.ADDRESS, "network/address");
    CFG_STR_FIELD(config->NETWORK.GATEWAY, "network/gateway");
    CFG_STR_FIELD(config->NETWORK.SUBNET, "network/subnet");
    CFG_STR_FIELD(config->NETWORK.DNS, "network/dns");

    CFG_INT_FIELD(config->SETTINGS.GENERAL.HIDDEN, "settings/general/hidden");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.SOUND, "settings/general/sound");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BGM, "settings/general/bgm");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.POWER, "settings/general/power");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.LOW_BATTERY, "settings/general/low_battery");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.COLOUR, "settings/general/colour");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BRIGHTNESS, "settings/general/brightness");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.HDMI, "settings/general/hdmi");
    CFG_INT_FIELD(config->SETTINGS.GENERAL.SHUTDOWN, "settings/general/shutdown");
    CFG_STR_FIELD(config->SETTINGS.GENERAL.STARTUP, "settings/general/startup");
    CFG_STR_FIELD(config->SETTINGS.GENERAL.LANGUAGE, "settings/general/language");

    CFG_INT_FIELD(config->SETTINGS.ADVANCED.SWAP, "settings/advanced/swap");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THERMAL, "settings/advanced/thermal");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.FONT, "settings/advanced/font");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.OFFSET, "settings/advanced/offset");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LOCK, "settings/advanced/lock");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LED, "settings/advanced/led");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THEME, "settings/advanced/random_theme");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RETROWAIT, "settings/advanced/retrowait");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.ANDROID, "settings/advanced/android");
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.VERBOSE, "settings/advanced/verbose");
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.VOLUME, "settings/advanced/volume");
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.BRIGHTNESS, "settings/advanced/brightness");
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.STATE, "settings/advanced/state");

    CFG_INT_FIELD(config->VISUAL.BATTERY, "visual/battery");
    CFG_INT_FIELD(config->VISUAL.NETWORK, "visual/network");
    CFG_INT_FIELD(config->VISUAL.BLUETOOTH, "visual/bluetooth");
    CFG_INT_FIELD(config->VISUAL.CLOCK, "visual/clock");
    CFG_INT_FIELD(config->VISUAL.BOX_ART, "visual/boxart");
    CFG_INT_FIELD(config->VISUAL.NAME, "visual/name");
    CFG_INT_FIELD(config->VISUAL.DASH, "visual/dash");
    CFG_INT_FIELD(config->VISUAL.THETITLEFORMAT, "visual/thetitleformat");
    CFG_INT_FIELD(config->VISUAL.COUNTERFOLDER, "visual/counterfolder");
    CFG_INT_FIELD(config->VISUAL.COUNTERFILE, "visual/counterfile");

    CFG_INT_FIELD(config->WEB.SHELL, "web/shell");
    CFG_INT_FIELD(config->WEB.BROWSER, "web/browser");
    CFG_INT_FIELD(config->WEB.TERMINAL, "web/terminal");
    CFG_INT_FIELD(config->WEB.SYNCTHING, "web/syncthing");
    CFG_INT_FIELD(config->WEB.RESILIO, "web/resilio");
    CFG_INT_FIELD(config->WEB.NTP, "web/ntp");

    CFG_INT_FIELD(config->STORAGE.BIOS, "storage/bios");
    CFG_INT_FIELD(config->STORAGE.CONFIG, "storage/config");
    CFG_INT_FIELD(config->STORAGE.CATALOGUE, "storage/catalogue");
    CFG_INT_FIELD(config->STORAGE.CONTENT, "storage/content");
    CFG_INT_FIELD(config->STORAGE.MUSIC, "storage/music");
    CFG_INT_FIELD(config->STORAGE.SAVE, "storage/save");
    CFG_INT_FIELD(config->STORAGE.SCREENSHOT, "storage/screenshot");
    CFG_INT_FIELD(config->STORAGE.THEME, "storage/theme");
    CFG_INT_FIELD(config->STORAGE.LANGUAGE, "storage/language");

#undef CFG_INT_FIELD
#undef CFG_STR_FIELD
}
