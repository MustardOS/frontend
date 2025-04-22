#include "muxshare.h"
#include "muxstart.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxstart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"

int muxstart_main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <progress> <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    snprintf(mux_module, sizeof(mux_module), "muxstart");
    
            
    init_theme(0, 0);
    
    init_muxstart();

    lv_obj_set_user_data(ui_scrStart, mux_module);

    if (config.BOOT.FACTORY_RESET) {
        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

        char init_wall[MAX_BUFFER_SIZE];
        snprintf(init_wall, sizeof(init_wall), "%s/%simage/wall/muxstart.png",
                 INTERNAL_THEME, mux_dimension);
        if (!file_exist(init_wall)) {
            snprintf(init_wall, sizeof(init_wall), "%s/image/wall/muxstart.png",
                     INTERNAL_THEME);
        }

        char lv_wall[MAX_BUFFER_SIZE];
        snprintf(lv_wall, sizeof(lv_wall), "M:%s", init_wall);

        lv_img_set_src(ui_imgWall, strdup(lv_wall));
    } else {
        load_wallpaper(ui_scrStart, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    }

    load_font_text(ui_scrStart);

#if TEST_IMAGE
    display_testing_message(ui_scrStart);
#endif

    overlay_image = lv_img_create(ui_scrStart);
    load_overlay_image(ui_scrStart, overlay_image);

    int progress = safe_atoi(argv[1]);
    if (!progress) lv_obj_set_style_bg_opa(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_bar_set_value(ui_barProgress, progress, LV_ANIM_OFF);
    lv_label_set_text(ui_lblMessage, argv[2]);

    refresh_screen(ui_scrStart);

    return 0;
}
