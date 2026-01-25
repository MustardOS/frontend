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

static void show_help() {
    show_info_box(lang.MUXSPACE.TITLE, lang.MUXSPACE.HELP, 0);
}

static void init_space_bars(void) {
    lv_bar_set_range(ui_barPrimary_space, 0, 100);
    lv_bar_set_range(ui_barSecondary_space, 0, 100);
    lv_bar_set_range(ui_barExternal_space, 0, 100);
    lv_bar_set_range(ui_barSystem_space, 0, 100);

    lv_bar_set_mode(ui_barPrimary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_barSecondary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_barExternal_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_barSystem_space, LV_BAR_MODE_NORMAL);
}

static void update_storage_info() {
    struct mount storage_info[] = {
            {ui_pnlPrimary_space,
                    ui_pnlPrimaryBar_space,
                    ui_lblPrimaryValue_space,
                    ui_barPrimary_space,
                    device.STORAGE.ROM.MOUNT},

            {ui_pnlSecondary_space,
                    ui_pnlSecondaryBar_space,
                    ui_lblSecondaryValue_space,
                    ui_barSecondary_space,
                    device.STORAGE.SDCARD.MOUNT},

            {ui_pnlExternal_space,
                    ui_pnlExternalBar_space,
                    ui_lblExternalValue_space,
                    ui_barExternal_space,
                    device.STORAGE.USB.MOUNT},

            {ui_pnlSystem_space,
                    ui_pnlSystemBar_space,
                    ui_lblSystemValue_space,
                    ui_barSystem_space,
                    device.STORAGE.ROOT.MOUNT},
    };

    for (size_t i = 0; i < A_SIZE(storage_info); i++) {
        double total_space, free_space, used_space;
        get_storage_info(storage_info[i].partition, &total_space, &free_space, &used_space);

        if (total_space > 0) {
            lv_obj_clear_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);

            int percentage = (int) ((used_space / total_space) * 100.0 + 0.5);

            if (percentage < 0) percentage = 0;
            if (percentage > 100) percentage = 100;

            lv_bar_set_value(storage_info[i].bar, percentage, LV_ANIM_ON);

            char space_info[32];
            snprintf(space_info, sizeof(space_info), "%.2f GB / %.2f GB (%d%%)",
                     used_space, total_space, percentage);
            lv_label_set_text(storage_info[i].value, space_info);

            if (percentage >= 90) {
                lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(0xEE3F3F), MU_OBJ_INDI_DEFAULT);
                lv_obj_set_style_bg_opa(storage_info[i].bar, 255, MU_OBJ_INDI_DEFAULT);
            }
        } else {
            lv_obj_add_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_VALUE_ITEM(-1, space, Primary, lang.MUXSPACE.PRIMARY, "primary", "");
    INIT_VALUE_ITEM(-1, space, Secondary, lang.MUXSPACE.SECONDARY, "secondary", "");
    INIT_VALUE_ITEM(-1, space, External, lang.MUXSPACE.EXTERNAL, "external", "");
    INIT_VALUE_ITEM(-1, space, System, lang.MUXSPACE.SYSTEM, "system", "");

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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "space");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

#define SPACE(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_space, UDATA);
    SPACE_ELEMENTS
#undef SPACE

    overlay_display();
}

int muxspace_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSPACE.TITLE);

    init_muxspace(ui_pnlContent);
    init_elements();
    init_space_bars();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    update_storage_info();

    init_timer(ui_gen_refresh_task, update_storage_info);
    list_nav_next(0);

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
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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
