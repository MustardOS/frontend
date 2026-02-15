#include "muxshare.h"
#include "ui/ui_muxoverlay.h"

#define UI_COUNT 12

#define OVERLAY(NAME, ENUM, UDATA) static int NAME##_original;
OVERLAY_ELEMENTS
#undef OVERLAY

static void show_help(void) {
    struct help_msg help_messages[] = {
#define OVERLAY(NAME, ENUM, UDATA) { UDATA, lang.MUXOVERLAY.HELP.ENUM },
            OVERLAY_ELEMENTS
#undef OVERLAY
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define OVERLAY(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_overlay);
    OVERLAY_ELEMENTS
#undef OVERLAY

    GenAlpha_original = pct_to_int(lv_dropdown_get_selected(ui_droGenAlpha_overlay), 0, 255);
    BatAlpha_original = pct_to_int(lv_dropdown_get_selected(ui_droBatAlpha_overlay), 0, 255);
    VolAlpha_original = pct_to_int(lv_dropdown_get_selected(ui_droVolAlpha_overlay), 0, 255);
    BriAlpha_original = pct_to_int(lv_dropdown_get_selected(ui_droBriAlpha_overlay), 0, 255);
}

static void restore_tweak_options(void) {
#define OVERLAY(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_overlay, config.SETTINGS.OVERLAY.ENUM);
    OVERLAY_ELEMENTS
#undef OVERLAY

    lv_dropdown_set_selected(ui_droGenAlpha_overlay, int_to_pct(config.SETTINGS.OVERLAY.GENALPHA, 0, 255));
    lv_dropdown_set_selected(ui_droBatAlpha_overlay, int_to_pct(config.SETTINGS.OVERLAY.BATALPHA, 0, 255));
    lv_dropdown_set_selected(ui_droVolAlpha_overlay, int_to_pct(config.SETTINGS.OVERLAY.VOLALPHA, 0, 255));
    lv_dropdown_set_selected(ui_droBriAlpha_overlay, int_to_pct(config.SETTINGS.OVERLAY.BRIALPHA, 0, 255));
}

static void save_tweak_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_PCT(overlay, GenAlpha, "settings/overlay/gen_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, GenAnchor, "settings/overlay/gen_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, GenScale, "settings/overlay/gen_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, BatAlpha, "settings/overlay/bat_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, BatAnchor, "settings/overlay/bat_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BatScale, "settings/overlay/bat_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, VolAlpha, "settings/overlay/vol_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, VolAnchor, "settings/overlay/vol_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, VolScale, "settings/overlay/vol_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, BriAlpha, "settings/overlay/bri_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, BriAnchor, "settings/overlay/bri_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, BriScale, "settings/overlay/bri_scale", INT, 0);

    if (is_modified > 0) refresh_config = 1;
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

    INIT_OPTION_ITEM(-1, overlay, GenAlpha, lang.MUXOVERLAY.GENALPHA, "gen_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, GenAnchor, lang.MUXOVERLAY.GENANCHOR, "gen_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, GenScale, lang.MUXOVERLAY.GENSCALE, "gen_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, BatAlpha, lang.MUXOVERLAY.BATALPHA, "bat_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, BatAnchor, lang.MUXOVERLAY.BATANCHOR, "bat_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, BatScale, lang.MUXOVERLAY.BATSCALE, "bat_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, VolAlpha, lang.MUXOVERLAY.VOLALPHA, "vol_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, VolAnchor, lang.MUXOVERLAY.VOLANCHOR, "vol_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, VolScale, lang.MUXOVERLAY.VOLSCALE, "vol_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, BriAlpha, lang.MUXOVERLAY.BRIALPHA, "bri_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, BriAnchor, lang.MUXOVERLAY.BRIANCHOR, "bri_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, BriScale, lang.MUXOVERLAY.BRISCALE, "bri_scale", scale_options, 3);

    char *alpha_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droGenAlpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_droBatAlpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_droVolAlpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_droBriAlpha_overlay, alpha_values);
    free(alpha_values);

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

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_RESET, SHORT);
        return;
    }

    remove_directory_recursive(CONF_CONFIG_PATH "settings/overlay");
    refresh_config = 1;

    play_sound(SND_MUOS);
    load_mux("overlay");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.RESET,  0},
            {NULL, NULL,                            0}
    });

#define OVERLAY(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_overlay, UDATA);
    OVERLAY_ELEMENTS
#undef OVERLAY

    overlay_display();
}

int muxoverlay_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXOVERLAY.TITLE);
    init_muxoverlay(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
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
