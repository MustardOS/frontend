#include "muxshare.h"
#include "ui/ui_muxdevice.h"

#define DEVICE(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(DEVICE_ELEMENTS) };
#undef DEVICE

#define DEVICE(NAME, UDATA) static int NAME##_original;
DEVICE_ELEMENTS
#undef DEVICE

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define DEVICE(NAME, UDATA) {UDATA, lang.muxdevice.help.NAME},
        DEVICE_ELEMENTS
#undef DEVICE
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define DEVICE(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_device);
    DEVICE_ELEMENTS
#undef DEVICE
}

static void restore_device_options(void) {
#define DEVICE(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_device, device.board.NAME);
    DEVICE_ELEMENTS
#undef DEVICE
}

static void save_device_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_DEV(device, has_bluetooth, "board/bluetooth", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_rgb, "led/rgb", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_debugfs, "board/debugfs", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_hdmi, "board/hdmi", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_lid, "board/lid", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_network, "board/network", INT, 0);
    CHECK_AND_SAVE_DEV(device, has_portmaster, "board/portmaster", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_device = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, device, has_bluetooth, lang.muxdevice.hasbluetooth, "bluetooth", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_rgb, lang.muxdevice.hasrgb, "rgb", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_debugfs, lang.muxdevice.hasdebugfs, "debugfs", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_hdmi, lang.muxdevice.hashdmi, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_lid, lang.muxdevice.haslid, "lid", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_network, lang.muxdevice.hasnetwork, "network", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, has_portmaster, lang.muxdevice.hasportmaster, "portmaster", disabled_enabled, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }
    play_sound(snd_back);

    save_device_options();

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

#define DEVICE(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_device, UDATA);
    DEVICE_ELEMENTS
#undef DEVICE

    overlay_display();
}

int muxdevice_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxdevice.title);
    init_muxdevice(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_device_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
