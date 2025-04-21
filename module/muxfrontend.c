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
    init_dispose();
    lv_disp_load_scr(ui_screen_temp);

    if (ui_screen_container && lv_obj_is_valid(ui_screen_container)) {
        lv_obj_del(ui_screen_container);
        ui_screen_container = NULL;
    }
    current_item_index = 0;
    first_open = 1;
}

static int main(int argc, char *argv[]) {
    printf("****muxfrontend argv[0]: %s\n", argv[0]);

    printf("****setup_background_process\n");
    
    // lv_log_register_print_cb(my_log_cb);
    
                init_theme(0, 0);
    
    while (1) {
        printf("frontend loop\n");
        if (file_exist(MUOS_ACT_LOAD)){
            char *action = read_line_from_file(MUOS_ACT_LOAD, 1);
            char *explore_dir = read_line_from_file(EXPLORE_DIR, 1);
            char *last_index = read_line_from_file(MUOS_IDX_LOAD, 1);

            if (strcmp(action, "reboot") == 0 || strcmp(action, "shutdown") == 0) {
                printf("****muxfrontend reboot or shutdown action: %s\n", action);
                break;
            } else if (strcmp(action, "explore") == 0) {
                printf("****muxfrontend explore action: %s  explore_dir: %s   last_index: %s\n", action, explore_dir, last_index);
                char *args[] = { "./muxplore", "-d", explore_dir, "-i", last_index };
                muxplore_main(5, args);
            } else if (strcmp(action, "info") == 0) {
                printf("****muxfrontend info action: %s  explore_dir: %s   last_index: %s\n", action, explore_dir, last_index);
                char *args[] = { "./muxinfo"};
                muxinfo_main(1, args);
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
