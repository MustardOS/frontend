#include "common.h"
#include "options.h"
#include "config.h"
#include "mini/mini.h"

void load_config(struct mux_config *config) {
    mini_t * muos_config = mini_try_load(MUOS_CONFIG_FILE);

    config->BOOT.FACTORY_RESET = get_ini_int(muos_config, "boot", "factory_reset", 0);

    config->CLOCK.NOTATION = get_ini_int(muos_config, "clock", "notation", 0);
    config->CLOCK.POOL = get_ini_string(muos_config, "clock", "pool");

    config->NETWORK.ENABLED = get_ini_int(muos_config, "network", "enabled", 0);
    config->NETWORK.INTERFACE = get_ini_string(muos_config, "network", "interface");
    config->NETWORK.TYPE = get_ini_int(muos_config, "network", "type", 0);
    config->NETWORK.SSID = get_ini_string(muos_config, "network", "ssid");
    config->NETWORK.ADDRESS = get_ini_string(muos_config, "network", "address");
    config->NETWORK.GATEWAY = get_ini_string(muos_config, "network", "gateway");
    config->NETWORK.SUBNET = get_ini_string(muos_config, "network", "subnet");
    config->NETWORK.DNS = get_ini_string(muos_config, "network", "dns");

    config->SETTINGS.GENERAL.HIDDEN = get_ini_int(muos_config, "settings.general", "hidden", 0);
    config->SETTINGS.GENERAL.SOUND = get_ini_int(muos_config, "settings.general", "sound", 0);
    config->SETTINGS.GENERAL.STARTUP = get_ini_string(muos_config, "settings.general", "startup");
    config->SETTINGS.GENERAL.POWER = get_ini_int(muos_config, "settings.general", "power", 0);
    config->SETTINGS.GENERAL.LOW_BATTERY = get_ini_int(muos_config, "settings.general", "low_battery", 0);
    config->SETTINGS.GENERAL.COLOUR = get_ini_int(muos_config, "settings.general", "colour", 9);
    config->SETTINGS.GENERAL.BRIGHTNESS = get_ini_int(muos_config, "settings.general", "brightness", 49);
    config->SETTINGS.GENERAL.HDMI = get_ini_int(muos_config, "settings.general", "hdmi", 0);

    config->SETTINGS.ADVANCED.SWAP = get_ini_int(muos_config, "settings.advanced", "swap", 0);
    config->SETTINGS.ADVANCED.THERMAL = get_ini_int(muos_config, "settings.advanced", "thermal", 0);
    config->SETTINGS.ADVANCED.FONT = get_ini_int(muos_config, "settings.advanced", "font", 1);
    config->SETTINGS.ADVANCED.VERBOSE = get_ini_int(muos_config, "settings.advanced", "verbose", 0);
    config->SETTINGS.ADVANCED.VOLUME_LOW = get_ini_int(muos_config, "settings.advanced", "volume_low", 0);
    config->SETTINGS.ADVANCED.OFFSET = get_ini_int(muos_config, "settings.advanced", "offset", 50);

    config->THEME.NAME = get_ini_string(muos_config, "theme", "name");

    config->VISUAL.BATTERY = get_ini_int(muos_config, "visual", "battery", 1);
    config->VISUAL.NETWORK = get_ini_int(muos_config, "visual", "network", 0);
    config->VISUAL.BLUETOOTH = get_ini_int(muos_config, "visual", "bluetooth", 0);
    config->VISUAL.CLOCK = get_ini_int(muos_config, "visual", "clock", 1);
    config->VISUAL.BOX_ART = get_ini_int(muos_config, "visual", "boxart", 1);

    config->WEB.SHELL = get_ini_int(muos_config, "web", "shell", 1);
    config->WEB.BROWSER = get_ini_int(muos_config, "web", "browser", 0);
    config->WEB.TERMINAL = get_ini_int(muos_config, "web", "terminal", 0);
    config->WEB.SYNCTHING = get_ini_int(muos_config, "web", "syncthing", 0);
    config->WEB.NTP = get_ini_int(muos_config, "web", "ntp", 1);

    mini_free(muos_config);
}
