#include "muxshare.h"
#include "../common/ui/font.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxaccess.h"

#define ACCESS(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(ACCESS_ELEMENTS) };
#undef ACCESS

#define ACCESS(NAME, UDATA) static int NAME##_original;
ACCESS_ELEMENTS
#undef ACCESS

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

static mux_dialogue save_dlg;

static const int text_size_scale[] = {0, 115, 130, 145};

#define LEGIBLE_FONT_NAME "Atkinson Hyperlegible"
#define ACCESS_BACKUP     CONF_CONFIG_PATH "visual/accessbak/"

static void backup_path(const char *key, char *out, const size_t len) {
    const char *base = strrchr(key, '/');
    snprintf(out, len, ACCESS_BACKUP "%s", base ? base + 1 : key);
}

static void preset_int(const char *key, const int value) {
    char live[MAX_BUFFER_SIZE];
    snprintf(live, sizeof(live), CONF_CONFIG_PATH "%s", key);

    char kept[MAX_BUFFER_SIZE];
    backup_path(key, kept, sizeof(kept));
    create_directories(kept, 1);

    if (!file_exist(kept)) write_text_to_file(kept, "w", INT, cfg_read_int(live, 0));

    write_text_to_file(live, "w", INT, value);
}

static void revert_int(const char *key) {
    char kept[MAX_BUFFER_SIZE];
    backup_path(key, kept, sizeof(kept));

    if (!file_exist(kept)) return;

    char live[MAX_BUFFER_SIZE];
    snprintf(live, sizeof(live), CONF_CONFIG_PATH "%s", key);

    write_text_to_file(live, "w", INT, cfg_read_int(kept, 0));
    remove(kept);
}

static void preset_char(const char *key, const char *value) {
    char live[MAX_BUFFER_SIZE];
    snprintf(live, sizeof(live), CONF_CONFIG_PATH "%s", key);

    char kept[MAX_BUFFER_SIZE];
    backup_path(key, kept, sizeof(kept));
    create_directories(kept, 1);

    if (!file_exist(kept)) {
        char *current = read_all_char_from(live);
        write_text_to_file(kept, "w", CHAR, current ? current : "");
        free(current);
    }

    write_text_to_file(live, "w", CHAR, value);
}

static void revert_char(const char *key) {
    char kept[MAX_BUFFER_SIZE];
    backup_path(key, kept, sizeof(kept));

    if (!file_exist(kept)) return;

    char live[MAX_BUFFER_SIZE];
    snprintf(live, sizeof(live), CONF_CONFIG_PATH "%s", key);

    char *previous = read_all_char_from(kept);
    write_text_to_file(live, "w", CHAR, previous ? previous : "");
    free(previous);

    remove(kept);
}

static const char *motion_keys[] = {"visual/elementtransition", "visual/selectionanimation", "visual/blackfade",
                                    "visual/namescroll",        "visual/video_wallpaper",    "visual/videopreview"};

static const char *text_keys[] = {
    "settings/font/list_size", "settings/font/header_size", "settings/font/footer_size", "settings/font/panel_size"
};

static int text_key_theme_size(const size_t key) {
    const int16_t sizes[] = {
        theme.font.font_list_size, theme.font.font_header_size, theme.font.font_footer_size, theme.font.font_panel_size
    };

    return sizes[key] > 0 ? sizes[key] : get_font_size();
}

static int text_key_base(const size_t key) {
    char kept[MAX_BUFFER_SIZE];
    backup_path(text_keys[key], kept, sizeof(kept));

    char live[MAX_BUFFER_SIZE];
    snprintf(live, sizeof(live), CONF_CONFIG_PATH "%s", text_keys[key]);

    const int stored = cfg_read_int(file_exist(kept) ? kept : live, 0);

    return stored > 0 ? stored : text_key_theme_size(key);
}

static int text_size_for(const size_t key, const int index) {
    if (index <= 0 || index >= (int) A_SIZE(text_size_scale)) return 0;

    return text_key_base(key) * text_size_scale[index] / 100;
}

static int any_access_modified(void) {
#define ACCESS(NAME, UDATA)                                                                                            \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_access) != NAME##_original) return 1;
    ACCESS_ELEMENTS
#undef ACCESS
    return 0;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define ACCESS(NAME, UDATA) {UDATA, lang.muxaccess.help.NAME},
        ACCESS_ELEMENTS
#undef ACCESS
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define ACCESS(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_access);
    ACCESS_ELEMENTS
#undef ACCESS
}

static int text_size_index(void) {
    for (int i = 1; i < (int) A_SIZE(text_size_scale); i++)
        if (config.settings.font.list_size == text_size_for(0, i)) return i;

    return 0;
}

static int legible_font_index(void) {
    return config.settings.advanced.font == 2 && !strcasecmp(config.settings.font.name, LEGIBLE_FONT_NAME);
}

static void restore_access_options(void) {
    lv_dropdown_set_selected(ui_dro_reduce_motion_access, config.visual.reduce_motion);
    lv_dropdown_set_selected(ui_dro_high_contrast_access, config.visual.high_contrast);
    lv_dropdown_set_selected(ui_dro_bold_focus_access, config.visual.bold_focus);
    lv_dropdown_set_selected(ui_dro_text_size_access, text_size_index());
    lv_dropdown_set_selected(ui_dro_legible_font_access, legible_font_index());
}

static void preset_motion(const int reduced) {
    if (!reduced) {
        for (size_t i = 0; i < A_SIZE(motion_keys); i++)
            revert_int(motion_keys[i]);

        revert_int("visual/boxarttransition");

        return;
    }

    for (size_t i = 0; i < A_SIZE(motion_keys); i++)
        preset_int(motion_keys[i], 0);

    preset_int("visual/boxarttransition", 13);
}

static void preset_bold(const int bold) {
    if (!bold) {
        revert_int("visual/shadow");
        return;
    }

    preset_int("visual/shadow", 1);
}

static void preset_legible_font(const int legible) {
    if (!legible) {
        revert_int("settings/advanced/font");
        revert_char("settings/font/name");

        return;
    }

    preset_int("settings/advanced/font", 2);
    preset_char("settings/font/name", LEGIBLE_FONT_NAME);
}

static void preset_text_size(const int index) {
    if (index <= 0) {
        for (size_t i = 0; i < A_SIZE(text_keys); i++)
            revert_int(text_keys[i]);

        return;
    }

    for (size_t i = 0; i < A_SIZE(text_keys); i++)
        preset_int(text_keys[i], text_size_for(i, index));
}

static void save_access_options(void) {
    int is_modified = 0;
    int save_failed = 0;

    CHECK_AND_SAVE_STD(access, reduce_motion, "visual/reducemotion", INT, 0);
    CHECK_AND_SAVE_STD(access, high_contrast, "visual/highcontrast", INT, 0);
    CHECK_AND_SAVE_STD(access, bold_focus, "visual/boldfocus", INT, 0);

    const int motion = lv_dropdown_get_selected(ui_dro_reduce_motion_access);
    if (motion != reduce_motion_original) preset_motion(motion);

    const int bold = lv_dropdown_get_selected(ui_dro_bold_focus_access);
    if (bold != bold_focus_original) preset_bold(bold);

    const int legible = lv_dropdown_get_selected(ui_dro_legible_font_access);
    if (legible != legible_font_original) {
        is_modified++;
        preset_legible_font(legible);
    }

    const int size_index = lv_dropdown_get_selected(ui_dro_text_size_access);
    if (size_index != text_size_original) {
        is_modified++;
        preset_text_size(size_index);
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }

    REPORT_SAVE_FAILURE();
}

static void init_navigation_group(void) {
    char *text_size_options[] = {
        lang.muxaccess.size.theme, lang.muxaccess.size.large, lang.muxaccess.size.larger, lang.muxaccess.size.largest
    };

    INIT_OPTION_ITEM(-1, access, reduce_motion, lang.muxaccess.reducemotion, "reducemotion", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, access, high_contrast, lang.muxaccess.highcontrast, "highcontrast", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, access, bold_focus, lang.muxaccess.boldfocus, "boldfocus", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, access, text_size, lang.muxaccess.textsize, "textsize", text_size_options, 4);
    INIT_OPTION_ITEM(-1, access, legible_font, lang.muxaccess.legiblefont, "legiblefont", disabled_enabled, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    if (dialogue_active(&save_dlg)) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }

        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    if (dialogue_active(&save_dlg)) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }

        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_dpad_up(void) {
    if (dialogue_active(&save_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }

        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&save_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }

        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad_hold(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&save_dlg)) {
        dialogue_handle_dpad_hold(&save_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void leave_module(void) {
    play_sound(snd_back);
    save_access_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "access");
    if (config.boot.factory_reset) load_mux("installer");

    mux_input_stop();
}

static void handle_a(void) {
    if (hold_call) return;

    if (dialogue_active(&save_dlg)) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        dialogue_dismiss(&save_dlg);

        if (opt == mux_unsaved_save) save_access_options();

        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "access");
        if (config.boot.factory_reset) load_mux("installer");

        mux_input_stop();
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        dialogue_dismiss(&save_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_access_modified()) {
        dialogue_open(&save_dlg, &theme);
        return;
    }

    leave_module();
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

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

#define ACCESS(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_access, UDATA);
    ACCESS_ELEMENTS
#undef ACCESS

    overlay_display();
}

int muxaccess_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxaccess.title);
    init_muxaccess(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_access_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    orientation_introduce(mux_module, lang.muxaccess.title, lang.muxaccess.overview);

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
