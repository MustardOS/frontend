#include "muxshare.h"

lv_obj_t *ui_imgButton;

static void handle_input(mux_input_type type, mux_input_action action) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];

    const char *glyph[MUX_INPUT_COUNT] = {
            // Gamepad buttons:
            [MUX_INPUT_A] = "btn_a",
            [MUX_INPUT_B] = "btn_b",
            [MUX_INPUT_C] = "btn_c",
            [MUX_INPUT_X] = "btn_x",
            [MUX_INPUT_Y] = "btn_y",
            [MUX_INPUT_Z] = "btn_z",
            [MUX_INPUT_L1] = "btn_l1",
            [MUX_INPUT_L2] = "btn_l2",
            [MUX_INPUT_L3] = "btn_l3",
            [MUX_INPUT_R1] = "btn_r1",
            [MUX_INPUT_R2] = "btn_r2",
            [MUX_INPUT_R3] = "btn_r3",
            [MUX_INPUT_SELECT] = "btn_select",
            [MUX_INPUT_START] = "btn_start",
            [MUX_INPUT_MENU_SHORT] = "btn_menu",
            [MUX_INPUT_MENU_LONG] = "btn_menu",

            // D-pad:
            [MUX_INPUT_DPAD_UP] = "dpad_up",
            [MUX_INPUT_DPAD_DOWN] = "dpad_down",
            [MUX_INPUT_DPAD_LEFT] = "dpad_left",
            [MUX_INPUT_DPAD_RIGHT] = "dpad_right",

            // Left stick:
            [MUX_INPUT_LS_UP] = "ls_up",
            [MUX_INPUT_LS_DOWN] = "ls_down",
            [MUX_INPUT_LS_LEFT] = "ls_left",
            [MUX_INPUT_LS_RIGHT] = "ls_right",

            // Right stick:
            [MUX_INPUT_RS_UP] = "rs_up",
            [MUX_INPUT_RS_DOWN] = "rs_down",
            [MUX_INPUT_RS_LEFT] = "rs_left",
            [MUX_INPUT_RS_RIGHT] = "rs_right",

            // Volume buttons:
            [MUX_INPUT_VOL_UP] = "vol_up",
            [MUX_INPUT_VOL_DOWN] = "vol_down",
    };

    switch (action) {
        case MUX_INPUT_PRESS:
            if (glyph[type]) {
                lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
                if (generate_image_embed(config.THEME.STORAGE_THEME, mux_dimension, mux_module, glyph[type], image_path,
                                         sizeof(image_path), image_embed, sizeof(image_embed)) ||
                    generate_image_embed(INTERNAL_THEME, mux_dimension, mux_module, glyph[type], image_path,
                                         sizeof(image_path), image_embed, sizeof(image_embed))) {
                }
                if (file_exist(image_path)) {
                    lv_img_set_src(ui_imgButton, image_embed);
                } else {
                    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
                }
            }
            break;
        case MUX_INPUT_RELEASE:
            lv_img_set_src(ui_imgButton, &ui_image_Nothing);
            break;
        case MUX_INPUT_HOLD:
            break;
    }
}

static void handle_quit(void) {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");

    close_input();
    mux_input_stop();
}

static void init_elements(void) {
    header_and_footer_setup();

    ui_imgButton = lv_img_create(ui_screen);
    lv_obj_set_align(ui_imgButton, LV_ALIGN_CENTER);
    lv_img_set_src(ui_imgButton, &ui_image_Nothing);
    lv_obj_set_style_img_recolor(ui_imgButton, lv_color_hex(theme.LIST_DEFAULT.GLYPH_RECOLOUR), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgButton, theme.LIST_DEFAULT.GLYPH_RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);

    lv_label_set_text(ui_lblMessage, config.SETTINGS.ADVANCED.SWAP ? lang.MUXTESTER.QUIT_ALT : lang.MUXTESTER.QUIT);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(ui_pnlMessage, -12);

    lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);

    overlay_display();
}

int muxtester_main(void) {
    const char *m = "muxtester";
    set_process_name(m);
    init_module(m);

    init_theme(0, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTESTER.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblScreenMessage, lang.MUXTESTER.ANY);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();

    init_timer(NULL, NULL);

    mux_input_options input_opts = {
            .input_handler = handle_input,
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_DPAD_DOWN) |
                                         BIT(config.SETTINGS.ADVANCED.SWAP ? MUX_INPUT_A : MUX_INPUT_B),
                            .press_handler = handle_quit,
                            .hold_handler = handle_quit,
                    }
            }
    };
    init_input(&input_opts, false);
    input_opts.stick_nav = false;
    mux_input_task(&input_opts);

    return 0;
}
