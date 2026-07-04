#include "muxshare.h"
#include "ui/ui_muxoverlay.h"

#define OVERLAY(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(OVERLAY_ELEMENTS) };
#undef OVERLAY

#define OVERLAY(NAME, UDATA) static int NAME##_original;
OVERLAY_ELEMENTS
#undef OVERLAY

static int save_mode = 0;
static mux_dialogue save_dlg;

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_mode, &save_dlg);
}

static int any_overlay_modified(void) {
    if (pct_to_int(lv_dropdown_get_selected(ui_dro_gen_alpha_overlay), 0, 255) != gen_alpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_gen_anchor_overlay) != gen_anchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_gen_scale_overlay) != gen_scale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_dro_bat_alpha_overlay), 0, 255) != bat_alpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_bat_anchor_overlay) != bat_anchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_bat_scale_overlay) != bat_scale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_dro_vol_alpha_overlay), 0, 255) != vol_alpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_vol_anchor_overlay) != vol_anchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_vol_scale_overlay) != vol_scale_original) return 1;

    if (pct_to_int(lv_dropdown_get_selected(ui_dro_bri_alpha_overlay), 0, 255) != bri_alpha_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_bri_anchor_overlay) != bri_anchor_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_bri_scale_overlay) != bri_scale_original) return 1;

    return 0;
}

static int reset_mode = 0;
static mux_dialogue reset_dlg;

static void show_reset_dialog(void) {
    dialogue_open(&reset_mode, &reset_dlg, &theme);
}

static void hide_reset_dialog(void) {
    dialogue_dismiss(&reset_mode, &reset_dlg);
}

static void do_reset(void) {
    remove_directory_recursive(CONF_CONFIG_PATH "settings/overlay");
    refresh_config = 1;
    play_sound(snd_muos);
    load_mux("overlay");
    mux_input_stop();
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define OVERLAY(NAME, UDATA) {UDATA, lang.muxoverlay.help.NAME},
        OVERLAY_ELEMENTS
#undef OVERLAY
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define OVERLAY(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_overlay);
    OVERLAY_ELEMENTS
#undef OVERLAY

    gen_alpha_original = pct_to_int(lv_dropdown_get_selected(ui_dro_gen_alpha_overlay), 0, 255);
    bat_alpha_original = pct_to_int(lv_dropdown_get_selected(ui_dro_bat_alpha_overlay), 0, 255);
    vol_alpha_original = pct_to_int(lv_dropdown_get_selected(ui_dro_vol_alpha_overlay), 0, 255);
    bri_alpha_original = pct_to_int(lv_dropdown_get_selected(ui_dro_bri_alpha_overlay), 0, 255);
}

static void restore_tweak_options(void) {
#define OVERLAY(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_overlay, config.settings.overlay.NAME);
    OVERLAY_ELEMENTS
#undef OVERLAY

    lv_dropdown_set_selected(ui_dro_gen_alpha_overlay, int_to_pct(config.settings.overlay.gen_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_bat_alpha_overlay, int_to_pct(config.settings.overlay.bat_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_vol_alpha_overlay, int_to_pct(config.settings.overlay.vol_alpha, 0, 255));
    lv_dropdown_set_selected(ui_dro_bri_alpha_overlay, int_to_pct(config.settings.overlay.bri_alpha, 0, 255));
}

static void save_tweak_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_PCT(overlay, gen_alpha, "settings/overlay/gen_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, gen_anchor, "settings/overlay/gen_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, gen_scale, "settings/overlay/gen_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, bat_alpha, "settings/overlay/bat_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, bat_anchor, "settings/overlay/bat_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, bat_scale, "settings/overlay/bat_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, vol_alpha, "settings/overlay/vol_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, vol_anchor, "settings/overlay/vol_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, vol_scale, "settings/overlay/vol_scale", INT, 0);

    CHECK_AND_SAVE_PCT(overlay, bri_alpha, "settings/overlay/bri_alpha", INT, 0, 255);
    CHECK_AND_SAVE_STD(overlay, bri_anchor, "settings/overlay/bri_anchor", INT, 0);
    CHECK_AND_SAVE_STD(overlay, bri_scale, "settings/overlay/bri_scale", INT, 0);

    if (is_modified > 0) refresh_config = 1;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *anchor_options[] = {
        lang.muxoverlay.anchor.top.left,    lang.muxoverlay.anchor.top.middle,    lang.muxoverlay.anchor.top.right,
        lang.muxoverlay.anchor.centre.left, lang.muxoverlay.anchor.centre.middle, lang.muxoverlay.anchor.centre.right,
        lang.muxoverlay.anchor.bottom.left, lang.muxoverlay.anchor.bottom.middle, lang.muxoverlay.anchor.bottom.right,
    };

    char *scale_options[] = {lang.muxoverlay.scale.original, lang.muxoverlay.scale.fit, lang.muxoverlay.scale.stretch};

    INIT_OPTION_ITEM(-1, overlay, gen_alpha, lang.muxoverlay.genalpha, "gen_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, gen_anchor, lang.muxoverlay.genanchor, "gen_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, gen_scale, lang.muxoverlay.genscale, "gen_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, bat_alpha, lang.muxoverlay.batalpha, "bat_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, bat_anchor, lang.muxoverlay.batanchor, "bat_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, bat_scale, lang.muxoverlay.batscale, "bat_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, vol_alpha, lang.muxoverlay.volalpha, "vol_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, vol_anchor, lang.muxoverlay.volanchor, "vol_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, vol_scale, lang.muxoverlay.volscale, "vol_scale", scale_options, 3);

    INIT_OPTION_ITEM(-1, overlay, bri_alpha, lang.muxoverlay.brialpha, "bri_alpha", NULL, 0);
    INIT_OPTION_ITEM(-1, overlay, bri_anchor, lang.muxoverlay.brianchor, "bri_anchor", anchor_options, 9);
    INIT_OPTION_ITEM(-1, overlay, bri_scale, lang.muxoverlay.briscale, "bri_scale", scale_options, 3);

    char *alpha_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_gen_alpha_overlay, ui_dro_gen_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_bat_alpha_overlay, ui_dro_bat_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_vol_alpha_overlay, ui_dro_vol_alpha_overlay, alpha_values);
    apply_theme_list_drop_down(&theme, ui_lbl_bri_alpha_overlay, ui_dro_bri_alpha_overlay, alpha_values);
    free(alpha_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    if (reset_mode) {
        dialogue_handle_dpad(&reset_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    if (reset_mode) {
        dialogue_handle_dpad(&reset_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_dpad_up(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (reset_mode) {
        dialogue_handle_dpad(&reset_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (reset_mode) {
        dialogue_handle_dpad(&reset_dlg, &theme, +1, !swap_axis);
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
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_tweak_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

        mux_input_stop();
        return;
    }

    if (reset_mode) {
        const mux_confirm_opt opt = (mux_confirm_opt) reset_dlg.selected;
        hide_reset_dialog();
        if (opt == mux_confirm_yep) do_reset();
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

    if (dialogue_guard_unsaved(&save_mode, &save_dlg, &theme, any_overlay_modified())) return;

    play_sound(snd_back);
    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "overlay");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || reset_mode) return;

    if (config.settings.advanced.trust_remove) {
        do_reset();
        return;
    }

    play_sound(snd_confirm);
    show_reset_dialog();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode || reset_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {NULL, NULL, 0}});

#define OVERLAY(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_overlay, UDATA);
    OVERLAY_ELEMENTS
#undef OVERLAY

    overlay_display();
}

int muxoverlay_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxoverlay.title);
    init_muxoverlay(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
    );
    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.generic.confirm, NULL, lang.generic.reset, lang.generic.cancel,
        lang.generic.select, lang.generic.back
    );
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
