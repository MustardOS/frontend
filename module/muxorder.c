#include "muxshare.h"
#include "../common/ui/orientation.h"

static lv_obj_t *ui_objects_label[order_count];
static lv_obj_t *ui_objects_value[order_count];
static lv_obj_t *ui_objects_glyph[order_count];
static lv_obj_t *ui_objects_panel[order_count];

static char order_dir[MAX_BUFFER_SIZE];

typedef enum { scope_directory = 0, scope_global, scope_count } scope_opt;

static mux_dialogue scope_dlg;

static void show_scope_dialog(void) {
    dialogue_open_at(&scope_dlg, &theme, order_scope_directory ? scope_directory : scope_global);
}

static void refresh_rows(void) {
    for (int i = 0; i < order_count; i++) {
        const order_method method = (order_method) i;
        const char *variant = order_variant_name(method, order_variants[method]);

        char value[MAX_BUFFER_SIZE];
        if (method == order_active) {
            snprintf(value, sizeof(value), "%s - %s", variant, lang.muxorder.active);
        } else {
            snprintf(value, sizeof(value), "%s", variant);
        }

        lv_label_set_text(ui_objects_value[i], value);
    }
}

static void add_order_item(const int index) {
    const order_method method = (order_method) index;

    lv_obj_t *ui_pnl_item = lv_obj_create(ui_pnl_content);
    lv_obj_t *ui_lbl_item = lv_label_create(ui_pnl_item);
    lv_obj_t *ui_lbl_item_glyph = lv_img_create(ui_pnl_item);
    lv_obj_t *ui_lbl_item_value = lv_label_create(ui_pnl_item);

    apply_theme_list_panel(ui_pnl_item);
    apply_theme_option_item_label(&theme, ui_lbl_item, order_method_name(method), 1);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, order_method_glyph(method));
    apply_theme_list_value(&theme, ui_lbl_item_value, "");

    ui_objects_label[index] = ui_lbl_item;
    ui_objects_value[index] = ui_lbl_item_value;
    ui_objects_glyph[index] = ui_lbl_item_glyph;
    ui_objects_panel[index] = ui_pnl_item;
}

static void init_navigation_group(void) {
    reset_ui_groups();

    ui_count_static = order_count;

    for (int i = 0; i < order_count; i++) {
        add_order_item(i);
    }

    add_ui_groups(ui_objects_label, ui_objects_value, ui_objects_glyph, ui_objects_panel, 1);
    refresh_rows();
}

static void leave_module(void) {
    load_mux("explore");
    mux_input_stop();
}

static void handle_a(void) {
    if (dialogue_active(&scope_dlg)) {
        const scope_opt opt = (scope_opt) scope_dlg.selected;
        dialogue_dismiss(&scope_dlg);

        order_save(order_dir, opt == scope_directory);

        toast_message(lang.generic.saving, tst_wait_f);

        leave_module();
        return;
    }

    if (msgbox_active) return;

    order_active = (order_method) current_item_index;
    refresh_rows();

    play_sound(snd_confirm);
    show_scope_dialog();
}

static void handle_y(void) {
    if (msgbox_active || dialogue_active(&scope_dlg)) return;

    order_reset_defaults();

    play_sound(snd_confirm);
    refresh_rows();
}

static void cycle_variant(const int delta) {
    const order_method method = (order_method) current_item_index;
    const int total = order_variant_count(method);

    order_variants[method] = (order_variants[method] + delta + total) % total;

    play_sound(snd_navigate);
    refresh_rows();
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&scope_dlg)) {
        dialogue_mark_cancelled(&scope_dlg);
        dialogue_dismiss(&scope_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    leave_module();
}

static void handle_dpad_up(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad(&scope_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad(&scope_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad_hold(&scope_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad_hold(&scope_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    handle_list_nav_down_hold();
}

static void handle_dpad_left(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad(&scope_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    cycle_variant(-1);
}

static void handle_dpad_right(void) {
    if (dialogue_active(&scope_dlg)) {
        dialogue_handle_dpad(&scope_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    cycle_variant(+1);
}

static void handle_page_up(void) {
    if (dialogue_active(&scope_dlg) || msgbox_active) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (dialogue_active(&scope_dlg) || msgbox_active) return;

    handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call || dialogue_active(&scope_dlg)) return;

    play_sound(snd_info_open);
    show_info_box(lang.muxorder.title, lang.muxorder.help, 0);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.muxorder.reset, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxorder_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxorder.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    char *dir = read_all_char_from(ORDER_DIR_FROM);
    snprintf(order_dir, sizeof(order_dir), "%s", dir ? dir : "");
    free(dir);

    order_load(order_dir);

    init_navigation_group();

    lv_obj_update_layout(ui_pnl_content);
    gen_step_movement(0, +1, 2, 0, 1);

    const char *scope_options[scope_count] = {lang.generic.scope_directory, lang.muxorder.scope_global};
    dialogue_init(
        &scope_dlg, &theme, ui_screen, lang.muxorder.scope, NULL, scope_options, scope_count, lang.generic.select,
        lang.generic.cancel
    );

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_page_up,
            [mux_input_r1] = handle_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxorder.title, lang.muxorder.overview);

    mux_input_task(&input_opts);

    return 0;
}
