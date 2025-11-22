#include "muxshare.h"
#include "ui/ui_muxcustom.h"

#define UI_COUNT 19

#define CUSTOM(NAME, UDATA) static int NAME##_original;
CUSTOM_ELEMENTS
#undef CUSTOM

static void list_nav_move(int steps, int direction);

static char theme_alt_original[MAX_BUFFER_SIZE];

struct theme_resolution {
    char *resolution;
    int value;
};

struct theme_resolution theme_resolutions[] = {
        {"640x480",  1},
        {"720x480",  2},
        {"720x576",  3},
        {"720x720",  4},
        {"1024x768", 5},
        {"1280x720", 6}
};

static int get_theme_resolution_value(char *resolution) {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (strcmp(resolution, theme_resolutions[i].resolution) == 0) return theme_resolutions[i].value;
    }

    return 0;
}

static void restore_theme_resolution(void) {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (theme_resolutions[i].value == config.SETTINGS.GENERAL.THEME_RESOLUTION) {
            int index = lv_dropdown_get_option_index(ui_droThemeResolution_custom, theme_resolutions[i].resolution);
            ThemeResolution_original = index <= 0 ? 0 : index;
            lv_dropdown_set_selected(ui_droThemeResolution_custom, ThemeResolution_original);
        }
    }
}

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblCatalogue_custom,       lang.MUXCUSTOM.HELP.CATALOGUE},
            {ui_lblConfig_custom,          lang.MUXCUSTOM.HELP.CONFIG},
            {ui_lblTheme_custom,           lang.MUXCUSTOM.HELP.THEME},
            {ui_lblThemeResolution_custom, lang.MUXCUSTOM.HELP.THEME_RES},
            {ui_lblThemeAlternate_custom,  lang.MUXCUSTOM.HELP.THEME_ALT},
            {ui_lblAnimation_custom,       lang.MUXCUSTOM.HELP.ANIMATION},
            {ui_lblMusic_custom,           lang.MUXCUSTOM.HELP.MUSIC},
            {ui_lblMusicVolume_custom,     lang.MUXCUSTOM.HELP.MUSIC_VOLUME},
            {ui_lblShuffle_custom,         lang.MUXCUSTOM.HELP.SHUFFLE},
            {ui_lblLaunchSwap_custom,      lang.MUXCUSTOM.HELP.LAUNCH_SWAP},
            {ui_lblBlackFade_custom,       lang.MUXCUSTOM.HELP.FADE},
            {ui_lblBoxArtImage_custom,     lang.MUXCUSTOM.HELP.BOX_ART},
            {ui_lblBoxArtAlign_custom,     lang.MUXCUSTOM.HELP.BOX_ALIGN},
            {ui_lblLaunchSplash_custom,    lang.MUXCUSTOM.HELP.SPLASH},
            {ui_lblGridModeContent_custom, lang.MUXCUSTOM.HELP.GRID_MODE_CONTENT},
            {ui_lblBoxArtHide_custom,      lang.MUXCUSTOM.HELP.BOX_HIDE},
            {ui_lblFont_custom,            lang.MUXCUSTOM.HELP.FONT},
            {ui_lblSound_custom,           lang.MUXCUSTOM.HELP.SOUND},
            {ui_lblChime_custom,           lang.MUXCUSTOM.HELP.CHIME},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static int populate_theme_alternates(void) {
    lv_dropdown_clear_options(ui_droThemeAlternate_custom);

    char alt_path[MAX_BUFFER_SIZE];
    snprintf(alt_path, sizeof(alt_path), "%s/alternate", STORAGE_THEME);

    struct dirent *entry;
    DIR *dir = opendir(alt_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char *filename = entry->d_name;
            size_t len = strlen(filename);

            if ((len > 4 && strcmp(str_tolower(filename + len - 4), ".ini") == 0) ||
                (len > 7 && strcmp(str_tolower(filename + len - 7), ".muxalt") == 0)) {
                char *name_without_ext = strip_ext(filename);
                if (!item_exists(items, item_count, name_without_ext)) {
                    add_item(&items, &item_count, name_without_ext, name_without_ext, "", ITEM);
                }
            }
        }

        closedir(dir);
        sort_items(items, item_count);

        for (int i = 0; i < item_count; i++) {
            lv_dropdown_add_option(ui_droThemeAlternate_custom, items[i].display_name, LV_DROPDOWN_POS_LAST);
        }

        free_items(&items, &item_count);
    }

    return lv_dropdown_get_option_cnt(ui_droThemeAlternate_custom);
}

static void init_dropdown_settings(void) {
#define CUSTOM(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_custom);
    CUSTOM_ELEMENTS
#undef CUSTOM
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *music_options[] = {
            lang.GENERIC.DISABLED,
            lang.MUXCUSTOM.MUSIC.GLOBAL,
            lang.MUXCUSTOM.MUSIC.THEME
    };

    char *sound_options[] = {
            lang.GENERIC.DISABLED,
            lang.MUXCUSTOM.SOUND.GLOBAL,
            lang.MUXCUSTOM.SOUND.THEME
    };

    char *boxart_image[] = {
            lang.MUXCUSTOM.BOX_ART.BEHIND,
            lang.MUXCUSTOM.BOX_ART.FRONT,
            lang.MUXCUSTOM.BOX_ART.FS_BEHIND,
            lang.MUXCUSTOM.BOX_ART.FS_FRONT,
            lang.GENERIC.DISABLED
    };

    char *boxart_align[] = {
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_MID,
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_MID,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_MID
    };

    char *font_options[] = {
            lang.MUXCUSTOM.FONT.LANG,
            lang.MUXCUSTOM.FONT.THEME
    };

    char *launch_swap_options[] = {
            lang.MUXCUSTOM.LAUNCH_SWAP.PRESS_A,
            lang.MUXCUSTOM.LAUNCH_SWAP.HOLD_A,
            lang.MUXCUSTOM.LAUNCH_SWAP.LOAD_STATE,
            lang.MUXCUSTOM.LAUNCH_SWAP.START_FRESH
    };

    INIT_OPTION_ITEM(-1, custom, Catalogue, lang.MUXCUSTOM.CATALOGUE, "catalogue", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, Config, lang.MUXCUSTOM.CONFIG, "config", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, Theme, lang.MUXCUSTOM.THEME, "theme", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, ThemeResolution, lang.MUXCUSTOM.THEME_RES, "resolution", NULL, 0);

    if (populate_theme_alternates() > 0) {
        INIT_OPTION_ITEM(-1, custom, ThemeAlternate, lang.MUXCUSTOM.THEME_ALT, "alternate", NULL, 0);
    } else {
        lv_obj_add_flag(ui_pnlThemeAlternate_custom, LV_OBJ_FLAG_HIDDEN);
    }

    INIT_OPTION_ITEM(-1, custom, Animation, lang.MUXCUSTOM.ANIMATION, "animation", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, Music, lang.MUXCUSTOM.MUSIC.TITLE, "music", music_options, 3);
    INIT_OPTION_ITEM(-1, custom, MusicVolume, lang.MUXCUSTOM.MUSIC.VOLUME, "musicvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, BlackFade, lang.MUXCUSTOM.FADE, "blackfade", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, LaunchSwap, lang.MUXCUSTOM.LAUNCH_SWAP.TITLE, "launch_swap", launch_swap_options, 4);
    INIT_OPTION_ITEM(-1, custom, Shuffle, lang.MUXCUSTOM.SHUFFLE, "shuffle", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, BoxArtImage, lang.MUXCUSTOM.BOX_ART.TITLE, "boxart", boxart_image, 5);
    INIT_OPTION_ITEM(-1, custom, BoxArtAlign, lang.MUXCUSTOM.BOX_ART.ALIGN.TITLE, "align", boxart_align, 9);
    INIT_OPTION_ITEM(-1, custom, LaunchSplash, lang.MUXCUSTOM.SPLASH, "splash", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, GridModeContent, lang.MUXCUSTOM.GRID_MODE_CONTENT, "gridmodecontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, BoxArtHide, lang.MUXCUSTOM.BOX_ART.HIDE_GRID_MODE, "boxarthide", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, Font, lang.MUXCUSTOM.FONT.TITLE, "font", font_options, 2);
    INIT_OPTION_ITEM(-1, custom, Sound, lang.MUXCUSTOM.SOUND.TITLE, "sound", sound_options, 3);
    INIT_OPTION_ITEM(-1, custom, Chime, lang.MUXCUSTOM.CHIME, "chime", disabled_enabled, 2);

    char *volume_values = generate_number_string(0, 100, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droMusicVolume_custom, volume_values);
    free(volume_values);

    lv_dropdown_clear_options(ui_droThemeResolution_custom);
    lv_dropdown_add_option(ui_droThemeResolution_custom, lang.MUXCUSTOM.SCREEN, LV_DROPDOWN_POS_LAST);

    char theme_device_folder[MAX_BUFFER_SIZE];
    for (int i = 0; i < A_SIZE(theme_resolutions); i++) {
        snprintf(theme_device_folder, sizeof(theme_device_folder), STORAGE_THEME "/%s",
                 theme_resolutions[i].resolution);

        if (directory_exist(theme_device_folder)) {
            lv_dropdown_add_option(ui_droThemeResolution_custom,
                                   theme_resolutions[i].resolution, LV_DROPDOWN_POS_LAST);
        }
    }

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
        apply_text_long_dot(&theme, ui_objects_panel[i], ui_objects[i]);
    }

    // Temporary removal of elements
    HIDE_OPTION_ITEM(custom, Animation);

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
}

static void check_focus(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblCatalogue_custom ||
        element_focused == ui_lblConfig_custom ||
        element_focused == ui_lblTheme_custom) {
        lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else if (element_focused == ui_lblMusicVolume_custom) {
        lv_label_set_text(ui_lblNavA, lang.GENERIC.SET);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

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
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;

    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void restore_custom_options(void) {
    snprintf(theme_alt_original, sizeof(theme_alt_original), "%s",
             str_replace(read_line_char_from(STORAGE_THEME "/active.txt", 1), "\r", ""));
    int32_t option_index = lv_dropdown_get_option_index(ui_droThemeAlternate_custom, theme_alt_original);
    if (option_index > 0) lv_dropdown_set_selected(ui_droThemeAlternate_custom, option_index);

    restore_theme_resolution();
    lv_dropdown_set_selected(ui_droBoxArtImage_custom, config.VISUAL.BOX_ART);
    lv_dropdown_set_selected(ui_droBoxArtAlign_custom, config.VISUAL.BOX_ART_ALIGN - 1);
    lv_dropdown_set_selected(ui_droAnimation_custom, config.VISUAL.BACKGROUNDANIMATION);
    lv_dropdown_set_selected(ui_droLaunchSplash_custom, config.VISUAL.LAUNCHSPLASH);
    lv_dropdown_set_selected(ui_droBoxArtHide_custom, config.VISUAL.BOX_ART_HIDE);
    lv_dropdown_set_selected(ui_droBlackFade_custom, config.VISUAL.BLACKFADE);
    lv_dropdown_set_selected(ui_droLaunchSwap_custom, config.VISUAL.LAUNCH_SWAP);
    lv_dropdown_set_selected(ui_droShuffle_custom, config.VISUAL.SHUFFLE);
    lv_dropdown_set_selected(ui_droGridModeContent_custom, config.VISUAL.GRID_MODE_CONTENT);
    lv_dropdown_set_selected(ui_droFont_custom, config.SETTINGS.ADVANCED.FONT);
    lv_dropdown_set_selected(ui_droMusic_custom, config.SETTINGS.GENERAL.BGM);
    lv_dropdown_set_selected(ui_droMusicVolume_custom, config.SETTINGS.GENERAL.BGMVOL);
    lv_dropdown_set_selected(ui_droSound_custom, config.SETTINGS.GENERAL.SOUND);
    lv_dropdown_set_selected(ui_droChime_custom, config.SETTINGS.GENERAL.CHIME);
}

static void save_custom_options(char *next_screen) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(custom, Animation, "visual/backgroundanimation", INT, 0);
    CHECK_AND_SAVE_STD(custom, Music, "settings/general/bgm", INT, 0);
    CHECK_AND_SAVE_STD(custom, MusicVolume, "settings/general/bgmvol", INT, 0);
    CHECK_AND_SAVE_STD(custom, BlackFade, "visual/blackfade", INT, 0);
    CHECK_AND_SAVE_STD(custom, LaunchSwap, "visual/launch_swap", INT, 0);
    CHECK_AND_SAVE_STD(custom, Shuffle, "visual/shuffle", INT, 0);
    CHECK_AND_SAVE_STD(custom, BoxArtImage, "visual/boxart", INT, 0);
    CHECK_AND_SAVE_STD(custom, BoxArtAlign, "visual/boxartalign", INT, 1);
    CHECK_AND_SAVE_STD(custom, LaunchSplash, "visual/launchsplash", INT, 0);
    CHECK_AND_SAVE_STD(custom, GridModeContent, "visual/gridmodecontent", INT, 0);
    CHECK_AND_SAVE_STD(custom, BoxArtHide, "visual/boxarthide", INT, 0);
    CHECK_AND_SAVE_STD(custom, Font, "settings/advanced/font", INT, 0);
    CHECK_AND_SAVE_STD(custom, Sound, "settings/general/sound", INT, 0);
    CHECK_AND_SAVE_STD(custom, Chime, "settings/general/chime", INT, 0);

    char theme_resolution[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_droThemeResolution_custom, theme_resolution, sizeof(theme_resolution));
    int idx_theme_resolution = get_theme_resolution_value(theme_resolution);

    if (lv_dropdown_get_selected(ui_droThemeResolution_custom) != ThemeResolution_original) {
        is_modified++;

        write_text_to_file(CONF_CONFIG_PATH "settings/general/theme_resolution", "w", INT, idx_theme_resolution);
        refresh_resolution = 1;
    }

    if (!lv_obj_has_flag(ui_pnlThemeAlternate_custom, LV_OBJ_FLAG_HIDDEN)) {
        char theme_alt[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_droThemeAlternate_custom, theme_alt, sizeof(theme_alt));

        if (strcasecmp(theme_alt, theme_alt_original) != 0) {
            write_text_to_file(STORAGE_THEME "/active.txt", "w", CHAR, theme_alt);

            char theme_alt_archive[MAX_BUFFER_SIZE];
            snprintf(theme_alt_archive, sizeof(theme_alt_archive), STORAGE_THEME "/alternate/%s.muxalt",
                     theme_alt);

            if (file_exist(theme_alt_archive)) {
                LOG_INFO(mux_module, "Extracting Alternative Theme: %s", theme_alt_archive)
                extract_archive(theme_alt_archive, next_screen);
            } else {
                char png_bootlogo[MAX_BUFFER_SIZE];
                snprintf(png_bootlogo, sizeof(png_bootlogo), STORAGE_THEME "/%simage/bootlogo.png",
                         mux_dimension);
                if (!file_exist(png_bootlogo)) {
                    snprintf(png_bootlogo, sizeof(png_bootlogo), STORAGE_THEME "/image/bootlogo.png");
                }
                if (file_exist(png_bootlogo)) update_bootlogo(next_screen);
            }

            static char rgb_script[MAX_BUFFER_SIZE];
            snprintf(rgb_script, sizeof(rgb_script), STORAGE_THEME "/alternate/rgb/%s/rgbconf.sh",
                     theme_alt);
            if (file_exist(rgb_script)) {
                if (device.BOARD.RGB && config.SETTINGS.GENERAL.RGB) {
                    const char *args[] = {rgb_script, NULL};
                    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
                }

                static char rgb_script_dest[MAX_BUFFER_SIZE];
                snprintf(rgb_script_dest, sizeof(rgb_script_dest), STORAGE_THEME "/rgb/rgbconf.sh");

                create_directories(strip_dir(rgb_script_dest));
                write_text_to_file(rgb_script_dest, "w", CHAR, read_all_char_from(rgb_script));
            }
        }
    }

    if (lv_dropdown_get_selected(ui_droMusic_custom) != Music_original) {
        is_modified++;

        int idx_music = lv_dropdown_get_selected(ui_droMusic_custom);
        if (!idx_music) {
            if (!is_silence_playing) play_silence_bgm();
        } else {
            if (idx_music != Music_original || is_silence_playing) init_fe_bgm(&fe_bgm, idx_music, 1);
            set_bgm_volume(lv_dropdown_get_selected(ui_droMusicVolume_custom));
        }
    }

    if (lv_dropdown_get_selected(ui_droSound_custom) != Sound_original) {
        is_modified++;

        int idx_sound = lv_dropdown_get_selected(ui_droSound_custom);
        init_fe_snd(&fe_snd, idx_sound, idx_sound);
    }

    if (is_modified > 0) run_tweak_script();

    if (file_exist(MUOS_PIK_LOAD)) remove(MUOS_PIK_LOAD);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    struct {
        const char *mux_name;
        const char *launch;
        int16_t *kiosk_flag;
    } elements[] = {
            {"theme",     "/theme",            &kiosk.CUSTOM.THEME},
            {"catalogue", "package/catalogue", &kiosk.CUSTOM.CATALOGUE},
            {"config",    "package/config",    &kiosk.CUSTOM.RACONFIG}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].mux_name) == 0) {
            if (is_ksk(*elements[i].kiosk_flag)) {
                kiosk_denied();
                return;
            }

            save_custom_options("custom");

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, elements[i].mux_name);
            write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, elements[i].launch);

            play_sound(SND_CONFIRM);

            toast_message(lang.GENERIC.LOADING, FOREVER);
            lv_obj_move_foreground(ui_pnlMessage);

            // Refresh and add a small delay to actually display the message!
            lv_task_handler();
            usleep(256);

            load_mux("picker");

            close_input();
            mux_input_stop();

            break;
        }
    }

    if (element_focused == ui_lblMusicVolume_custom) {
        toast_message(lang.MUXCUSTOM.MUSIC.SET, SHORT);
        set_bgm_volume(lv_dropdown_get_selected(ui_droMusicVolume_custom));
    } else if (element_focused == ui_lblThemeAlternate_custom) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "alternate");
    } else {
        handle_option_next();
    }
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

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");
    save_custom_options("config");

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
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

    check_focus();

#define CUSTOM(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_custom, UDATA);
    CUSTOM_ELEMENTS
#undef CUSTOM

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

int muxcustom_main(void) {
    init_module("muxcustom");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCUSTOM.TITLE);
    init_muxcustom(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_custom_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

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
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
