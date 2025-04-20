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

#include "muxlaunch.h"
#include "muxplore.h"
#include "muxinfo.h"

void my_log_cb(const char * buf) {
    printf("%s", buf);
}

void cleanup_screen() {
    printf("delete timers\n");
    init_dispose();
    printf("[UI] Change to temp screen\n");
    lv_disp_load_scr(ui_screen_temp);

    if (ui_screen_container && lv_obj_is_valid(ui_screen_container)) {
        printf("[Delete screen container %p\n", ui_screen_container);
        lv_obj_del(ui_screen_container);
        ui_screen_container = NULL;
        printf("Screen deleted\n");
    } else {
        printf("[UI] Screen already null or invalid\n");
    }
    current_item_index = 0;
}

int main(int argc, char *argv[]) {
    printf("****muxfrontend argv[0]: %s\n", argv[0]);

    printf("****setup_background_process\n");
    setup_background_process();

    // lv_log_register_print_cb(my_log_cb);
    
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
