#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "config.h"

void load_config(struct mux_config *config) {
    const char *base_path = "/run/muos/global";
    char buffer[MAX_BUFFER_SIZE];

#define CFG_INT_FIELD(field, path, def)                          \
    snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path);  \
    field = (int)({                                              \
        char *ep;                                                \
        long val = strtol(read_text_from_file(buffer), &ep, 10); \
        *ep ? def : val;                                         \
    });

#define CFG_STR_FIELD(field, path, def)                                      \
    snprintf(buffer, sizeof(buffer), "%s/%s", base_path, path);              \
    strncpy(field, read_text_from_file(buffer) ?: def, MAX_BUFFER_SIZE - 1); \
    field[MAX_BUFFER_SIZE - 1] = '\0';

    CFG_INT_FIELD(config->BOOT.FACTORY_RESET, "boot/factory_reset", 0)

    CFG_INT_FIELD(config->CLOCK.NOTATION, "clock/notation", 0)
    CFG_STR_FIELD(config->CLOCK.POOL, "clock/pool", "pool.ntp.org")

    CFG_INT_FIELD(config->NETWORK.ENABLED, "network/enabled", 0)
    CFG_INT_FIELD(config->NETWORK.TYPE, "network/type", 0)
    CFG_STR_FIELD(config->NETWORK.INTERFACE, "network/interface", "wlan0")
    CFG_STR_FIELD(config->NETWORK.SSID, "network/ssid", "")
    CFG_STR_FIELD(config->NETWORK.PASS, "network/pass", "")
    CFG_STR_FIELD(config->NETWORK.ADDRESS, "network/address", "192.168.0.123")
    CFG_STR_FIELD(config->NETWORK.GATEWAY, "network/gateway", "192.168.0.1")
    CFG_STR_FIELD(config->NETWORK.SUBNET, "network/subnet", "24")
    CFG_STR_FIELD(config->NETWORK.DNS, "network/dns", "1.1.1.1")

    CFG_INT_FIELD(config->SETTINGS.ADVANCED.ACCELERATE, "settings/advanced/accelerate", 96)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.SWAP, "settings/advanced/swap", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THERMAL, "settings/advanced/thermal", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.FONT, "settings/advanced/font", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.OFFSET, "settings/advanced/offset", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LOCK, "settings/advanced/lock", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.LED, "settings/advanced/led", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.THEME, "settings/advanced/random_theme", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RETROWAIT, "settings/advanced/retrowait", 0)
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.USBFUNCTION, "settings/advanced/usb_function", "none")
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.VERBOSE, "settings/advanced/verbose", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.RUMBLE, "settings/advanced/rumble", 0)
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.VOLUME, "settings/advanced/volume", "previous")
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.BRIGHTNESS, "settings/advanced/brightness", "previous")
    CFG_STR_FIELD(config->SETTINGS.ADVANCED.STATE, "settings/advanced/state", "mem")
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.HDMIOUTPUT, "settings/advanced/hdmi_output", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.USERINIT, "settings/advanced/user_init", 0)
    CFG_INT_FIELD(config->SETTINGS.ADVANCED.DPADSWAP, "settings/advanced/dpad_swap", 1)

    CFG_INT_FIELD(config->SETTINGS.GENERAL.HIDDEN, "settings/general/hidden", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.SOUND, "settings/general/sound", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BGM, "settings/general/bgm", 0)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.COLOUR, "settings/general/colour", 32)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.BRIGHTNESS, "settings/general/brightness", 96)
    CFG_INT_FIELD(config->SETTINGS.GENERAL.HDMI, "settings/general/hdmi", 0)
    CFG_STR_FIELD(config->SETTINGS.GENERAL.STARTUP, "settings/general/startup", "launcher")
    CFG_STR_FIELD(config->SETTINGS.GENERAL.LANGUAGE, "settings/general/language", "English")

    CFG_INT_FIELD(config->SETTINGS.POWER.LOW_BATTERY, "settings/power/low_battery", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.SHUTDOWN, "settings/power/shutdown", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.IDLE_DISPLAY, "settings/power/idle_display", 0)
    CFG_INT_FIELD(config->SETTINGS.POWER.IDLE_SLEEP, "settings/power/idle_sleep", 0)

    CFG_INT_FIELD(config->VISUAL.BATTERY, "visual/battery", 1)
    CFG_INT_FIELD(config->VISUAL.NETWORK, "visual/network", 0)
    CFG_INT_FIELD(config->VISUAL.BLUETOOTH, "visual/bluetooth", 0)
    CFG_INT_FIELD(config->VISUAL.CLOCK, "visual/clock", 1)
    CFG_INT_FIELD(config->VISUAL.BOX_ART, "visual/boxart", 0)
    CFG_INT_FIELD(config->VISUAL.BOX_ART_ALIGN, "visual/boxartalign", 0)
    CFG_INT_FIELD(config->VISUAL.NAME, "visual/name", 0)
    CFG_INT_FIELD(config->VISUAL.DASH, "visual/dash", 0)
    CFG_INT_FIELD(config->VISUAL.FRIENDLYFOLDER, "visual/friendlyfolder", 1)
    CFG_INT_FIELD(config->VISUAL.THETITLEFORMAT, "visual/thetitleformat", 0)
    CFG_INT_FIELD(config->VISUAL.TITLEINCLUDEROOTDRIVE, "visual/titleincluderootdrive", 0)
    CFG_INT_FIELD(config->VISUAL.FOLDERITEMCOUNT, "visual/folderitemcount", 0)
    CFG_INT_FIELD(config->VISUAL.FOLDEREMPTY, "visual/folderempty", 0)
    CFG_INT_FIELD(config->VISUAL.COUNTERFOLDER, "visual/counterfolder", 1)
    CFG_INT_FIELD(config->VISUAL.COUNTERFILE, "visual/counterfile", 1)
    CFG_INT_FIELD(config->VISUAL.BACKGROUNDANIMATION, "visual/backgroundanimation", 0)
    CFG_INT_FIELD(config->VISUAL.LAUNCHSPLASH, "visual/launchsplash", 0)
    CFG_INT_FIELD(config->VISUAL.BLACKFADE, "visual/blackfade", 1)

    CFG_INT_FIELD(config->WEB.SSHD, "web/sshd", 1)
    CFG_INT_FIELD(config->WEB.SFTPGO, "web/sftpgo", 0)
    CFG_INT_FIELD(config->WEB.TTYD, "web/ttyd", 0)
    CFG_INT_FIELD(config->WEB.SYNCTHING, "web/syncthing", 0)
    CFG_INT_FIELD(config->WEB.RSLSYNC, "web/rslsync", 0)
    CFG_INT_FIELD(config->WEB.NTP, "web/ntp", 1)
    CFG_INT_FIELD(config->WEB.TAILSCALED, "web/tailscaled", 0)

#undef CFG_INT_FIELD
#undef CFG_STR_FIELD
}
