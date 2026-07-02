#include "muxshare.h"
#include "ui/ui_muxcontent.h"

#define CONTENT(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CONTENT_ELEMENTS) };
#undef CONTENT

#define CONTENT(NAME, UDATA) static int NAME##_original;
CONTENT_ELEMENTS
#undef CONTENT

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

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

static int any_content_modified(void) {
#define CONTENT(NAME, UDATA)                                                                                           \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_content) != NAME##_original) return 1;
    CONTENT_ELEMENTS
#undef CONTENT
    return 0;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define CONTENT(NAME, UDATA) {UDATA, lang.muxcontent.help.NAME},
        CONTENT_ELEMENTS
#undef CONTENT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define CONTENT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_content);
    CONTENT_ELEMENTS
#undef CONTENT
}

static void restore_content_options(void) {
    lv_dropdown_set_selected(ui_dro_launch_swap_content, config.visual.launch_swap);
    lv_dropdown_set_selected(ui_dro_shuffle_content, config.visual.shuffle);
    lv_dropdown_set_selected(ui_dro_box_art_image_content, config.visual.box_art);
    lv_dropdown_set_selected(
        ui_dro_box_art_align_content, config.visual.box_art_align > 0 ? config.visual.box_art_align - 1 : 0
    );
    lv_dropdown_set_selected(ui_dro_full_width_content, config.visual.content_width);
    lv_dropdown_set_selected(ui_dro_launch_splash_content, config.visual.launchsplash);
    lv_dropdown_set_selected(ui_dro_grid_mode_content, config.visual.grid_mode_content);
    lv_dropdown_set_selected(ui_dro_grid_mode_art_content, 1 - config.visual.box_art_hide);
    lv_dropdown_set_selected(ui_dro_box_art_transition_content, config.visual.box_art_transition);
    lv_dropdown_set_selected(ui_dro_box_art_scale_content, config.visual.box_art_scale);
    lv_dropdown_set_selected(ui_dro_box_art_padding_content, config.visual.box_art_padding);
    lv_dropdown_set_selected(ui_dro_box_art_placeholder_content, config.visual.box_art_placeholder);
    lv_dropdown_set_selected(ui_dro_video_preview_content, config.visual.video_preview);
}

static int save_content_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(content, launch_swap, "visual/launch_swap", INT, 0);
    CHECK_AND_SAVE_STD(content, shuffle, "visual/shuffle", INT, 0);
    CHECK_AND_SAVE_STD(content, box_art_image, "visual/boxart", INT, 0);
    CHECK_AND_SAVE_STD(content, box_art_align, "visual/boxartalign", INT, 1);
    CHECK_AND_SAVE_STD(content, full_width, "visual/contentwidth", INT, 0);
    CHECK_AND_SAVE_STD(content, launch_splash, "visual/launchsplash", INT, 0);
    CHECK_AND_SAVE_STD(content, grid_mode, "visual/gridmodecontent", INT, 0);

    if ((int) lv_dropdown_get_selected(ui_dro_grid_mode_art_content) != grid_mode_art_original) {
        is_modified++;
        write_text_to_file(
            CONF_CONFIG_PATH "visual/boxarthide", "w", INT,
            1 - (int) lv_dropdown_get_selected(ui_dro_grid_mode_art_content)
        );
    }

    CHECK_AND_SAVE_STD(content, box_art_transition, "visual/boxarttransition", INT, 0);
    CHECK_AND_SAVE_STD(content, box_art_scale, "visual/boxartscale", INT, 0);
    CHECK_AND_SAVE_STD(content, box_art_padding, "visual/boxartpadding", INT, 0);
    CHECK_AND_SAVE_STD(content, box_art_placeholder, "visual/boxartplaceholder", INT, 0);
    CHECK_AND_SAVE_STD(content, video_preview, "visual/videopreview", INT, 0);

    if (is_modified > 0) run_tweak_script(lang.generic.saving);

    return 0;
}

static void init_navigation_group(void) {
    char *boxart_image[] = {
        lang.muxcontent.box_art.behind, lang.muxcontent.box_art.front, lang.muxcontent.box_art.fs_behind,
        lang.muxcontent.box_art.fs_front, lang.generic.disabled
    };

    char *boxart_align[] = {lang.muxcontent.box_art.align.t_left,  lang.muxcontent.box_art.align.t_mid,
                            lang.muxcontent.box_art.align.t_right, lang.muxcontent.box_art.align.b_left,
                            lang.muxcontent.box_art.align.b_mid,   lang.muxcontent.box_art.align.b_right,
                            lang.muxcontent.box_art.align.m_left,  lang.muxcontent.box_art.align.m_right,
                            lang.muxcontent.box_art.align.m_mid};

    char *launch_swap_options[] = {
        lang.muxcontent.launch_swap.press_a, lang.muxcontent.launch_swap.hold_a, lang.muxcontent.launch_swap.load_state,
        lang.muxcontent.launch_swap.start_fresh
    };

    char *boxart_transition[] = {
        lang.muxcontent.box_art.transition.fade_in,     lang.muxcontent.box_art.transition.slide_right,
        lang.muxcontent.box_art.transition.slide_left,  lang.muxcontent.box_art.transition.slide_up,
        lang.muxcontent.box_art.transition.slide_down,  lang.muxcontent.box_art.transition.bounce_right,
        lang.muxcontent.box_art.transition.bounce_left, lang.muxcontent.box_art.transition.bounce_up,
        lang.muxcontent.box_art.transition.bounce_down, lang.muxcontent.box_art.transition.shoot_right,
        lang.muxcontent.box_art.transition.shoot_left,  lang.muxcontent.box_art.transition.shoot_up,
        lang.muxcontent.box_art.transition.shoot_down,  lang.generic.disabled
    };

    char *video_preview_options[] = {
        lang.generic.disabled, lang.muxcontent.video_preview.delay_3, lang.muxcontent.video_preview.delay_5,
        lang.muxcontent.video_preview.delay_10
    };

    INIT_OPTION_ITEM(
        -1, content, launch_swap, lang.muxcontent.launch_swap.title, "launch_swap", launch_swap_options, 4
    );
    INIT_OPTION_ITEM(-1, content, shuffle, lang.muxcontent.shuffle, "shuffle", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, box_art_image, lang.muxcontent.box_art.title, "boxart", boxart_image, 5);
    INIT_OPTION_ITEM(-1, content, box_art_align, lang.muxcontent.box_art.align.title, "align", boxart_align, 9);
    INIT_OPTION_ITEM(
        -1, content, box_art_transition, lang.muxcontent.box_art.transition.title, "boxarttransition",
        boxart_transition, 14
    );
    INIT_OPTION_ITEM(-1, content, box_art_scale, lang.muxcontent.box_art.scale, "boxartscale", NULL, 0);
    INIT_OPTION_ITEM(-1, content, box_art_padding, lang.muxcontent.box_art.padding, "boxartpadding", NULL, 0);
    INIT_OPTION_ITEM(
        -1, content, box_art_placeholder, lang.muxcontent.box_art.placeholder, "boxartplaceholder", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, content, video_preview, lang.muxcontent.video_preview.title, "videopreview", video_preview_options, 4
    );
    INIT_OPTION_ITEM(-1, content, full_width, lang.muxcontent.full_width, "width", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, launch_splash, lang.muxcontent.launch_splash, "splash", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, grid_mode, lang.muxcontent.grid_mode, "gridmodecontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, grid_mode_art, lang.muxcontent.grid_mode_art, "boxarthide", disabled_enabled, 2);

    char boxart_scale_values[MAX_BUFFER_SIZE];
    snprintf(
        boxart_scale_values, sizeof(boxart_scale_values), "%s\n%s", lang.generic.disabled,
        generate_number_string(1, 100, 1, NULL, "%", NULL, 1)
    );
    apply_theme_list_drop_down(&theme, ui_dro_box_art_scale_content, boxart_scale_values);

    char boxart_padding_values[MAX_BUFFER_SIZE];
    snprintf(
        boxart_padding_values, sizeof(boxart_padding_values), "%s\n%s", lang.generic.disabled,
        generate_number_string(1, 100, 1, NULL, "%", NULL, 1)
    );
    apply_theme_list_drop_down(&theme, ui_dro_box_art_padding_content, boxart_padding_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
    gen_step_movement(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1, 2, 0, 1);
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
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
            play_sound(snd_navigate);
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

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_content_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "content");

        mux_input_stop();
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

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_content_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);

    save_content_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "content");

    mux_input_stop();
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

#define CONTENT(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_content, UDATA);
    CONTENT_ELEMENTS
#undef CONTENT

    overlay_display();
}

int muxcontent_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxcontent.title);
    init_muxcontent(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_elements();
    init_navigation_group();
    first_open = 1;

    restore_content_options();
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
