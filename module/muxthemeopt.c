#include "muxshare.h"
#include "ui/ui_muxthemeopt.h"

#define THEMEOPT(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(THEMEOPT_ELEMENTS)
};
#undef THEMEOPT

#define THEMEOPT(NAME, ENUM, UDATA) static int NAME##_original;
THEMEOPT_ELEMENTS
#undef THEMEOPT

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

static int any_themeopt_modified(void) {
#define THEMEOPT(NAME, ENUM, UDATA) if ((int) lv_dropdown_get_selected(ui_dro##NAME##_themeopt) != NAME##_original) return 1;
    THEMEOPT_ELEMENTS
#undef THEMEOPT
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

static const int16_t glyph_size_values[] = {-2, 0, -1, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128};

static void do_reset(void) {
    lv_dropdown_set_selected(ui_droHeaderHeight_themeopt, 0);
    lv_dropdown_set_selected(ui_droFooterHeight_themeopt, 0);
    lv_dropdown_set_selected(ui_droContentItemCount_themeopt, 0);
    lv_dropdown_set_selected(ui_droGlyphSize_themeopt, 0);
    play_sound(SND_MUOS);
}

static void show_help(void) {
    struct help_msg help_messages[] = {
#define THEMEOPT(NAME, ENUM, UDATA) { UDATA, lang.MUXTHEMEOPT.HELP.ENUM },
            THEMEOPT_ELEMENTS
#undef THEMEOPT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define THEMEOPT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_themeopt);
    THEMEOPT_ELEMENTS
#undef THEMEOPT
}

static void restore_themeopt_options(void) {
    lv_dropdown_set_selected(ui_droHeaderHeight_themeopt, (uint32_t) (config.SETTINGS.THEMEOPT.HEADER_HEIGHT + 1));
    lv_dropdown_set_selected(ui_droFooterHeight_themeopt, (uint32_t) (config.SETTINGS.THEMEOPT.FOOTER_HEIGHT + 1));
    lv_dropdown_set_selected(ui_droContentItemCount_themeopt, (uint32_t) config.SETTINGS.THEMEOPT.CONTENT_ITEM_COUNT);

    uint32_t glyph_idx = 0;
    int16_t stored = config.SETTINGS.THEMEOPT.GLYPH_SIZE;
    for (size_t i = 0; i < A_SIZE(glyph_size_values); i++) {
        if (glyph_size_values[i] == stored) {
            glyph_idx = (uint32_t) i;
            break;
        }
    }
    lv_dropdown_set_selected(ui_droGlyphSize_themeopt, glyph_idx);
}

static void save_themeopt_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(themeopt, HeaderHeight, "settings/theme/header_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, FooterHeight, "settings/theme/footer_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, ContentItemCount, "settings/theme/content_item_count", INT, 0);

    uint32_t glyph_sel = lv_dropdown_get_selected(ui_droGlyphSize_themeopt);
    if ((int) glyph_sel != GlyphSize_original) {
        int16_t val = (glyph_sel < A_SIZE(glyph_size_values)) ? glyph_size_values[glyph_sel] : -2;
        write_text_to_file(CONF_CONFIG_PATH "settings/theme/glyph_size", "w", INT, (int) val);
        config.SETTINGS.THEMEOPT.GLYPH_SIZE = val;
        is_modified++;
    }

    if (is_modified > 0) run_tweak_script(lang.GENERIC.SAVING);
}

static void init_navigation_group(void) {
    char *height_options = generate_number_string(0, 64, 1, lang.MUXTHEMEOPT.SIZE_DEFAULT, NULL, NULL, 0);
    char *count_options = generate_number_string(1, 64, 1, lang.MUXTHEMEOPT.SIZE_DEFAULT, NULL, NULL, 0);

    char glyph_options[256];
    snprintf(glyph_options, sizeof(glyph_options),
             "%s\n%s\n%s\n8\n12\n16\n20\n24\n28\n32\n36\n40\n48\n56\n64\n80\n96\n128",
             lang.MUXTHEMEOPT.SIZE_DEFAULT,
             lang.MUXTHEMEOPT.GLYPH_AUTO,
             lang.MUXTHEMEOPT.GLYPH_NATIVE);

    INIT_OPTION_ITEM(-1, themeopt, HeaderHeight, lang.MUXTHEMEOPT.HEADER_HEIGHT, "headerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, FooterHeight, lang.MUXTHEMEOPT.FOOTER_HEIGHT, "footerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, ContentItemCount, lang.MUXTHEMEOPT.CONTENT_ITEM_COUNT, "count", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, GlyphSize, lang.MUXTHEMEOPT.GLYPH_SIZE, "glyphsize", NULL, 0);

    apply_theme_list_drop_down(&theme, ui_droHeaderHeight_themeopt, height_options);
    apply_theme_list_drop_down(&theme, ui_droFooterHeight_themeopt, height_options);
    apply_theme_list_drop_down(&theme, ui_droContentItemCount_themeopt, count_options);
    apply_theme_list_drop_down(&theme, ui_droGlyphSize_themeopt, glyph_options);

    free(height_options);
    free(count_options);

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

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, -1);
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

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, +1);
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

        if (opt == MUX_UNSAVED_SAVE) save_themeopt_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

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

static void handle_x(void) {
    if (msgbox_active || save_mode || reset_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE) {
        do_reset();
        return;
    }

    play_sound(SND_CONFIRM);
    show_reset_dialog();
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

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_themeopt_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);

    save_themeopt_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

    mux_input_stop();
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

#define THEMEOPT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_themeopt, UDATA);
    THEMEOPT_ELEMENTS
#undef THEMEOPT

    overlay_display();
}

int muxthemeopt_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTHEMEOPT.TITLE);
    init_muxthemeopt(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_elements();
    init_navigation_group();
    first_open = 1;

    restore_themeopt_options();
    init_dropdown_settings();

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    dialogue_init_confirm(&reset_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, lang.GENERIC.RESET, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A]          = handle_a,
                    [MUX_INPUT_B]          = handle_b,
                    [MUX_INPUT_X]          = handle_x,
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN]  = handle_dpad_down,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2]   = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT]  = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP]    = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN]  = handle_dpad_down_hold,
                    [MUX_INPUT_L1]         = handle_list_nav_page_up,
                    [MUX_INPUT_L2]         = hold_call_set,
                    [MUX_INPUT_R1]         = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
