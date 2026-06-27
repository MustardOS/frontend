#include "muxshare.h"
#include "ui/ui_muxvisual.h"

static int save_mode = 0;
static int pending_sort = 0;

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

#define VISUAL(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(VISUAL_ELEMENTS) };
#undef VISUAL

#define VISUAL(NAME, UDATA) static int NAME##_original;
VISUAL_ELEMENTS
#undef VISUAL

static int any_visual_modified(void) {
#define VISUAL(NAME, UDATA)                                                                                            \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_visual) != NAME##_original) return 1;
    VISUAL_ELEMENTS
#undef VISUAL
    return 0;
}

static int overlay_count;
static int has_theme_overlay;

static int overlay_config_to_dropdown(const int config_val) {
    if (!has_theme_overlay) {
        if (config_val == 1) return 0;
        if (config_val >= 2) return config_val - 1;
    }
    return config_val;
}

static int overlay_dropdown_to_config(const int dropdown_idx) {
    if (!has_theme_overlay && dropdown_idx >= 1) return dropdown_idx + 1;
    return dropdown_idx;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define VISUAL(NAME, UDATA) {UDATA, lang.muxvisual.help.NAME},
        VISUAL_ELEMENTS
#undef VISUAL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define VISUAL(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_visual);
    VISUAL_ELEMENTS
#undef VISUAL
}

static void restore_visual_options(void) {
#define VISUAL(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_visual, config.visual.NAME);
    VISUAL_ELEMENTS
#undef VISUAL

    {
        const int ddr = overlay_config_to_dropdown(config.visual.overlay_image);
        lv_dropdown_set_selected(ui_dro_overlay_image_visual, ddr < 0 || ddr >= overlay_count ? 0 : ddr);
    }

    lv_dropdown_set_selected(
        ui_dro_overlay_transparency_visual, int_to_pct(config.visual.overlay_transparency, 0, 255)
    );
}

static void save_visual_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(visual, battery, "visual/battery", INT, 0);
    CHECK_AND_SAVE_STD(visual, clock, "visual/clock", INT, 0);
    CHECK_AND_SAVE_STD(visual, network, "visual/network", INT, 0);
    CHECK_AND_SAVE_STD(visual, bluetooth, "visual/bluetooth", INT, 0);
    CHECK_AND_SAVE_STD(visual, header_title, "visual/headertitle", INT, 0);
    CHECK_AND_SAVE_STD(visual, dialogue_transition, "visual/dialoguetransition", INT, 0);
    CHECK_AND_SAVE_STD(visual, name, "visual/name", INT, 0);
    CHECK_AND_SAVE_STD(visual, dash, "visual/dash", INT, 0);
    CHECK_AND_SAVE_STD(visual, friendly_folder, "visual/friendlyfolder", INT, 0);
    CHECK_AND_SAVE_STD(visual, the_title_format, "visual/thetitleformat", INT, 0);
    CHECK_AND_SAVE_STD(visual, title_include_root_drive, "visual/titleincluderootdrive", INT, 0);
    CHECK_AND_SAVE_STD(visual, folder_item_count, "visual/folderitemcount", INT, 0);
    CHECK_AND_SAVE_STD(visual, display_empty_folder, "visual/folderempty", INT, 0);
    CHECK_AND_SAVE_STD(visual, menu_counter_folder, "visual/counterfolder", INT, 0);
    CHECK_AND_SAVE_STD(visual, menu_counter_file, "visual/counterfile", INT, 0);
    CHECK_AND_SAVE_STD(visual, hidden, "visual/hidden", INT, 0);
    CHECK_AND_SAVE_STD(visual, content_collect, "visual/contentcollect", INT, 0);
    CHECK_AND_SAVE_STD(visual, content_history, "visual/contenthistory", INT, 0);
    CHECK_AND_SAVE_STD(visual, mixed_content, "visual/mixedcontent", INT, 0);
    CHECK_AND_SAVE_STD(visual, forward_history, "visual/forwardhistory", INT, 0);
    CHECK_AND_SAVE_STD(visual, name_scroll, "visual/namescroll", INT, 0);
    CHECK_AND_SAVE_STD(visual, label_scroll_speed, "visual/labelscrollspeed", INT, 0);
    CHECK_AND_SAVE_STD(visual, list_glyph, "visual/listglyph", INT, 0);
    CHECK_AND_SAVE_STD(visual, selection_animation, "visual/selectionanimation", INT, 0);
    CHECK_AND_SAVE_STD(visual, selection_style, "visual/selectionstyle", INT, 0);
    CHECK_AND_SAVE_STD(visual, render_shadows, "visual/shadow", INT, 0);

    {
        const int oi_current = lv_dropdown_get_selected(ui_dro_overlay_image_visual);
        if (oi_current != overlay_image_original) {
            is_modified++;
            write_text_to_file(
                CONF_CONFIG_PATH "visual/overlayimage", "w", INT, overlay_dropdown_to_config(oi_current)
            );
        }
    }

    {
        const int ot_current = lv_dropdown_get_selected(ui_dro_overlay_transparency_visual);
        if (ot_current != overlay_transparency_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "visual/overlaytransparency", "w", INT, pct_to_int(ot_current, 0, 255));
        }
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *visual_names[] = {
        lang.muxvisual.name.full, lang.muxvisual.name.rem_sq, lang.muxvisual.name.rem_pa, lang.muxvisual.name.rem_sqpa
    };

    char *scroll_mode[] = {
        lang.muxvisual.scroll_mode.disabled, lang.muxvisual.scroll_mode.continuous, lang.muxvisual.scroll_mode.bounce
    };

    char *label_scroll_speed[] = {scroll_speed[0], scroll_speed[1], scroll_speed[2], scroll_speed[3]};

    char *dialogue_transition[] = {
        lang.muxcontent.box_art.transition.fade_in,     lang.muxcontent.box_art.transition.slide_right,
        lang.muxcontent.box_art.transition.slide_left,  lang.muxcontent.box_art.transition.slide_up,
        lang.muxcontent.box_art.transition.slide_down,  lang.muxcontent.box_art.transition.bounce_right,
        lang.muxcontent.box_art.transition.bounce_left, lang.muxcontent.box_art.transition.bounce_up,
        lang.muxcontent.box_art.transition.bounce_down, lang.muxcontent.box_art.transition.shoot_right,
        lang.muxcontent.box_art.transition.shoot_left,  lang.muxcontent.box_art.transition.shoot_up,
        lang.muxcontent.box_art.transition.shoot_down,  lang.generic.disabled
    };

    char *selection_animation[] = {lang.generic.disabled, lang.generic.minimal, lang.generic.low,
                                   lang.generic.medium,   lang.generic.high,    lang.generic.maximum};

    char *bounce_direction[] = {
        lang.generic.outward, lang.generic.vertical, lang.generic.horizontal, lang.generic.wobble, lang.generic.shrink
    };

    INIT_OPTION_ITEM(-1, visual, sort, lang.muxvisual.sort, "sort", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, battery, lang.muxvisual.battery, "battery", battery_display, 3);
    INIT_OPTION_ITEM(-1, visual, clock, lang.muxvisual.clock, "clock", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, network, lang.muxvisual.network, "network", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, bluetooth, lang.muxvisual.bluetooth, "bluetooth", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, header_title, lang.muxvisual.headertitle, "headertitle", hidden_visible, 2);
    INIT_OPTION_ITEM(
        -1, visual, dialogue_transition, lang.muxvisual.dialoguetransition, "dialoguetransition", dialogue_transition,
        14
    );
    INIT_OPTION_ITEM(-1, visual, name, lang.muxvisual.name.title, "name", visual_names, 4);
    INIT_OPTION_ITEM(-1, visual, name_scroll, lang.muxvisual.namescroll, "namescroll", scroll_mode, 3);
    INIT_OPTION_ITEM(
        -1, visual, label_scroll_speed, lang.muxvisual.labelscrollspeed, "labelscrollspeed", label_scroll_speed, 4
    );
    INIT_OPTION_ITEM(-1, visual, list_glyph, lang.muxvisual.listglyph, "listglyph", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, selection_animation, lang.muxvisual.selectionanimation, "selectionanimation", selection_animation, 6
    );
    INIT_OPTION_ITEM(-1, visual, selection_style, lang.muxvisual.selectionstyle, "selectionstyle", bounce_direction, 5);
    INIT_OPTION_ITEM(-1, visual, dash, lang.muxvisual.dash, "dash", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, friendly_folder, lang.muxvisual.friendlyfolder, "friendlyfolder", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, the_title_format, lang.muxvisual.thetitleformat, "thetitleformat", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, title_include_root_drive, lang.muxvisual.titleincluderootdrive, "titleincluderootdrive",
        disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, folder_item_count, lang.muxvisual.folderitemcount, "folderitemcount", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, display_empty_folder, lang.muxvisual.displayemptyfolder, "folderempty", hidden_visible, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, menu_counter_folder, lang.muxvisual.menucounterfolder, "counterfolder", hidden_visible, 2
    );
    INIT_OPTION_ITEM(-1, visual, menu_counter_file, lang.muxvisual.menucounterfile, "counterfile", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, hidden, lang.muxvisual.hidden, "hidden", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, content_collect, lang.muxvisual.contentcollect, "collection", toggle_icon_visible, 3);
    INIT_OPTION_ITEM(-1, visual, content_history, lang.muxvisual.contenthistory, "history", toggle_icon_visible, 3);
    INIT_OPTION_ITEM(-1, visual, mixed_content, lang.muxvisual.mixedcontent, "mixedcontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, forward_history, lang.muxvisual.forwardhistory, "forwardhistory", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, overlay_image, lang.muxvisual.overlay.image, "overlayimage", NULL, 0);
    INIT_OPTION_ITEM(
        -1, visual, overlay_transparency, lang.muxvisual.overlay.transparency, "overlaytransparency", NULL, 0
    );
    INIT_OPTION_ITEM(-1, visual, render_shadows, lang.muxvisual.rendershadows, "shadow", disabled_enabled, 2);

    if (config.visual.selection_animation == 6) {
        char *ludicrous_options[] = {lang.generic.disabled, lang.generic.minimal, lang.generic.low,
                                     lang.generic.medium,   lang.generic.high,    lang.generic.maximum,
                                     lang.generic.ludicrous};
        add_drop_down_options(ui_dro_selection_animation_visual, ludicrous_options, 7);
    }

    const char *program = lv_obj_get_user_data(ui_screen);
    char tmp_path[MAX_BUFFER_SIZE];
    has_theme_overlay = load_image_specifics(mux_dim, program, "overlay", "png", tmp_path, sizeof(tmp_path))
                        || load_image_specifics("", program, "overlay", "png", tmp_path, sizeof(tmp_path));
    overlay_count = load_overlay_set(ui_dro_overlay_image_visual, has_theme_overlay);

    char *pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_dro_overlay_transparency_visual, pct_values);
    free(pct_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!device.board.has_network) HIDE_OPTION_ITEM(visual, network);
    if (!device.board.has_bluetooth) HIDE_OPTION_ITEM(visual, bluetooth);
}

static void refresh_overlay_preview(void) {
    const struct _lv_obj_t *focused = lv_group_get_focused(ui_group);

    if (focused == ui_lbl_overlay_image_visual) {
        const int16_t saved_image = config.visual.overlay_image;
        const int16_t saved_opa = config.visual.overlay_transparency;

        config.visual.overlay_image =
            (int16_t) overlay_dropdown_to_config(lv_dropdown_get_selected(ui_dro_overlay_image_visual));
        config.visual.overlay_transparency =
            (int16_t) pct_to_int(lv_dropdown_get_selected(ui_dro_overlay_transparency_visual), 0, 255);

        load_overlay_image_sdl();

        config.visual.overlay_image = saved_image;
        config.visual.overlay_transparency = saved_opa;
    } else if (focused == ui_lbl_overlay_transparency_visual) {
        const int opa = pct_to_int(lv_dropdown_get_selected(ui_dro_overlay_transparency_visual), 0, 255);
        display_update_overlay_opacity((uint8_t) opa);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
    refresh_overlay_preview();
}

static void handle_option_next(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
    refresh_overlay_preview();
}

static void handle_x(void) {
    if (msgbox_active || save_mode) return;

    const char *focused_key = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    if (!focused_key || strcmp(focused_key, "selectionanimation") != 0) return;

    char *ludicrous_options[] = {lang.generic.disabled, lang.generic.minimal, lang.generic.low,
                                 lang.generic.medium,   lang.generic.high,    lang.generic.maximum,
                                 lang.generic.ludicrous};
    add_drop_down_options(ui_dro_selection_animation_visual, ludicrous_options, 7);
    lv_dropdown_set_selected(ui_dro_selection_animation_visual, 6);
    play_sound(snd_muos);
}

static void handle_a(void) {
    if (msgbox_active) return;

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (pending_sort) {
            pending_sort = 0;

            if (opt == mux_unsaved_save) save_visual_options();
            play_sound(snd_confirm);

            load_mux("sort");
            mux_input_stop();

            return;
        }

        if (opt == mux_unsaved_save) save_visual_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

        mux_input_stop();

        return;
    }

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_sort_visual) {
        if (!config.settings.advanced.trust_modify && any_visual_modified()) {
            pending_sort = 1;
            show_save_dialog();

            return;
        }

        play_sound(snd_confirm);

        save_visual_options();
        load_mux("sort");

        mux_input_stop();
    } else {
        handle_option_next();
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_visual_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);

    save_visual_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode) return;

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

#define VISUAL(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_visual, UDATA);
    VISUAL_ELEMENTS
#undef VISUAL

    overlay_display();
}

int muxvisual_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxvisual.title);
    init_muxvisual(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_visual_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
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
