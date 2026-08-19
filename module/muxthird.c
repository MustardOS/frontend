#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "../common/thirdparty.h"

static void list_nav_move(int steps, int direction);

static void add_library_item(const char *name, const char *version) {
    ui_count_static++;

    lv_obj_t *ui_pnl_item = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(ui_pnl_item);

    lv_obj_t *ui_lbl_item = lv_label_create(ui_pnl_item);
    apply_theme_option_item_label(&theme, ui_lbl_item, name, 1);

    lv_obj_t *ui_ico_item = lv_img_create(ui_pnl_item);
    apply_theme_list_glyph(&theme, ui_ico_item, mux_module, "third");

    lv_obj_t *ui_val_item = lv_label_create(ui_pnl_item);
    apply_theme_list_value(&theme, ui_val_item, version);

    lv_obj_set_user_data(ui_lbl_item, (void *) name);

    lv_group_add_obj(ui_group, ui_lbl_item);
    lv_group_add_obj(ui_group_value, ui_val_item);
    lv_group_add_obj(ui_group_glyph, ui_ico_item);
    lv_group_add_obj(ui_group_panel, ui_pnl_item);
}

static void init_navigation_group(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (size_t i = 0; i < A_SIZE(third_party_libs); i++) {
        add_library_item(third_party_libs[i].name, third_party_libs[i].version);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    }

    list_nav_move(0, +1);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, -1, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "third");

    mux_input_stop();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});

    overlay_display();
}

int muxthird_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxthird.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxthird.title, lang.muxthird.overview);

    mux_input_task(&input_opts);

    return 0;
}
