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
                char *args[] = { "./muxinfo"};
                muxinfo_main(1, args);
            } else if (strcmp(action, "archive") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "app");
                char *args[] = { "./muxarchive"};
                muxarchive_main(1, args);
            } else if (strcmp(action, "task") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "app");
                char *args[] = { "./muxtask"};
                muxtask_main(1, args);
            } else if (strcmp(action, "tweakgen") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxtweakgen"};
                muxtweakgen_main(1, args);
            } else if (strcmp(action, "connect") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxconnect"};
                muxconnect_main(1, args);
            } else if (strcmp(action, "custom") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxcustom"};
                muxcustom_main(1, args);
            } else if (strcmp(action, "network") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "connect");
                char *args[] = { "./muxnetwork"};
                muxnetwork_main(1, args);
            } else if (strcmp(action, "language") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxlanguage"};
                muxlanguage_main(1, args);
            } else if (strcmp(action, "webserv") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "connect");
                char *args[] = { "./muxwebserv"};
                muxwebserv_main(1, args);
            } else if (strcmp(action, "hdmi") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "tweakgen");
                char *args[] = { "./muxhdmi"};
                muxhdmi_main(1, args);
            } else if (strcmp(action, "rtc") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "tweakgen");
                char *args[] = { "./muxrtc"};
                muxrtc_main(1, args);
            } else if (strcmp(action, "storage") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxstorage"};
                muxstorage_main(1, args);
            } else if (strcmp(action, "power") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxpower"};
                muxpower_main(1, args);
            } else if (strcmp(action, "visual") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "config");
                char *args[] = { "./muxvisual"};
                muxvisual_main(1, args);
            } else if (strcmp(action, "net_profile") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "network");
                char *args[] = { "./muxnetprofile"};
                muxnetprofile_main(1, args);
            } else if (strcmp(action, "net_scan") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "network");
                char *args[] = { "./muxnetscan"};
                muxnetscan_main(1, args);
            } else if (strcmp(action, "timezone") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "rtc");
                char *args[] = { "./muxtimezone"};
                muxtimezone_main(1, args);
            } else if (strcmp(action, "screenshot") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                char *args[] = { "./muxshot"};
                muxshot_main(1, args);
            } else if (strcmp(action, "space") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                char *args[] = { "./muxspace"};
                muxspace_main(1, args);
            } else if (strcmp(action, "tester") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                char *args[] = { "./muxtester"};
                muxtester_main(1, args);
            } else if (strcmp(action, "system") == 0) {
                write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
                char *args[] = { "./muxsysinfo"};
                muxsysinfo_main(1, args);
            } else {
                printf("****muxfrontend launcher action: %s\n", action);
                char *args[] = { "./muxlaunch" };
                muxlaunch_main(1, args);
            }
        } else {
            printf("****muxfrontend launcher action no file: \n");
            char *args[] = { "./muxlaunch" };
            muxlaunch_main(1, args);            
        }
        cleanup_screen();        
    }

    return 0;
}
