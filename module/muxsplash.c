#include "muxshare.h"

int muxsplash_main(char *splash_image, bool apply_recolour) {
    init_module(__func__);
    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    lv_obj_add_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", splash_image);

    lv_img_set_src(ui_imgWall, init_wall);
    if (apply_recolour && theme.IMAGE_LIST.RECOLOUR_ALPHA > 0) {
        lv_obj_set_style_img_recolor(ui_imgWall, lv_color_hex(theme.IMAGE_LIST.RECOLOUR), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_imgWall, theme.IMAGE_LIST.RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);
    }

    display_testing_message(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);

    refresh_screen(ui_screen);

    return 0;
}
