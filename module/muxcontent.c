#include "muxshare.h"
#include "ui/ui_muxcontent.h"

#define CONTENT(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(CONTENT_ELEMENTS)
};
#undef CONTENT

#define CONTENT(NAME, ENUM, UDATA) static int NAME##_original;
CONTENT_ELEMENTS
#undef CONTENT

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];

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
#define CONTENT(NAME, ENUM, UDATA) if ((int) lv_dropdown_get_selected(ui_dro##NAME##_content) != NAME##_original) return 1;
    CONTENT_ELEMENTS
#undef CONTENT
    return 0;
}

static void show_help(void) {
    struct help_msg help_messages[] = {
#define CONTENT(NAME, ENUM, UDATA) { UDATA, lang.MUXCONTENT.HELP.ENUM },
            CONTENT_ELEMENTS
#undef CONTENT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define CONTENT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_content);
    CONTENT_ELEMENTS
#undef CONTENT
}

static void restore_content_options(void) {
    lv_dropdown_set_selected(ui_droLaunchSwap_content, config.VISUAL.LAUNCH_SWAP);
    lv_dropdown_set_selected(ui_droShuffle_content, config.VISUAL.SHUFFLE);
    lv_dropdown_set_selected(ui_droBoxArtImage_content, config.VISUAL.BOX_ART);
    lv_dropdown_set_selected(ui_droBoxArtAlign_content, config.VISUAL.BOX_ART_ALIGN > 0 ? config.VISUAL.BOX_ART_ALIGN - 1 : 0);
    lv_dropdown_set_selected(ui_droFullWidth_content, config.VISUAL.CONTENT_WIDTH);
    lv_dropdown_set_selected(ui_droLaunchSplash_content, config.VISUAL.LAUNCHSPLASH);
    lv_dropdown_set_selected(ui_droGridMode_content, config.VISUAL.GRID_MODE_CONTENT);
    lv_dropdown_set_selected(ui_droGridModeArt_content, 1 - config.VISUAL.BOX_ART_HIDE);
    lv_dropdown_set_selected(ui_droBoxArtScale_content, config.VISUAL.BOX_ART_SCALE);
    lv_dropdown_set_selected(ui_droBoxArtTransition_content, config.VISUAL.BOX_ART_TRANSITION);
    lv_dropdown_set_selected(ui_droVideoPreview_content, config.VISUAL.VIDEO_PREVIEW);
}

static int save_content_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(content, LaunchSwap, "visual/launch_swap", INT, 0);
    CHECK_AND_SAVE_STD(content, Shuffle, "visual/shuffle", INT, 0);
    CHECK_AND_SAVE_STD(content, BoxArtImage, "visual/boxart", INT, 0);
    CHECK_AND_SAVE_STD(content, BoxArtAlign, "visual/boxartalign", INT, 1);
    CHECK_AND_SAVE_STD(content, FullWidth, "visual/contentwidth", INT, 0);
    CHECK_AND_SAVE_STD(content, LaunchSplash, "visual/launchsplash", INT, 0);
    CHECK_AND_SAVE_STD(content, GridMode, "visual/gridmodecontent", INT, 0);

    if ((int) lv_dropdown_get_selected(ui_droGridModeArt_content) != GridModeArt_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "visual/boxarthide", "w", INT, 1 - (int) lv_dropdown_get_selected(ui_droGridModeArt_content));
    }

    CHECK_AND_SAVE_STD(content, BoxArtScale, "visual/boxartscale", INT, 0);
    CHECK_AND_SAVE_STD(content, BoxArtTransition, "visual/boxarttransition", INT, 0);
    CHECK_AND_SAVE_STD(content, VideoPreview, "visual/videopreview", INT, 0);

    if (is_modified > 0) run_tweak_script(lang.GENERIC.SAVING);

    return 0;
}

static void init_navigation_group(void) {
    char *boxart_image[] = {
            lang.MUXCONTENT.BOX_ART.BEHIND,
            lang.MUXCONTENT.BOX_ART.FRONT,
            lang.MUXCONTENT.BOX_ART.FS_BEHIND,
            lang.MUXCONTENT.BOX_ART.FS_FRONT,
            lang.GENERIC.DISABLED
    };

    char *boxart_align[] = {
            lang.MUXCONTENT.BOX_ART.ALIGN.T_LEFT,
            lang.MUXCONTENT.BOX_ART.ALIGN.T_MID,
            lang.MUXCONTENT.BOX_ART.ALIGN.T_RIGHT,
            lang.MUXCONTENT.BOX_ART.ALIGN.B_LEFT,
            lang.MUXCONTENT.BOX_ART.ALIGN.B_MID,
            lang.MUXCONTENT.BOX_ART.ALIGN.B_RIGHT,
            lang.MUXCONTENT.BOX_ART.ALIGN.M_LEFT,
            lang.MUXCONTENT.BOX_ART.ALIGN.M_RIGHT,
            lang.MUXCONTENT.BOX_ART.ALIGN.M_MID
    };

    char *launch_swap_options[] = {
            lang.MUXCONTENT.LAUNCH_SWAP.PRESS_A,
            lang.MUXCONTENT.LAUNCH_SWAP.HOLD_A,
            lang.MUXCONTENT.LAUNCH_SWAP.LOAD_STATE,
            lang.MUXCONTENT.LAUNCH_SWAP.START_FRESH
    };

    char *boxart_transition[] = {
            lang.MUXCONTENT.BOX_ART.TRANSITION.FADE_IN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SLIDE_DOWN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.BOUNCE_DOWN,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_RIGHT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_LEFT,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_UP,
            lang.MUXCONTENT.BOX_ART.TRANSITION.SHOOT_DOWN,
            lang.GENERIC.DISABLED
    };

    char *video_preview_options[] = {
            lang.GENERIC.DISABLED,
            lang.MUXCONTENT.VIDEO_PREVIEW.DELAY_3,
            lang.MUXCONTENT.VIDEO_PREVIEW.DELAY_5,
            lang.MUXCONTENT.VIDEO_PREVIEW.DELAY_10
    };

    INIT_OPTION_ITEM(-1, content, LaunchSwap, lang.MUXCONTENT.LAUNCH_SWAP.TITLE, "launch_swap", launch_swap_options, 4);
    INIT_OPTION_ITEM(-1, content, Shuffle, lang.MUXCONTENT.SHUFFLE, "shuffle", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, BoxArtImage, lang.MUXCONTENT.BOX_ART.TITLE, "boxart", boxart_image, 5);
    INIT_OPTION_ITEM(-1, content, BoxArtAlign, lang.MUXCONTENT.BOX_ART.ALIGN.TITLE, "align", boxart_align, 9);
    INIT_OPTION_ITEM(-1, content, BoxArtScale, lang.MUXCONTENT.BOX_ART.SCALE, "boxartscale", NULL, 0);
    INIT_OPTION_ITEM(-1, content, BoxArtTransition, lang.MUXCONTENT.BOX_ART.TRANSITION.TITLE, "boxarttransition", boxart_transition, 14);
    INIT_OPTION_ITEM(-1, content, VideoPreview, lang.MUXCONTENT.VIDEO_PREVIEW.TITLE, "videopreview", video_preview_options, 4);
    INIT_OPTION_ITEM(-1, content, FullWidth, lang.MUXCONTENT.WIDTH, "width", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, LaunchSplash, lang.MUXCONTENT.LAUNCHSPLASH, "splash", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, GridMode, lang.MUXCONTENT.GRIDMODE, "gridmodecontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, content, GridModeArt, lang.MUXCONTENT.GRIDMODEART, "boxarthide", disabled_enabled, 2);

    char boxart_scale_values[MAX_BUFFER_SIZE];
    snprintf(boxart_scale_values, sizeof(boxart_scale_values), "%s\n%s", lang.GENERIC.DISABLED, generate_number_string(1, 100, 1, NULL, "%", NULL, 1));
    apply_theme_list_drop_down(&theme, ui_droBoxArtScale_content, boxart_scale_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
    gen_step_movement(direct_to_previous(ui_objects, ui_count, &nav_moved), +1, 0, 0);
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
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
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) save_content_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
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

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_content_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);

    save_content_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "content");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode) return;

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
            {NULL, NULL,                            0}
    });

#define CONTENT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_content, UDATA);
    CONTENT_ELEMENTS
#undef CONTENT

    overlay_display();
}

int muxcontent_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCONTENT.TITLE);
    init_muxcontent(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_elements();
    init_navigation_group();
    first_open = 1;

    restore_content_options();
    init_dropdown_settings();

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
