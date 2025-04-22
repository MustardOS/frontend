#include "./ui/ui_muxlaunch.h"
#include "muxshare.h"
#include "../lvgl/lvgl.h"
#include <unistd.h>
#include "../common/common.h"
#include "../common/init.h"
#include "../common/ui_common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/osk.h"

#include "muxapp.h"
#include "muxarchive.h"
#include "muxassign.h"
#include "muxcharge.h"
#include "muxcollect.h"
#include "muxconfig.h"
#include "muxconnect.h"
#include "muxcredits.h"
#include "muxcustom.h"
#include "muxgov.h"
#include "muxhdmi.h"
#include "muxhistory.h"
#include "muxinfo.h"
#include "muxlanguage.h"
#include "muxlaunch.h"
#include "muxnetprofile.h"
#include "muxnetscan.h"
#include "muxnetwork.h"
#include "muxoption.h"
#include "muxpass.h"
#include "muxpicker.h"
#include "muxpower.h"
#include "muxplore.h"
#include "muxrtc.h"
#include "muxsearch.h"
#include "muxshot.h"
#include "muxsnapshot.h"
#include "muxspace.h"
#include "muxsplash.h"
#include "muxstart.h"
#include "muxstorage.h"
#include "muxsysinfo.h"
#include "muxtask.h"
#include "muxtester.h"
#include "muxtimezone.h"
#include "muxtweakadv.h"
#include "muxtweakgen.h"
#include "muxvisual.h"
#include "muxwebserv.h"

static void cleanup_screen() {
    if (ui_screen_container == NULL) return;
    init_dispose();
    lv_disp_load_scr(ui_screen_temp);

    if (ui_screen_container && lv_obj_is_valid(ui_screen_container)) {
        lv_obj_del(ui_screen_container);
        ui_screen_container = NULL;
    }
    current_item_index = 0;
    first_open = 1;
    key_curr = 0;
    key_show = 0;
    msgbox_active = 0;
    nav_sound = 0;
}

int main(int argc, char *argv[]) {
    setup_background_process();
    
    load_device(&device);
    load_config(&config);
    load_lang(&lang);
    init_theme(0, 0);
    init_display();
    
    while (1) {
        printf("frontend loop\n");
        if (file_exist(MUOS_ACT_LOAD)){
            char *action = read_line_from_file(MUOS_ACT_LOAD, 1);
            char *explore_dir = read_line_from_file(EXPLORE_DIR, 1);
            char *last_index = read_line_from_file(MUOS_IDX_LOAD, 1);

            if (strcmp(action, "reboot") == 0) {
                run_exec((const char *[]) { "/opt/muos/script/mux/quit.sh", "reboot", "frontend" });
                break;
            } else if (strcmp(action, "shutdown") == 0) {
                run_exec((const char *[]) { "/opt/muos/script/mux/quit.sh", "poweroff", "frontend" });
                break;
            } else if (strcmp(action, "explore") == 0) {
                printf("****muxfrontend explore action: %s  explore_dir: %s   last_index: %s\n", action, explore_dir, last_index);
                char *args[] = { "./muxplore", "-d", explore_dir, "-i", last_index };
                muxplore_main(5, args);
            } else if (strcmp(action, "info") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "launcher");
                muxinfo_main();
            } else if (strcmp(action, "archive") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "app");
                muxarchive_main();
            } else if (strcmp(action, "task") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "app");
                muxtask_main();
            } else if (strcmp(action, "tweakgen") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxtweakgen_main();
            } else if (strcmp(action, "connect") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxconnect_main();
            } else if (strcmp(action, "custom") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxcustom_main();
            } else if (strcmp(action, "network") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "connect");
                muxnetwork_main();
            } else if (strcmp(action, "language") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxlanguage_main();
            } else if (strcmp(action, "webserv") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "connect");
                muxwebserv_main();
            } else if (strcmp(action, "hdmi") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "tweakgen");
                muxhdmi_main();
            } else if (strcmp(action, "rtc") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "tweakgen");
                muxrtc_main();
            } else if (strcmp(action, "storage") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxstorage_main();
            } else if (strcmp(action, "power") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxpower_main();
            } else if (strcmp(action, "visual") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                muxvisual_main();
            } else if (strcmp(action, "net_profile") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "network");
                muxnetprofile_main();
            } else if (strcmp(action, "net_scan") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "network");
                muxnetscan_main();
            } else if (strcmp(action, "timezone") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "rtc");
                muxtimezone_main();
            } else if (strcmp(action, "screenshot") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                muxshot_main();
            } else if (strcmp(action, "space") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                muxspace_main();
            } else if (strcmp(action, "tester") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                muxtester_main();
            } else if (strcmp(action, "system") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                muxsysinfo_main();
            } else {
                printf("****muxfrontend launcher action: %s\n", action);
                muxlaunch_main();
            }
        } else {
            printf("****muxfrontend launcher action no file: \n");
            muxlaunch_main();
        }
        cleanup_screen();        
    }

    return 0;
}
