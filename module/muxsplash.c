#include "muxshare.h"

int muxsplash_main(char *splash_image, const int apply_recolour) {
    init_module(__func__);
    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", splash_image);

    lv_img_header_t wall_hdr;
    if (lv_img_decoder_get_info(init_wall, &wall_hdr) == LV_RES_OK) {
        lv_img_set_src(ui_img_wall, init_wall);
    } else {
        lv_img_set_src(ui_img_wall, &ui_img_blank);
    }

    if (apply_recolour && theme.image_list.recolour_alpha > 0) {
        lv_obj_set_style_img_recolor(ui_img_wall, lv_color_hex(theme.image_list.recolour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_img_wall, theme.image_list.recolour_alpha, MU_OBJ_MAIN_DEFAULT);
    }

    watermark(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);

    refresh_screen(ui_screen, 3);

    return 0;
}
