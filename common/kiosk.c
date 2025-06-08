#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "kiosk.h"

void load_kiosk(struct mux_kiosk *kiosk) {
    char buffer[MAX_BUFFER_SIZE];

    CFG_INT_FIELD(kiosk->ENABLE, CONF_KIOSK_PATH "enable", 0)

    CFG_INT_FIELD(kiosk->APPLICATION.ARCHIVE, CONF_KIOSK_PATH "application/archive", 0)
    CFG_INT_FIELD(kiosk->APPLICATION.TASK, CONF_KIOSK_PATH "application/task", 0)

    CFG_INT_FIELD(kiosk->CONFIG.CUSTOMISATION, CONF_KIOSK_PATH "config/custom", 0)
    CFG_INT_FIELD(kiosk->CONFIG.LANGUAGE, CONF_KIOSK_PATH "config/language", 0)
    CFG_INT_FIELD(kiosk->CONFIG.NETWORK, CONF_KIOSK_PATH "config/network", 0)
    CFG_INT_FIELD(kiosk->CONFIG.STORAGE, CONF_KIOSK_PATH "config/storage", 0)
    CFG_INT_FIELD(kiosk->CONFIG.BACKUP, CONF_KIOSK_PATH "config/backup", 0)
    CFG_INT_FIELD(kiosk->CONFIG.WEB_SERVICES, CONF_KIOSK_PATH "config/webserv", 0)

    CFG_INT_FIELD(kiosk->CONTENT.CORE, CONF_KIOSK_PATH "content/core", 0)
    CFG_INT_FIELD(kiosk->CONTENT.GOVERNOR, CONF_KIOSK_PATH "content/governor", 0)
    CFG_INT_FIELD(kiosk->CONTENT.TAG, CONF_KIOSK_PATH "content/tag", 0)
    CFG_INT_FIELD(kiosk->CONTENT.OPTION, CONF_KIOSK_PATH "content/option", 0)
    CFG_INT_FIELD(kiosk->CONTENT.RETROARCH, CONF_KIOSK_PATH "content/retroarch", 0)
    CFG_INT_FIELD(kiosk->CONTENT.SEARCH, CONF_KIOSK_PATH "content/search", 0)

    CFG_INT_FIELD(kiosk->CUSTOM.BOOTLOGO, CONF_KIOSK_PATH "custom/bootlogo", 0)
    CFG_INT_FIELD(kiosk->CUSTOM.CATALOGUE, CONF_KIOSK_PATH "custom/catalogue", 0)
    CFG_INT_FIELD(kiosk->CUSTOM.CONFIGURATION, CONF_KIOSK_PATH "custom/configuration", 0)
    CFG_INT_FIELD(kiosk->CUSTOM.THEME, CONF_KIOSK_PATH "custom/theme", 0)

    CFG_INT_FIELD(kiosk->DATETIME.CLOCK, CONF_KIOSK_PATH "datetime/clock", 0)
    CFG_INT_FIELD(kiosk->DATETIME.TIMEZONE, CONF_KIOSK_PATH "datetime/timezone", 0)

    CFG_INT_FIELD(kiosk->LAUNCH.APPLICATION, CONF_KIOSK_PATH "launch/application", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.CONFIGURATION, CONF_KIOSK_PATH "launch/config", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.EXPLORE, CONF_KIOSK_PATH "launch/explore", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.COLLECTION, CONF_KIOSK_PATH "launch/collection", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.HISTORY, CONF_KIOSK_PATH "launch/history", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.INFORMATION, CONF_KIOSK_PATH "launch/info", 0)

    CFG_INT_FIELD(kiosk->SETTING.ADVANCED, CONF_KIOSK_PATH "setting/advanced", 0)
    CFG_INT_FIELD(kiosk->SETTING.GENERAL, CONF_KIOSK_PATH "setting/general", 0)
    CFG_INT_FIELD(kiosk->SETTING.HDMI, CONF_KIOSK_PATH "setting/hdmi", 0)
    CFG_INT_FIELD(kiosk->SETTING.POWER, CONF_KIOSK_PATH "setting/power", 0)
    CFG_INT_FIELD(kiosk->SETTING.VISUAL, CONF_KIOSK_PATH "setting/visual", 0)
}
