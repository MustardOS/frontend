#include "muxshare.h"
#include "ui/ui_muxdevice.h"

#define UI_COUNT 7

#define DEVICE(NAME, UDATA) static int NAME##_original;
    DEVICE_ELEMENTS
#undef DEVICE

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblHasBluetooth_device,  lang.MUXDEVICE.HELP.BLUETOOTH},
            {ui_lblHasRgb_device,        lang.MUXDEVICE.HELP.RGB},
            {ui_lblHasDebugFs_device,    lang.MUXDEVICE.HELP.DEBUGFS},
            {ui_lblHasHdmi_device,       lang.MUXDEVICE.HELP.HDMI},
            {ui_lblHasLid_device,        lang.MUXDEVICE.HELP.LID},
            {ui_lblHasNetwork_device,    lang.MUXDEVICE.HELP.NETWORK},
            {ui_lblHasPortmaster_device, lang.MUXDEVICE.HELP.PORTMASTER}
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define DEVICE(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_device);
    DEVICE_ELEMENTS
#undef DEVICE
}

static void restore_device_options(void) {
    lv_dropdown_set_selected(ui_droHasBluetooth_device, device.DEVICE.HAS_BLUETOOTH);
    lv_dropdown_set_selected(ui_droHasRgb_device, device.DEVICE.RGB);
    lv_dropdown_set_selected(ui_droHasDebugFs_device, device.DEVICE.DEBUGFS);
    lv_dropdown_set_selected(ui_droHasHdmi_device, device.DEVICE.HAS_HDMI);
    lv_dropdown_set_selected(ui_droHasLid_device, device.DEVICE.HAS_LID);
    lv_dropdown_set_selected(ui_droHasNetwork_device, device.DEVICE.HAS_NETWORK);
    lv_dropdown_set_selected(ui_droHasPortmaster_device, device.DEVICE.HAS_PORTMASTER);
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
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);

        refresh_device = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, device, HasBluetooth, lang.MUXDEVICE.BLUETOOTH, "bluetooth", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasRgb, lang.MUXDEVICE.RGB, "rgb", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasDebugFs, lang.MUXDEVICE.DEBUGFS, "debugfs", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasHdmi, lang.MUXDEVICE.HDMI, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasLid, lang.MUXDEVICE.LID, "lid", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasNetwork, lang.MUXDEVICE.NETWORK, "network", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, device, HasPortmaster, lang.MUXDEVICE.PORTMASTER, "portmaster", disabled_enabled, 2);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

static void handle_back(void) {
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
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

#define DEVICE(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_device, UDATA);
    DEVICE_ELEMENTS
#undef DEVICE

    overlay_display();
}

static void ui_refresh_task(void) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxdevice_main(void) {
    init_module("muxdevice");

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

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
