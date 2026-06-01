#include "muxshare.h"
#include "ui/ui_muxoverlay.h"

#define OVERLAY(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(OVERLAY_ELEMENTS)
};
#undef OVERLAY

#define OVERLAY(NAME, ENUM, UDATA) static int NAME##_original;
OVERLAY_ELEMENTS
#undef OVERLAY

static int save_mode = 0;
static mux_dialogue save_dlg;

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
}

static int any_overlay_modified(void) {
    if (pct_to_int(lv_dropdown_get_selected(ui_droGenAlpha_overlay), 0, 255) != GenAlpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droGenAnchor_overlay) != GenAnchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droGenScale_overlay) != GenScale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_droBatAlpha_overlay), 0, 255) != BatAlpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droBatAnchor_overlay) != BatAnchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droBatScale_overlay) != BatScale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_droVolAlpha_overlay), 0, 255) != VolAlpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droVolAnchor_overlay) != VolAnchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droVolScale_overlay) != VolScale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_droBriAlpha_overlay), 0, 255) != BriAlpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droBriAnchor_overlay) != BriAnchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droBriScale_overlay) != BriScale_original) return 1;

    return 0;
}

static int reset_mode = 0;
static mux_dialogue reset_dlg;

static void show_reset_dialog(void) {
    reset_mode = 1;
    reset_dlg.selected = 0;
    dialogue_show(&reset_dlg);
    dialogue_refresh(&reset_dlg, &theme);
}

static void hide_reset_dialog(void) {
    reset_mode = 0;
    dialogue_hide(&reset_dlg);
}

static void do_reset(void) {
    remove_directory_recursive(CONF_CONFIG_PATH "settings/overlay");
    refresh_config = 1;
    play_sound(SND_MUOS);
    load_mux("overlay");
    mux_input_stop();
}

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


static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode || reset_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode || reset_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (save_mode) {
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) save_tweak_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

        mux_input_stop();
        return;
    }

    if (reset_mode) {
        mux_confirm_opt opt = (mux_confirm_opt) reset_dlg.selected;
        hide_reset_dialog();
        if (opt == MUX_CONFIRM_YEP) do_reset();
        return;
    }

    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (reset_mode) {
        hide_reset_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_overlay_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);
    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || reset_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE) {
        do_reset();
        return;
    }

    play_sound(SND_CONFIRM);
    show_reset_dialog();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode || reset_mode) return;

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

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    dialogue_init_confirm(&reset_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, lang.GENERIC.RESET, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
