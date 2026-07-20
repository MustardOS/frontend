#include "muxshare.h"
#include "ui/ui_muxcustom.h"

static int save_mode = 0;
static mux_dialogue save_dlg;

static int msg_mode = 0;
static mux_dialogue msg_dlg;

static void show_message_dialog(void) {
    msg_mode = 1;
    dialogue_show(&msg_dlg);
}

static void hide_message_dialog(void) {
    msg_mode = 0;
    dialogue_hide(&msg_dlg);
}

static int pending_submenu = 0;
static char pending_pdi[64];
static char pending_pik[MAX_BUFFER_SIZE];
static char pending_mux_load[32];

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

#define CUSTOM(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(CUSTOM_ELEMENTS) };
#undef CUSTOM

#define CUSTOM(NAME, UDATA) static int NAME##_original;
CUSTOM_ELEMENTS
#undef CUSTOM

static int any_custom_modified(void) {
#define CUSTOM(NAME, UDATA)                                                                                            \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_custom) != NAME##_original) return 1;
    CUSTOM_ELEMENTS
#undef CUSTOM
    return 0;
}

static void list_nav_move(int steps, int direction);

static char theme_alt_original[MAX_BUFFER_SIZE];

static int alt_theme_count = 0;

typedef struct theme_resolution {
    char *resolution;
    int value;
} theme_resolution;

theme_resolution theme_resolutions[] = {
    {"640x480", 1}, {"720x480", 2}, {"720x576", 3}, {"720x720", 4}, {"1024x768", 5}, {"1280x720", 6}, {"1920x1080", 7},
};

static int get_theme_resolution_value(const char *resolution) {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (strcmp(resolution, theme_resolutions[i].resolution) == 0) return theme_resolutions[i].value;
    }

    return 0;
}

static void restore_theme_resolution(void) {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (theme_resolutions[i].value == config.settings.general.theme_resolution) {
            const int index =
                lv_dropdown_get_option_index(ui_dro_theme_resolution_custom, theme_resolutions[i].resolution);
            theme_resolution_original = index <= 0 ? 0 : index;
            lv_dropdown_set_selected(ui_dro_theme_resolution_custom, theme_resolution_original);
        }
    }
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define CUSTOM(NAME, UDATA) {UDATA, lang.muxcustom.help.NAME},
        CUSTOM_ELEMENTS
#undef CUSTOM
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int visible_theme_alternate(void) {
    return alt_theme_count > 0 && !lv_obj_has_flag(ui_pnl_theme_alternate_custom, LV_OBJ_FLAG_HIDDEN);
}

static void populate_theme_alternates(void) {
    lv_dropdown_clear_options(ui_dro_theme_alternate_custom);

    char alt_path[MAX_BUFFER_SIZE];
    snprintf(alt_path, sizeof(alt_path), "%s/alternate", theme_base);

    struct dirent *entry;
    DIR *dir = opendir(alt_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            const char *filename = entry->d_name;
            const size_t len = strlen(filename);

            if ((len > 4 && strcmp(str_tolower(filename + len - 4), ".ini") == 0)
                || (len > 7 && strcmp(str_tolower(filename + len - 7), ".muxalt") == 0)) {
                const char *name_without_ext = strip_ext(filename);
                if (!item_exists(items, item_count, name_without_ext)) {
                    add_item(&items, &item_count, name_without_ext, name_without_ext, "", ITEM);
                }
            }
        }

        closedir(dir);
        sort_items(items, item_count);

        for (int i = 0; i < item_count; i++) {
            lv_dropdown_add_option(ui_dro_theme_alternate_custom, items[i].display_name, LV_DROPDOWN_POS_LAST);
        }

        free_items(&items, &item_count);
    }

    alt_theme_count = lv_dropdown_get_option_cnt(ui_dro_theme_alternate_custom);
}

static void init_dropdown_settings(void) {
#define CUSTOM(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_custom);
    CUSTOM_ELEMENTS
#undef CUSTOM

    music_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_music_volume_custom), 0, 100);
    sound_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_sound_volume_custom), 0, 100);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *music_options[] = {lang.generic.disabled, lang.muxcustom.music.global, lang.muxcustom.music.theme};

    char *sound_options[] = {lang.generic.disabled, lang.muxcustom.sound.global, lang.muxcustom.sound.theme};

    char *theme_scaling_options[] = {
        lang.muxcustom.scaling.no_scale, lang.muxcustom.scaling.scale, lang.muxcustom.scaling.stretch
    };

    char *background_scale_options[] = {
        lang.muxcustom.scaling.no_scale, lang.muxcustom.scaling.scale, lang.muxcustom.scaling.stretch
    };

    INIT_OPTION_ITEM(-1, custom, catalogue, lang.muxcustom.catalogue, "catalogue", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, config, lang.muxcustom.config, "config", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, content_options, lang.muxcustom.content, "content", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, font, lang.muxcustom.font.title, "font", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_opt, lang.muxcustom.themeopt, "themeopt", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme, lang.muxcustom.theme, "theme", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_resolution, lang.muxcustom.themeresolution, "resolution", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_scaling, lang.muxcustom.themescaling, "scaling", theme_scaling_options, 3);

    if (alt_theme_count > 0) {
        INIT_OPTION_ITEM(-1, custom, theme_alternate, lang.muxcustom.themealternate, "alternate", NULL, 0);
    } else {
        lv_obj_add_flag(ui_pnl_theme_alternate_custom, LV_OBJ_FLAG_HIDDEN);
    }

    INIT_OPTION_ITEM(-1, custom, video_wallpaper, lang.muxcustom.videowallpaper, "videowallpaper", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, custom, background_scale, lang.muxcustom.backgroundscale, "backgroundscale", background_scale_options, 3
    );
    INIT_OPTION_ITEM(-1, custom, music, lang.muxcustom.music.title, "music", music_options, 3);
    INIT_OPTION_ITEM(-1, custom, music_volume, lang.muxcustom.music.volume, "musicvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, black_fade, lang.muxcustom.blackfade, "blackfade", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, sound, lang.muxcustom.sound.title, "sound", sound_options, 3);
    INIT_OPTION_ITEM(-1, custom, sound_volume, lang.muxcustom.sound.volume, "soundvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, chime, lang.muxcustom.chime, "chime", disabled_enabled, 2);

    char *pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_lbl_music_volume_custom, ui_dro_music_volume_custom, pct_values);
    apply_theme_list_drop_down(&theme, ui_lbl_sound_volume_custom, ui_dro_sound_volume_custom, pct_values);
    free(pct_values);

    lv_dropdown_clear_options(ui_dro_theme_resolution_custom);
    lv_dropdown_add_option(ui_dro_theme_resolution_custom, lang.muxcustom.screen, LV_DROPDOWN_POS_LAST);

    char theme_device_folder[MAX_BUFFER_SIZE];
    for (int i = 0; i < A_SIZE(theme_resolutions); i++) {
        snprintf(
            theme_device_folder, sizeof(theme_device_folder), "%s/%s", theme_base, theme_resolutions[i].resolution
        );
        if (dir_exist(theme_device_folder))
            lv_dropdown_add_option(
                ui_dro_theme_resolution_custom, theme_resolutions[i].resolution, LV_DROPDOWN_POS_LAST
            );
    }

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 1);

    list_nav_move(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1);
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_catalogue_custom || e_focused == ui_lbl_config_custom
        || e_focused == ui_lbl_content_options_custom || e_focused == ui_lbl_font_custom
        || e_focused == ui_lbl_theme_opt_custom || e_focused == ui_lbl_theme_custom) {
        lv_label_set_text(ui_lbl_nav_a, lang.generic.select);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else if (e_focused == ui_lbl_theme_alternate_custom || e_focused == ui_lbl_music_volume_custom
               || e_focused == ui_lbl_sound_volume_custom) {
        lv_label_set_text(ui_lbl_nav_a, lang.generic.set);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
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
}

static void restore_custom_options(void) {
    char theme_active_txt_path[MAX_BUFFER_SIZE];
    snprintf(theme_active_txt_path, sizeof(theme_active_txt_path), "%s/active.txt", theme_base);

    char *active_line = read_line_char_from(theme_active_txt_path, 1);
    char *trimmed_line = str_replace(active_line, "\r", "");
    free(active_line);

    snprintf(theme_alt_original, sizeof(theme_alt_original), "%s", trimmed_line ? trimmed_line : "");
    free(trimmed_line);
    const int32_t option_index = lv_dropdown_get_option_index(ui_dro_theme_alternate_custom, theme_alt_original);
    if (option_index >= 0) lv_dropdown_set_selected(ui_dro_theme_alternate_custom, option_index);

    restore_theme_resolution();
    lv_dropdown_set_selected(ui_dro_video_wallpaper_custom, config.visual.video_wallpaper);
    lv_dropdown_set_selected(ui_dro_background_scale_custom, config.visual.background_scale);
    lv_dropdown_set_selected(ui_dro_black_fade_custom, config.visual.blackfade);
    lv_dropdown_set_selected(ui_dro_music_custom, config.settings.general.bgm);
    lv_dropdown_set_selected(ui_dro_music_volume_custom, int_to_pct(config.settings.general.bgmvol, 0, 100));
    lv_dropdown_set_selected(ui_dro_sound_custom, config.settings.general.sound);
    lv_dropdown_set_selected(ui_dro_sound_volume_custom, int_to_pct(config.settings.general.soundvol, 0, 100));
    lv_dropdown_set_selected(ui_dro_chime_custom, config.settings.general.chime);
    lv_dropdown_set_selected(ui_dro_theme_scaling_custom, config.settings.general.theme_scaling);
}

static int save_custom_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(custom, video_wallpaper, "visual/video_wallpaper", INT, 0);
    CHECK_AND_SAVE_STD(custom, background_scale, "visual/background_scale", INT, 0);
    CHECK_AND_SAVE_STD(custom, music, "settings/general/bgm", INT, 0);
    CHECK_AND_SAVE_PCT(custom, music_volume, "settings/general/bgmvol", INT, 0, 100);
    CHECK_AND_SAVE_STD(custom, black_fade, "visual/blackfade", INT, 0);
    CHECK_AND_SAVE_STD(custom, sound, "settings/general/sound", INT, 0);
    CHECK_AND_SAVE_PCT(custom, sound_volume, "settings/general/soundvol", INT, 0, 100);
    CHECK_AND_SAVE_STD(custom, chime, "settings/general/chime", INT, 0);
    CHECK_AND_SAVE_STD(custom, theme_scaling, "settings/general/theme_scaling", INT, 0);

    char theme_resolution[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_dro_theme_resolution_custom, theme_resolution, sizeof(theme_resolution));
    const int idx_theme_resolution = get_theme_resolution_value(theme_resolution);

    if (lv_dropdown_get_selected(ui_dro_theme_resolution_custom) != theme_resolution_original) {
        is_modified++;

        write_text_to_file(CONF_CONFIG_PATH "settings/general/theme_resolution", "w", INT, idx_theme_resolution);
        refresh_resolution = 1;
    }

    if (lv_dropdown_get_selected(ui_dro_theme_scaling_custom) != theme_scaling_original) {
        refresh_resolution = 1;
    }

    if (!lv_obj_has_flag(ui_pnl_theme_alternate_custom, LV_OBJ_FLAG_HIDDEN)) {
        char theme_alt[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_dro_theme_alternate_custom, theme_alt, sizeof(theme_alt));

        if (strcasecmp(theme_alt, theme_alt_original) != 0) {
            char theme_active_txt_path[MAX_BUFFER_SIZE];
            snprintf(theme_active_txt_path, sizeof(theme_active_txt_path), "%s/active.txt", theme_base);
            write_text_to_file(theme_active_txt_path, "w", CHAR, theme_alt);

            char theme_alt_archive[MAX_BUFFER_SIZE];
            snprintf(theme_alt_archive, sizeof(theme_alt_archive), "%s/alternate/%s.muxalt", theme_base, theme_alt);

            if (file_exist(theme_alt_archive)) {
                LOG_INFO(mux_module, "Extracting Alternative Theme: %s", theme_alt_archive);
                if (extract_zip_to_dir(theme_alt_archive, theme_base) != MUX_EXTRACT_OK) return -1;
            }

            write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);

            if (config.settings.rgb.mode == RGB_MODE_THEME_SUPPLIED) {
                const char *argv[2];
                argv[0] = RGBLED_BIN;
                argv[1] = "restore";
                run_exec(argv, 2, 0, 0, NULL, NULL);
            }
        }
    }

    if (lv_dropdown_get_selected(ui_dro_music_custom) != music_original) {
        is_modified++;

        const int idx_music = lv_dropdown_get_selected(ui_dro_music_custom);
        if (!idx_music) {
            if (!is_silence_playing) play_silence_bgm();
        } else {
            if (idx_music != music_original || is_silence_playing) init_fe_bgm(&fe_bgm, idx_music, 1);
        }
    }

    if (lv_dropdown_get_selected(ui_dro_music_volume_custom) != music_volume_original) {
        set_bgm_volume(lv_dropdown_get_selected(ui_dro_music_volume_custom));
    }

    if (lv_dropdown_get_selected(ui_dro_sound_custom) != sound_original) {
        is_modified++;

        const int idx_sound = lv_dropdown_get_selected(ui_dro_sound_custom);
        init_fe_snd(&fe_snd, idx_sound, idx_sound);
    }

    if (lv_dropdown_get_selected(ui_dro_sound_volume_custom) != sound_volume_original) {
        set_nav_volume(lv_dropdown_get_selected(ui_dro_sound_volume_custom));
    }

    if (is_modified > 0) run_tweak_script(lang.generic.saving);

    if (file_exist(MUOS_PIK_LOAD)) remove(MUOS_PIK_LOAD);
    return 0;
}

typedef enum {
    menu_option = 0,
    menu_theme,
    menu_catalogue,
    menu_config,
    menu_content,
    menu_font,
    menu_themeopt,
    menu_music_volume,
    menu_sound_volume,
    menu_theme_alternate,
} menu_action;

typedef int (*visible_fn)(void);

typedef struct {
    const char *mux_name;
    const char *launch_path;
    int16_t *kiosk_flag;
    menu_action action;
    visible_fn visible;
} menu_entry;

static int16_t kiosk_pass = 0;

static const menu_entry custom_menu_entries[ui_count_dynamic] = {
    {"catalogue", "package/catalogue", &kiosk.custom.catalogue, menu_catalogue, NULL},
    {"config", "package/config", &kiosk.custom.raconfig, menu_config, NULL},
    {"content", NULL, &kiosk_pass, menu_content, NULL},   // Content Options
    {"font", NULL, &kiosk_pass, menu_font, NULL},         // Font Settings
    {"themeopt", NULL, &kiosk_pass, menu_themeopt, NULL}, // Theme Options
    {"theme", "/theme", &kiosk.custom.theme, menu_theme, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Theme Resolution
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Theme Scaling
    {NULL, NULL, &kiosk_pass, menu_theme_alternate, visible_theme_alternate},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Video Wallpaper
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Background Scale
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Background Music
    {NULL, NULL, &kiosk_pass, menu_music_volume, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Black Fade Animation
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Navigation Sound
    {NULL, NULL, &kiosk_pass, menu_sound_volume, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // Startup Chime
};

// Shared shape for every "navigate to a submenu" menu action: kiosk check, then either defer via the
// pending-save dialog (if options are unsaved) or save immediately and load the target submenu.
static void navigate_to_submenu(const menu_entry *entry, const char *target_mux) {
    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_custom_modified()) {
        snprintf(pending_pdi, sizeof(pending_pdi), "%s", entry->mux_name);

        if (entry->launch_path) {
            snprintf(pending_pik, sizeof(pending_pik), "%s", entry->launch_path);
        } else {
            pending_pik[0] = '\0';
        }

        snprintf(pending_mux_load, sizeof(pending_mux_load), "%s", target_mux);
        pending_submenu = 1;
        show_save_dialog();

        return;
    }

    save_custom_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, entry->mux_name);
    if (entry->launch_path) write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, entry->launch_path);

    play_sound(snd_confirm);
    toast_message(lang.generic.loading, tst_wait_f);

    load_mux(target_mux);

    mux_input_stop();
}

static void handle_a(void) {
    if (msg_mode || msgbox_active || hold_call) return;

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (pending_submenu) {
            pending_submenu = 0;

            if (opt == mux_unsaved_save && save_custom_options() < 0) {
                show_message_dialog();
                return;
            }

            play_sound(snd_confirm);

            toast_message(lang.generic.loading, tst_wait_f);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, pending_pdi);

            if (pending_pik[0]) write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, pending_pik);

            load_mux(pending_mux_load);
            mux_input_stop();

            return;
        }

        if (opt == mux_unsaved_save && save_custom_options() < 0) {
            show_message_dialog();
            return;
        }

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

        mux_input_stop();

        return;
    }

    SELECT_VISIBLE_ENTRY(custom_menu_entries, entry);

    switch (entry->action) {
        case menu_catalogue:
        case menu_config:
        case menu_theme:
            navigate_to_submenu(entry, entry->action == menu_theme ? "theme" : "picker");
            break;
        case menu_content:
            navigate_to_submenu(entry, "content");
            break;
        case menu_font:
            navigate_to_submenu(entry, "font");
            break;
        case menu_themeopt:
            navigate_to_submenu(entry, "themeopt");
            break;
        case menu_music_volume:
            toast_message(lang.muxcustom.music.set, tst_wait_s);
            set_bgm_volume(pct_to_int(lv_dropdown_get_selected(ui_dro_music_volume_custom), 0, 100));
            music_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_music_volume_custom), 0, 100);
            break;
        case menu_sound_volume:
            toast_message(lang.muxcustom.sound.set, tst_wait_s);
            set_nav_volume(pct_to_int(lv_dropdown_get_selected(ui_dro_sound_volume_custom), 0, 100));
            sound_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_sound_volume_custom), 0, 100);
            break;
        case menu_theme_alternate: {
            char theme_alt[MAX_BUFFER_SIZE];
            lv_dropdown_get_selected_str(ui_dro_theme_alternate_custom, theme_alt, sizeof(theme_alt));
            if (strcasecmp(theme_alt, theme_alt_original) == 0) break;

            if (save_custom_options() < 0) {
                show_message_dialog();
                break;
            }

            mux_input_flush_queue();

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "alternate");
            init_dropdown_settings();
            load_mux("custom");

            mux_input_stop();
            break;
        }
        case menu_option:
            handle_option_next();
            break;
        default:
            break;
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (msg_mode) {
        play_sound(snd_back);
        hide_message_dialog();
        return;
    }

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_custom_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

    if (save_custom_options() < 0) {
        show_message_dialog();
        return;
    }

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
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    check_focus();

#define CUSTOM(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_custom, UDATA);
    CUSTOM_ELEMENTS
#undef CUSTOM

    overlay_display();
}

int muxcustom_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxcustom.title);
    init_muxcustom(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();

    populate_theme_alternates();
    init_navigation_group();

    restore_custom_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );
    dialogue_init_message(
        &msg_dlg, &theme, ui_screen, lang.generic.warning, NULL, lang.generic.unsafe_archive, lang.generic.cancel
    );
    init_timer(ui_gen_refresh_task, NULL);

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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
