#include "muxshare.h"
#include "ui/ui_muxdevice.h"

#define UI_COUNT 7

#define DEVICE(NAME, ENUM, UDATA) static int NAME##_original;
DEVICE_ELEMENTS
#undef DEVICE

static void show_help() {
    struct help_msg help_messages[] = {
#define DEVICE(NAME, ENUM, UDATA) { ui_lbl##NAME##_device, lang.MUXDEVICE.HELP.ENUM },
            DEVICE_ELEMENTS
#undef DEVICE
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define DEVICE(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_device);
    DEVICE_ELEMENTS
#undef DEVICE
}

static void restore_device_options(void) {
#define DEVICE(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_device, device.BOARD.ENUM);
    DEVICE_ELEMENTS
#undef DEVICE
}

static void save_device_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_DEV(device, HasBluetooth, "board/bluetooth", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasRgb, "led/rgb", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasDebugFs, "board/debugfs", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasHdmi, "board/hdmi", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasLid, "board/lid", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasNetwork, "board/network", INT, 0);
    CHECK_AND_SAVE_DEV(device, HasPortmaster, "board/portmaster", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_device = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, device, HasBluetooth, lang.MUXDEVICE.HASBLUETOOTH, "bluetooth", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasRgb, lang.MUXDEVICE.HASRGB, "rgb", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasDebugFs, lang.MUXDEVICE.HASDEBUGFS, "debugfs", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasHdmi, lang.MUXDEVICE.HASHDMI, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasLid, lang.MUXDEVICE.HASLID, "lid", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasNetwork, lang.MUXDEVICE.HASNETWORK, "network", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasPortmaster, lang.MUXDEVICE.HASPORTMASTER, "portmaster", disabled_enabled, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
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
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound(SND_BACK);

    save_device_options();

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define DEVICE(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_device, UDATA);
    DEVICE_ELEMENTS
#undef DEVICE

    overlay_display();
}

int muxdevice_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXDEVICE.TITLE);
    init_muxdevice(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_device_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
