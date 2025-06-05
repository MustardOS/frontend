#include "muxshare.h"
#include "ui/ui_muxspace.h"

#define UI_COUNT 4

struct mount {
    lv_obj_t *value_panel;
    lv_obj_t *bar_panel;
    lv_obj_t *value;
    lv_obj_t *bar;
    const char *partition;
};

static void show_help(lv_obj_t *element_focused) {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), lang.MUXSPACE.HELP);
}

static void update_storage_info() {
    struct mount storage_info[] = {
            {ui_pnlPrimary_space,   ui_pnlPrimaryBar_space,   ui_lblPrimaryValue_space,   ui_barPrimary_space,   device.STORAGE.ROM.MOUNT},
            {ui_pnlSecondary_space, ui_pnlSecondaryBar_space, ui_lblSecondaryValue_space, ui_barSecondary_space, device.STORAGE.SDCARD.MOUNT},
            {ui_pnlExternal_space,  ui_pnlExternalBar_space,  ui_lblExternalValue_space,  ui_barExternal_space,  device.STORAGE.USB.MOUNT},
            {ui_pnlSystem_space,    ui_pnlSystemBar_space,    ui_lblSystemValue_space,    ui_barSystem_space,    device.STORAGE.ROOT.MOUNT}
    };

    for (size_t i = 0; i < sizeof(storage_info) / sizeof(storage_info[0]); i++) {
        double total_space, free_space, used_space;
        get_storage_info(storage_info[i].partition, &total_space, &free_space, &used_space);

        if (total_space > 0) {
            lv_obj_clear_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);

            int percentage = (total_space > 0) ? (int) ((used_space * 100) / total_space) : 0;
            lv_bar_set_value(storage_info[i].bar, percentage, LV_ANIM_ON);

            char space_info[32];
            snprintf(space_info, sizeof(space_info), "%.2f GB / %.2f GB (%d%%)",
                     used_space, total_space, percentage);
            lv_label_set_text(storage_info[i].value, space_info);

            if (percentage >= 90) {
                lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(0xEE3F3F),
                                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(storage_info[i].bar, 255,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        } else {
            lv_obj_add_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void init_navigation_group() {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, space, Primary, lang.MUXSPACE.PRIMARY, "primary", "");
    INIT_VALUE_ITEM(-1, space, Secondary, lang.MUXSPACE.SECONDARY, "secondary", "");
    INIT_VALUE_ITEM(-1, space, External, lang.MUXSPACE.EXTERNAL, "external", "");
    INIT_VALUE_ITEM(-1, space, System, lang.MUXSPACE.SYSTEM, "system", "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
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

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "space");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
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
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define SPACE(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_space, UDATA);
    SPACE_ELEMENTS
#undef SPACE

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

int muxspace_main() {
    init_module("muxspace");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSPACE.TITLE);
    init_muxspace(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    update_storage_info();

    init_timer(ui_refresh_task, update_storage_info);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
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
