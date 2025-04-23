#include "./ui/ui_muxlaunch.h"
#include "muxshare.h"
#include "../lvgl/lvgl.h"
#include <unistd.h>
#include <limits.h>
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

static int exit_status = 0;
static int last_index = 0;
static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_sys[PATH_MAX];
static char forced_flag[PATH_MAX];

static char previous_module[MAX_BUFFER_SIZE];
static char splash_image_path[MAX_BUFFER_SIZE];

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

static void process_content_action(char *action, char *module) {
    if (!file_exist(ADD_MODE_WORK)) return;

    snprintf(rom_name, sizeof(rom_name), "%s", read_line_from_file(action, 1));
    snprintf(rom_dir, sizeof(rom_dir), "%s", read_line_from_file(action, 2));
    snprintf(rom_sys, sizeof(rom_sys), "%s", read_line_from_file(action, 3));
    snprintf(forced_flag, sizeof(forced_flag), "%s", read_line_from_file(action, 4));

    load_mux(module);
}

static void last_index_check(){
    last_index = 0;
	if (file_exist(MUOS_IDX_LOAD) && !file_exist(ADD_MODE_WORK)) {
        last_index = safe_atoi(read_line_from_file(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }
}

static void set_previous_module(char *module) {
    snprintf(previous_module, sizeof(previous_module), "%s", module);
}

static int set_splash_image_path(char *splash_image_name) {
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));
    const char *theme = theme_compat() ? STORAGE_THEME : INTERNAL_THEME;
    if ((snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s/%s.png", 
                theme, mux_dimension, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s.png", 
                theme, mux_dimension, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s/%s.png", 
                theme, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s.png", 
                theme, splash_image_name) >= 0 && file_exist(splash_image_path))
    ) return 1;

    return 0;
}

int main(int argc, char *argv[]) {
    setup_background_process();
    
    load_device(&device);
    load_config(&config);
    load_lang(&lang);
    init_theme(0, 0);
    init_display();
    
    while (1) {
        //Process content association and governor actions
        process_content_action(MUOS_ASS_LOAD, "assign");
        process_content_action(MUOS_GOV_LOAD, "governor");
        
        if (file_exist(MUOS_ACT_LOAD)){
            exit_status = 0;
            char *action = read_line_from_file(MUOS_ACT_LOAD, 1);

            if (strcmp(action, "reboot") == 0) {
                if (set_splash_image_path("reboot")) muxsplash_main(splash_image_path);
                safe_quit(0);
                break;
            } else if (strcmp(action, "shutdown") == 0) {
                if (set_splash_image_path("shutdown")) muxsplash_main(splash_image_path);
                safe_quit(0);
                break;
            } else if (strcmp(action, "assign") == 0) {
                muxassign_main(0, rom_name, rom_dir, rom_sys);
            } else if (strcmp(action, "governor") == 0) {
                muxgov_main(0, rom_name, rom_dir, rom_sys);
            } else if (strcmp(action, "explore") == 0) {
                last_index_check();
                char *explore_dir = read_line_from_file(EXPLORE_DIR, 1);
                muxassign_main(1, rom_name, explore_dir, "none");
                muxgov_main(1, rom_name, explore_dir, "none");
                load_mux("launcher");
                if (muxplore_main(last_index, explore_dir) == 1) {
                    safe_quit(0);
                    break;
                }
            } else if (strcmp(action, "info") == 0) {
                load_mux("launcher");
                muxinfo_main();
                set_previous_module("muxinfo");
            } else if (strcmp(action, "archive") == 0) {
                load_mux("app");
                muxarchive_main();
                set_previous_module("muxarchive");
            } else if (strcmp(action, "task") == 0) {
                load_mux("app");
                muxtask_main();
                set_previous_module("muxtask");
            } else if (strcmp(action, "tweakgen") == 0) {
                load_mux("config");
                muxtweakgen_main();
                set_previous_module("muxtweakgen");
            } else if (strcmp(action, "connect") == 0) {
                load_mux("config");
                muxconnect_main();
                set_previous_module("muxconnect");
            } else if (strcmp(action, "custom") == 0) {
                load_mux("config");
                muxcustom_main();
                set_previous_module("muxcustom");
            } else if (strcmp(action, "network") == 0) {
                load_mux("connect");
                muxnetwork_main();
                set_previous_module("muxnetwork");
            } else if (strcmp(action, "language") == 0) {
                load_mux("config");
                muxlanguage_main();
                set_previous_module("muxlanguage");
            } else if (strcmp(action, "webserv") == 0) {
                load_mux("connect");
                muxwebserv_main();
                set_previous_module("muxwebserv");
            } else if (strcmp(action, "hdmi") == 0) {
                load_mux("tweakgen");
                muxhdmi_main();
                set_previous_module("muxhdmi");
            } else if (strcmp(action, "rtc") == 0) {
                load_mux("tweakgen");
                muxrtc_main();
                set_previous_module("muxrtc");
            } else if (strcmp(action, "storage") == 0) {
                load_mux("config");
                muxstorage_main();
                set_previous_module("muxstorage");
            } else if (strcmp(action, "power") == 0) {
                load_mux("config");
                muxpower_main();
                set_previous_module("muxpower");
            } else if (strcmp(action, "visual") == 0) {
                load_mux("config");
                muxvisual_main();
                set_previous_module("muxvisual");
            } else if (strcmp(action, "net_profile") == 0) {
                load_mux("network");
                muxnetprofile_main();
                set_previous_module("muxnetprofile");
            } else if (strcmp(action, "net_scan") == 0) {
                load_mux("network");
                muxnetscan_main();
                set_previous_module("muxnetscan");
            } else if (strcmp(action, "timezone") == 0) {
                load_mux("rtc");
                muxtimezone_main();
                set_previous_module("muxtimezone");
            } else if (strcmp(action, "screenshot") == 0) {
                load_mux("info");
                muxshot_main();
                set_previous_module("muxshot");
            } else if (strcmp(action, "space") == 0) {
                load_mux("info");
                muxspace_main();
                set_previous_module("muxspace");
            } else if (strcmp(action, "tester") == 0) {
                load_mux("info");
                muxtester_main();
                set_previous_module("muxtester");
            } else if (strcmp(action, "system") == 0) {
                load_mux("info");
                muxsysinfo_main();
                set_previous_module("muxsysinfo");
            } else {
                printf("****muxfrontend launcher action: %s\n", action);
                muxlaunch_main();
                set_previous_module("muxlaunch");
            }
        } else {
            printf("****muxfrontend launcher action no file: \n");
            muxlaunch_main();
            set_previous_module("muxlaunch");
        }
        cleanup_screen();        
    }

    return 0;
}
