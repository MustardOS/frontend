#include <stdlib.h>
#include "common.h"
#include "options.h"
#include "kiosk.h"

void load_kiosk(struct mux_kiosk *kiosk) {
    char buffer[MAX_BUFFER_SIZE];

    CFG_INT_FIELD(kiosk->APPLICATION.ARCHIVE, "application/archive", 0)
    CFG_INT_FIELD(kiosk->APPLICATION.TASK, "application/task", 0)

    CFG_INT_FIELD(kiosk->CONFIG.CUSTOMISATION, "config/custom", 0)
    CFG_INT_FIELD(kiosk->CONFIG.LANGUAGE, "config/language", 0)
    CFG_INT_FIELD(kiosk->CONFIG.NETWORK, "config/network", 0)
    CFG_INT_FIELD(kiosk->CONFIG.STORAGE, "config/storage", 0)
    CFG_INT_FIELD(kiosk->CONFIG.WEB_SERVICES, "config/webserv", 0)

    CFG_INT_FIELD(kiosk->CONTENT.ASSIGN_CORE, "content/core", 0)
    CFG_INT_FIELD(kiosk->CONTENT.ASSIGN_GOVERNOR, "content/governor", 0)
    CFG_INT_FIELD(kiosk->CONTENT.OPTION, "content/option", 0)
    CFG_INT_FIELD(kiosk->CONTENT.RETROARCH, "content/retroarch", 0)
    CFG_INT_FIELD(kiosk->CONTENT.SEARCH, "content/search", 0)

    CFG_INT_FIELD(kiosk->CUSTOM.CATALOGUE, "custom/catalogue", 0)
    CFG_INT_FIELD(kiosk->CUSTOM.CONFIGURATION, "custom/configuration", 0)
    CFG_INT_FIELD(kiosk->CUSTOM.THEME, "custom/theme", 0)

    CFG_INT_FIELD(kiosk->DATETIME.CLOCK, "datetime/clock", 0)
    CFG_INT_FIELD(kiosk->DATETIME.TIMEZONE, "datetime/timezone", 0)

    CFG_INT_FIELD(kiosk->LAUNCH.APPLICATION, "launch/application", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.CONFIGURATION, "launch/config", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.EXPLORE, "launch/explore", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.COLLECTION, "launch/collection", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.HISTORY, "launch/history", 0)
    CFG_INT_FIELD(kiosk->LAUNCH.INFORMATION, "launch/info", 0)

    CFG_INT_FIELD(kiosk->SETTING.ADVANCED, "setting/advanced", 0)
    CFG_INT_FIELD(kiosk->SETTING.GENERAL, "setting/general", 0)
    CFG_INT_FIELD(kiosk->SETTING.HDMI, "setting/hdmi", 0)
    CFG_INT_FIELD(kiosk->SETTING.POWER, "setting/power", 0)
    CFG_INT_FIELD(kiosk->SETTING.VISUAL, "setting/visual", 0)
}
