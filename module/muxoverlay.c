#include "muxshare.h"
#include "ui/ui_muxoverlay.h"

#define UI_COUNT 8

#define OVERLAY(NAME, UDATA) static int NAME##_original;
OVERLAY_ELEMENTS
#undef OVERLAY

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblGenEnable_overlay, lang.MUXOVERLAY.HELP.GENERAL.ENABLE},
            {ui_lblGenAlpha_overlay,  lang.MUXOVERLAY.HELP.GENERAL.ALPHA},
            {ui_lblGenAnchor_overlay, lang.MUXOVERLAY.HELP.GENERAL.ANCHOR},
            {ui_lblGenScale_overlay,  lang.MUXOVERLAY.HELP.GENERAL.SCALE},
            {ui_lblBatEnable_overlay, lang.MUXOVERLAY.HELP.BATTERY.ENABLE},
            {ui_lblBatAlpha_overlay,  lang.MUXOVERLAY.HELP.BATTERY.ALPHA},
            {ui_lblBatAnchor_overlay, lang.MUXOVERLAY.HELP.BATTERY.ANCHOR},
            {ui_lblBatScale_overlay,  lang.MUXOVERLAY.HELP.BATTERY.SCALE}
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define OVERLAY(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_overlay);
    OVERLAY_ELEMENTS
#undef OVERLAY
}

static void restore_tweak_options(void) {
    lv_dropdown_set_selected(ui_droGenEnable_overlay, config.SETTINGS.OVERLAY.GENERAL_ENABLE);
    lv_dropdown_set_selected(ui_droGenAlpha_overlay, config.SETTINGS.OVERLAY.GENERAL_ALPHA);
    lv_dropdown_set_selected(ui_droGenAnchor_overlay, config.SETTINGS.OVERLAY.GENERAL_ANCHOR);
    lv_dropdown_set_selected(ui_droGenScale_overlay, config.SETTINGS.OVERLAY.GENERAL_SCALE);
    lv_dropdown_set_selected(ui_droBatEnable_overlay, config.SETTINGS.OVERLAY.BATTERY_ENABLE);
    lv_dropdown_set_selected(ui_droBatAlpha_overlay, config.SETTINGS.OVERLAY.BATTERY_ALPHA);
    lv_dropdown_set_selected(ui_droBatAnchor_overlay, config.SETTINGS.OVERLAY.BATTERY_ANCHOR);
    lv_dropdown_set_selected(ui_droBatScale_overlay, config.SETTINGS.OVERLAY.BATTERY_SCALE);
}

static void save_tweak_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(overlay, GenEnable, "settings/overlay/gen_enable", INT, 0);
    CHECK_AND_SAVE_STD(overlay, GenAlpha, "settings/overlay/gen_alpha", INT, 0);
    CHECK_AND_SAVE_STD(overlay, GenAnchor, "settings/overlay/gen_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, GenScale, "settings/overlay/gen_scale", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BatEnable, "settings/overlay/bat_enable", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BatAlpha, "settings/overlay/bat_alpha", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BatAnchor, "settings/overlay/bat_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BatScale, "settings/overlay/bat_scale", INT, 0);

    if (is_modified > 0) run_tweak_script();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *anchor_options[] = {
            lang.MUXOVERLAY.ANCHOR.TOP.LEFT,
            lang.MUXOVERLAY.ANCHOR.TOP.MIDDLE,
            lang.MUXOVERLAY.ANCHOR.TOP.RIGHT,
            lang.MUXOVERLAY.ANCHOR.CENTRE.LEFT,
            lang.MUXOVERLAY.ANCHOR.CENTRE.MIDDLE,
            lang.MUXOVERLAY.ANCHOR.CENTRE.RIGHT,
            lang.MUXOVERLAY.ANCHOR.BOTTOM.LEFT,
            lang.MUXOVERLAY.ANCHOR.BOTTOM.MIDDLE,
            lang.MUXOVERLAY.ANCHOR.BOTTOM.RIGHT,
    };

    char *scale_options[] = {
            lang.MUXOVERLAY.SCALE.ORIGINAL,
            lang.MUXOVERLAY.SCALE.FIT,
            lang.MUXOVERLAY.SCALE.STRETCH
    };

    INIT_OPTION_ITEM(-1, overlay, GenEnable, lang.MUXOVERLAY.GENERAL.ENABLE, "gen_enable", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, overlay, GenAlpha, lang.MUXOVERLAY.GENERAL.ALPHA, "gen_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, GenAnchor, lang.MUXOVERLAY.GENERAL.ANCHOR, "gen_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, GenScale, lang.MUXOVERLAY.GENERAL.SCALE, "gen_scale", scale_options, 3);
    INIT_OPTION_ITEM(-1, overlay, BatEnable, lang.MUXOVERLAY.BATTERY.ENABLE, "bat_enable", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, overlay, BatAlpha, lang.MUXOVERLAY.BATTERY.ALPHA, "bat_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, BatAnchor, lang.MUXOVERLAY.BATTERY.ANCHOR, "bat_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, BatScale, lang.MUXOVERLAY.BATTERY.SCALE, "bat_scale", scale_options, 3);

    char *alpha_values = generate_number_string(0, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droGenAlpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_droBatAlpha_overlay, alpha_values);
    free(alpha_values);

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

    decrease_option_value(lv_group_get_focused(ui_group_value), 1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value), 1);
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

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

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

#define OVERLAY(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_overlay, UDATA);
    OVERLAY_ELEMENTS
#undef OVERLAY

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

int muxoverlay_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOVERLAY.TITLE);
    init_muxoverlay(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);
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
