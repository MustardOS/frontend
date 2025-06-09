#include "muxshare.h"
#include "ui/ui_muxinfo.h"

#define UI_COUNT 6

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblScreenshot_info, lang.MUXINFO.HELP.SCREENSHOT},
            {ui_lblSpace_info,      lang.MUXINFO.HELP.SPACE},
            {ui_lblTester_info,     lang.MUXINFO.HELP.INPUT},
            {ui_lblSysInfo_info,    lang.MUXINFO.HELP.SYSINFO},
            {ui_lblNetInfo_info,    lang.MUXINFO.HELP.NETINFO},
            {ui_lblCredit_info,     lang.MUXINFO.HELP.CREDIT},
    };

    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);
    gen_help(element_focused, help_messages, num_messages);
}

static void init_navigation_group() {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_STATIC_ITEM(-1, info, Screenshot, lang.MUXINFO.SCREENSHOT, "screenshot", 0);
    INIT_STATIC_ITEM(-1, info, Space, lang.MUXINFO.SPACE, "space", 0);
    INIT_STATIC_ITEM(-1, info, Tester, lang.MUXINFO.INPUT, "tester", 0);
    INIT_STATIC_ITEM(-1, info, SysInfo, lang.MUXINFO.SYSINFO, "sysinfo", 0);
    INIT_STATIC_ITEM(-1, info, NetInfo, lang.MUXINFO.NETINFO, "netinfo", 0);
    INIT_STATIC_ITEM(-1, info, Credit, lang.MUXINFO.CREDIT, "credit", 0);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlNetInfo_info, MU_OBJ_FLAG_HIDE_FLOAT);
        ui_count -= 1;
    }

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (msgbox_active) return;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    play_sound(SND_CONFIRM);

    if (element_focused == ui_lblScreenshot_info) {
        load_mux("screenshot");
    } else if (element_focused == ui_lblSpace_info) {
        load_mux("space");
    } else if (element_focused == ui_lblTester_info) {
        load_mux("tester");
    } else if (element_focused == ui_lblSysInfo_info) {
        load_mux("sysinfo");
    } else if (element_focused == ui_lblNetInfo_info) {
        load_mux("netinfo");
    } else if (element_focused == ui_lblCredit_info) {
        load_mux("credits");
    }

    close_input();
    mux_input_stop();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "info");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define INFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_info, UDATA);
    INFO_ELEMENTS
#undef INFO

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxinfo_main() {
    init_module("muxinfo");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXINFO.TITLE);
    init_muxinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
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
