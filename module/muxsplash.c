#include "muxshare.h"

int muxsplash_main(char *splash_image) {
    init_module("muxsplash");

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    lv_obj_add_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", splash_image);

    lv_img_set_src(ui_imgWall, init_wall);

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);

    refresh_screen(ui_screen);

    return 0;
}
