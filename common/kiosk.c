#include "options.h"
#include "kiosk.h"
#include "init.h"
#include "../module/muxshare.h"

void load_kiosk(struct mux_kiosk *kiosk) {
    char buffer[MAX_BUFFER_SIZE];

    CFG_INT_FIELD(kiosk->enable, CONF_KIOSK_PATH "enable", 0);
    CFG_INT_FIELD(kiosk->message, CONF_KIOSK_PATH "message", 0);

    CFG_INT_FIELD(kiosk->application.archive, CONF_KIOSK_PATH "application/archive", 0);
    CFG_INT_FIELD(kiosk->application.task, CONF_KIOSK_PATH "application/task", 0);

    CFG_INT_FIELD(kiosk->config.customisation, CONF_KIOSK_PATH "config/custom", 0);
    CFG_INT_FIELD(kiosk->config.language, CONF_KIOSK_PATH "config/language", 0);
    CFG_INT_FIELD(kiosk->config.network, CONF_KIOSK_PATH "config/network", 0);
    CFG_INT_FIELD(kiosk->config.storage, CONF_KIOSK_PATH "config/storage", 0);
    CFG_INT_FIELD(kiosk->config.backup, CONF_KIOSK_PATH "config/backup", 0);
    CFG_INT_FIELD(kiosk->config.net_settings, CONF_KIOSK_PATH "config/netadv", 0);
    CFG_INT_FIELD(kiosk->config.proxy, CONF_KIOSK_PATH "config/net_proxy", 0);
    CFG_INT_FIELD(kiosk->config.web_services, CONF_KIOSK_PATH "config/webserv", 0);

    CFG_INT_FIELD(kiosk->content.core, CONF_KIOSK_PATH "content/core", 0);
    CFG_INT_FIELD(kiosk->content.governor, CONF_KIOSK_PATH "content/governor", 0);
    CFG_INT_FIELD(kiosk->content.control, CONF_KIOSK_PATH "content/control", 0);
    CFG_INT_FIELD(kiosk->content.tag, CONF_KIOSK_PATH "content/tag", 0);
    CFG_INT_FIELD(kiosk->content.option, CONF_KIOSK_PATH "content/option", 0);
    CFG_INT_FIELD(kiosk->content.retroarch, CONF_KIOSK_PATH "content/retroarch", 0);
    CFG_INT_FIELD(kiosk->content.colfilter, CONF_KIOSK_PATH "content/colfilter", 0);
    CFG_INT_FIELD(kiosk->content.shader, CONF_KIOSK_PATH "content/shader", 0);
    CFG_INT_FIELD(kiosk->content.remconfig, CONF_KIOSK_PATH "content/remconfig", 0);
    CFG_INT_FIELD(kiosk->content.search, CONF_KIOSK_PATH "content/search", 0);
    CFG_INT_FIELD(kiosk->content.history, CONF_KIOSK_PATH "content/history", 0);

    CFG_INT_FIELD(kiosk->collect.add_con, CONF_KIOSK_PATH "collect/add_con", 0);
    CFG_INT_FIELD(kiosk->collect.new_dir, CONF_KIOSK_PATH "collect/new_dir", 0);
    CFG_INT_FIELD(kiosk->collect.remove, CONF_KIOSK_PATH "collect/remove", 0);
    CFG_INT_FIELD(kiosk->collect.access, CONF_KIOSK_PATH "collect/access", 0);

    CFG_INT_FIELD(kiosk->custom.catalogue, CONF_KIOSK_PATH "custom/catalogue", 0);
    CFG_INT_FIELD(kiosk->custom.raconfig, CONF_KIOSK_PATH "custom/raconfig", 0);
    CFG_INT_FIELD(kiosk->custom.theme, CONF_KIOSK_PATH "custom/theme", 0);
    CFG_INT_FIELD(kiosk->custom.theme_down, CONF_KIOSK_PATH "custom/theme_down", 0);

    CFG_INT_FIELD(kiosk->datetime.clock, CONF_KIOSK_PATH "datetime/clock", 0);
    CFG_INT_FIELD(kiosk->datetime.timezone, CONF_KIOSK_PATH "datetime/timezone", 0);

    CFG_INT_FIELD(kiosk->launch.application, CONF_KIOSK_PATH "launch/apps", 0);
    CFG_INT_FIELD(kiosk->launch.configuration, CONF_KIOSK_PATH "launch/config", 0);
    CFG_INT_FIELD(kiosk->launch.explore, CONF_KIOSK_PATH "launch/explore", 0);
    CFG_INT_FIELD(kiosk->launch.collection, CONF_KIOSK_PATH "launch/collection", 0);
    CFG_INT_FIELD(kiosk->launch.history, CONF_KIOSK_PATH "launch/history", 0);
    CFG_INT_FIELD(kiosk->launch.information, CONF_KIOSK_PATH "launch/info", 0);

    CFG_INT_FIELD(kiosk->setting.advanced, CONF_KIOSK_PATH "setting/advanced", 0);
    CFG_INT_FIELD(kiosk->setting.rgb, CONF_KIOSK_PATH "setting/rgb", 0);
    CFG_INT_FIELD(kiosk->setting.general, CONF_KIOSK_PATH "setting/general", 0);
    CFG_INT_FIELD(kiosk->setting.hdmi, CONF_KIOSK_PATH "setting/hdmi", 0);
    CFG_INT_FIELD(kiosk->setting.power, CONF_KIOSK_PATH "setting/power", 0);
    CFG_INT_FIELD(kiosk->setting.visual, CONF_KIOSK_PATH "setting/visual", 0);
    CFG_INT_FIELD(kiosk->setting.overlay, CONF_KIOSK_PATH "setting/overlay", 0);
}

void kiosk_denied(void) {
    if (is_ksk(kiosk.message)) {
        play_sound(snd_error);
        toast_message(lang.generic.kiosk_disable, tst_wait_m);
        refresh_screen(ui_screen, 1);
    }
}
