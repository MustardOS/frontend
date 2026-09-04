#include "muxshare.h"
#include "../common/ui/list_frame.h"
#include "../common/ui/orientation.h"
#include "ui/ui_muxcustom.h"

static mux_dialogue save_dlg;

static mux_dialogue msg_dlg;

static int pending_submenu = 0;
static char pending_pdi[64];
static char pending_pik[MAX_BUFFER_SIZE];
static char pending_mux_load[32];

#define CUSTOM(NAME, UDATA) 1,
#define VISUAL(NAME, UDATA) 1,
#define FONT(NAME, UDATA)   1,
enum { ui_count_dynamic = E_SIZE(CUSTOM_ELEMENTS) + E_SIZE(VISUAL_ELEMENTS) + E_SIZE(FONT_ELEMENTS) };
#undef FONT
#undef VISUAL
#undef CUSTOM

#define CUSTOM(NAME, UDATA) static int NAME##_original;
CUSTOM_ELEMENTS
#undef CUSTOM

#define VISUAL(NAME, UDATA) static int NAME##_original;
VISUAL_ELEMENTS
#undef VISUAL

#define FONT(NAME, UDATA) static int NAME##_original;
FONT_ELEMENTS
#undef FONT

static char font_name_saved[MAX_BUFFER_SIZE];
static int has_language_type;
static int has_theme_type;
static int dropdown_to_canonical[4];
static int has_custom_type;
static int num_type_options;

static int overlay_count;
static int has_theme_overlay;

static int any_custom_modified(void) {
#define CUSTOM(NAME, UDATA)                                                                                            \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_custom) != NAME##_original) return 1;
    CUSTOM_ELEMENTS
#undef CUSTOM
#define VISUAL(NAME, UDATA)                                                                                            \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_visual) != NAME##_original) return 1;
    VISUAL_ELEMENTS
#undef VISUAL
#define FONT(NAME, UDATA)                                                                                              \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_font) != NAME##_original) return 1;
    FONT_ELEMENTS
#undef FONT
    return 0;
}

static void list_nav_move(int steps, int direction);

static const int16_t glyph_size_values[] = {-2, 0, -1, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128};

static void restore_glyph_dropdown(lv_obj_t *dropdown, const int16_t stored) {
    uint32_t idx = 0;

    for (size_t i = 0; i < A_SIZE(glyph_size_values); i++) {
        if (glyph_size_values[i] == stored) {
            idx = (uint32_t) i;
            break;
        }
    }

    lv_dropdown_set_selected(dropdown, idx);
}

static void
save_glyph_dropdown(const lv_obj_t *dropdown, const int original, const char *key, int16_t *cfg, int *is_modified) {
    const uint32_t sel = lv_dropdown_get_selected(dropdown);
    if ((int) sel == original) return;

    int16_t val = 0;
    if (sel < A_SIZE(glyph_size_values)) val = glyph_size_values[sel];

    write_text_to_file(key, "w", INT, (int) val);

    *cfg = val;
    (*is_modified)++;
}

static void restore_width_dropdown(lv_obj_t *dropdown, const int16_t stored) {
    uint32_t idx = 0;
    if (stored >= 10 && stored <= 100) idx = (uint32_t) (stored - 9);
    lv_dropdown_set_selected(dropdown, idx);
}

static void
save_width_dropdown(const lv_obj_t *dropdown, const int original, const char *key, int16_t *cfg, int *is_modified) {
    const uint32_t sel = lv_dropdown_get_selected(dropdown);
    if ((int) sel == original) return;

    const int16_t val = (int16_t) (sel >= 1 ? (int) sel + 9 : 0);
    write_text_to_file(key, "w", INT, (int) val);

    *cfg = val;
    (*is_modified)++;
}

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

static int detect_language_type(void) {
    char dir[MAX_BUFFER_SIZE];
    snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.settings.general.language);

    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return 0;

    int found = 0;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        const size_t len = strlen(name);
        if (!found && len > 4 && strcasecmp(name + len - 4, ".ttf") == 0) found = 1;
        free(entries[i]);
    }

    free(entries);
    return found;
}

static int type_to_canonical(const uint32_t dropdown_idx) {
    if (dropdown_idx < (uint32_t) num_type_options) return dropdown_to_canonical[dropdown_idx];

    return dropdown_to_canonical[num_type_options - 1];
}

static uint32_t type_to_dropdown(const int canonical) {
    for (int i = 0; i < num_type_options; i++) {
        if (dropdown_to_canonical[i] == canonical) return (uint32_t) i;
    }

    return (uint32_t) (num_type_options - 1);
}

// Lists every TTF the user has placed under MUOS/font, by filename alone
static int populate_user_font_names(void) {
    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    int added = 0;
    for (size_t m = 0; m < A_SIZE(mounts); m++) {
        if (!mounts[m] || !*mounts[m]) continue;

        char dir[MAX_BUFFER_SIZE];
        snprintf(dir, sizeof(dir), "%s/%s", mounts[m], USER_FONTS);
        remove_double_slashes(dir);

        struct dirent **entries;
        const int n = scandir(dir, &entries, NULL, alphasort);
        if (n < 0) continue;

        for (int i = 0; i < n; i++) {
            const char *file = entries[i]->d_name;
            const size_t len = strlen(file);

            if (len > 4 && strcasecmp(file + len - 4, ".ttf") == 0) {
                char name[MAX_BUFFER_SIZE];
                snprintf(name, sizeof(name), "%.*s", (int) (len - 4), file);

                // The same font on two storages should only be offered once
                if (lv_dropdown_get_option_index(ui_dro_font_name_font, name) < 0) {
                    lv_dropdown_add_option(ui_dro_font_name_font, name, LV_DROPDOWN_POS_LAST);
                    added++;
                }
            }

            free(entries[i]);
        }

        free(entries);
    }

    return added;
}

static void select_font_name(const char *wanted) {
    int32_t idx = wanted && *wanted ? lv_dropdown_get_option_index(ui_dro_font_name_font, wanted) : -1;
    if (idx < 0) idx = lv_dropdown_get_option_index(ui_dro_font_name_font, DEFAULT_FONT_NAME);
    lv_dropdown_set_selected(ui_dro_font_name_font, idx >= 0 ? (uint32_t) idx : 0);
}

static void populate_font_names(void) {
    char previous[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_dro_font_name_font, previous, sizeof(previous));

    lv_dropdown_clear_options(ui_dro_font_name_font);

    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));

    if (canonical_type == 3) {
        if (!populate_user_font_names())
            lv_dropdown_add_option(ui_dro_font_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);

        select_font_name(previous);
        return;
    }

    char dir[MAX_BUFFER_SIZE];
    if (canonical_type == 0) {
        snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.settings.general.language);
    } else {
        snprintf(dir, sizeof(dir), "%s", INTERNAL_FONTS);
    }

    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);

    if (n < 0) {
        lv_dropdown_add_option(ui_dro_font_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);
        select_font_name(previous);
        return;
    }

    int added = 0;
    for (int i = 0; i < n; i++) {
        const char *font_name = entries[i]->d_name;
        const size_t len = strlen(font_name);
        if (len > 4 && strcasecmp(font_name + len - 4, ".ttf") == 0) {
            char name_no_ext[MAX_BUFFER_SIZE];
            snprintf(name_no_ext, sizeof(name_no_ext), "%.*s", (int) (len - 4), font_name);
            lv_dropdown_add_option(ui_dro_font_name_font, name_no_ext, LV_DROPDOWN_POS_LAST);
            added++;
        }
        free(entries[i]);
    }
    free(entries);

    if (!added) lv_dropdown_add_option(ui_dro_font_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);

    select_font_name(previous);
}

static void apply_current_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));

    uint32_t idx = lv_dropdown_get_selected(ui_dro_list_size_font);
    config.settings.font.list_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_header_size_font);
    config.settings.font.header_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_footer_size_font);
    config.settings.font.footer_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_panel_size_font);
    config.settings.font.panel_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    lv_dropdown_get_selected_str(ui_dro_font_name_font, config.settings.font.name, sizeof(config.settings.font.name));

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts_preview();
    lv_obj_invalidate(ui_screen);

#define FONT(NAME, UDATA)                                                                                              \
    do {                                                                                                               \
        const lv_font_t *_f = lv_obj_get_style_text_font(ui_dro_##NAME##_font, LV_PART_MAIN);                          \
        lv_coord_t _h = lv_font_get_line_height(_f);                                                                   \
        lv_obj_set_height(ui_dro_##NAME##_font, _h);                                                                   \
        if (theme.list_default.label_long_mode != LV_LABEL_LONG_WRAP) lv_obj_set_height(ui_lbl_##NAME##_font, _h);     \
    } while (0);
    FONT_ELEMENTS
#undef FONT
}

static void revert_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_to_canonical((uint32_t) type_original);
    config.settings.font.list_size =
        (int16_t) (list_size_original < font_size_count ? font_size_values[list_size_original] : 0);
    config.settings.font.header_size =
        (int16_t) (header_size_original < font_size_count ? font_size_values[header_size_original] : 0);
    config.settings.font.footer_size =
        (int16_t) (footer_size_original < font_size_count ? font_size_values[footer_size_original] : 0);
    config.settings.font.panel_size =
        (int16_t) (panel_size_original < font_size_count ? font_size_values[panel_size_original] : 0);
    snprintf(config.settings.font.name, sizeof(config.settings.font.name), "%s", font_name_saved);

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts_preview();
    lv_obj_invalidate(ui_screen);
}

// Shows the overlay change straight away without committing it
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

static int font_locked = 0;

// A compiled theme font renders at one fixed size, so its settings are shown but dimmed
static void font_apply_lock(void) {
    font_locked = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font)) == 1 && !theme_font_is_scalable();

    const lv_opa_t opa = font_locked ? LV_OPA_50 : LV_OPA_COVER;

#define LOCK_ROW(NAME)                                                                                                 \
    do {                                                                                                               \
        lv_obj_set_style_text_opa(ui_lbl_##NAME##_font, opa, MU_OBJ_MAIN_DEFAULT);                                     \
        lv_obj_set_style_text_opa(ui_dro_##NAME##_font, opa, MU_OBJ_MAIN_DEFAULT);                                     \
        lv_obj_set_style_img_opa(ui_ico_##NAME##_font, opa, MU_OBJ_MAIN_DEFAULT);                                      \
    } while (0)

    LOCK_ROW(font_name);
    LOCK_ROW(list_size);
    LOCK_ROW(header_size);
    LOCK_ROW(footer_size);
    LOCK_ROW(panel_size);
#undef LOCK_ROW

    // A dimmed row stays on screen but drops out of navigation
    lv_obj_t *const rows[] = {
        ui_lbl_font_name_font, ui_lbl_list_size_font, ui_lbl_header_size_font, ui_lbl_footer_size_font,
        ui_lbl_panel_size_font
    };
    for (size_t i = 0; i < A_SIZE(rows); i++)
        list_frame_set_inert(list_frame_row_of(rows[i]), font_locked);
}

// A dimmed row is inert rather than merely faded
static int font_row_locked(const lv_obj_t *focused) {
    if (!font_locked) return 0;

    return focused == ui_dro_font_name_font || focused == ui_dro_list_size_font || focused == ui_dro_header_size_font
           || focused == ui_dro_footer_size_font || focused == ui_dro_panel_size_font;
}

// Any font row drives the live preview
static int font_row_focused(void) {
    const lv_obj_t *focused = lv_group_get_focused(ui_group_value);

#define FONT(NAME, UDATA)                                                                                              \
    if (focused == ui_dro_##NAME##_font) return 1;
    FONT_ELEMENTS
#undef FONT

    return 0;
}

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
    if (list_frame_focused()) {
        list_frame_help();

        return;
    }

    const struct help_msg help_messages[] = {
#define CUSTOM(NAME, UDATA) {UDATA, lang.muxcustom.help.NAME},
        CUSTOM_ELEMENTS
#undef CUSTOM
#define VISUAL(NAME, UDATA) {UDATA, lang.muxvisual.help.NAME},
            VISUAL_ELEMENTS
#undef VISUAL
#define FONT(NAME, UDATA) {UDATA, lang.muxfont.help.NAME},
                FONT_ELEMENTS
#undef FONT
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
                    add_item(&items, &item_count, name_without_ext, name_without_ext, "", content_type_item);
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
#define VISUAL(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_visual);
    VISUAL_ELEMENTS
#undef VISUAL
#define FONT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_font);
    FONT_ELEMENTS
#undef FONT

    lv_dropdown_get_selected_str(ui_dro_font_name_font, font_name_saved, sizeof(font_name_saved));

    font_apply_lock();

    music_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_music_volume_custom), 0, 100);
    sound_volume_original = pct_to_int(lv_dropdown_get_selected(ui_dro_sound_volume_custom), 0, 100);
}

static void init_navigation_group(void) {
    char *music_options[] = {lang.generic.disabled, lang.muxcustom.music.global, lang.muxcustom.music.theme};

    char *sound_options[] = {lang.generic.disabled, lang.muxcustom.sound.global, lang.muxcustom.sound.theme};

    char *theme_scaling_options[] = {
        lang.muxcustom.scaling.no_scale, lang.muxcustom.scaling.scale, lang.muxcustom.scaling.stretch
    };

    char *background_scale_options[] = {
        lang.muxcustom.scaling.no_scale, lang.muxcustom.scaling.scale, lang.muxcustom.scaling.stretch
    };

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

    char *element_transition[] = {lang.muxvisual.transition.fade_in,     lang.muxvisual.transition.slide_right,
                                  lang.muxvisual.transition.slide_left,  lang.muxvisual.transition.slide_up,
                                  lang.muxvisual.transition.slide_down,  lang.muxvisual.transition.bounce_right,
                                  lang.muxvisual.transition.bounce_left, lang.muxvisual.transition.bounce_up,
                                  lang.muxvisual.transition.bounce_down, lang.muxvisual.transition.shoot_right,
                                  lang.muxvisual.transition.shoot_left,  lang.muxvisual.transition.shoot_up,
                                  lang.muxvisual.transition.shoot_down,  lang.generic.disabled};

    char *selection_animation[] = {lang.generic.disabled, lang.generic.minimal, lang.generic.low,
                                   lang.generic.medium,   lang.generic.high,    lang.generic.maximum};

    char *notify_time_options[] = {
        lang.generic.brief, lang.generic.normal, lang.generic.long_wait, lang.generic.extended
    };

    char *page_skip_options[] = {lang.muxvisual.skip.page, lang.muxvisual.skip.letter};

    char *group_content_options[] = {
        lang.generic.disabled, lang.muxvisual.group.single, lang.muxvisual.group.two, lang.muxvisual.group.three,
        lang.muxvisual.group.four
    };

    char *shake_direction[] = {
        lang.generic.up, lang.generic.down, lang.generic.left, lang.generic.right, lang.generic.all
    };

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
    char *save_screenshot_options[] = {
        lang.generic.disabled, lang.muxcontent.save_screenshot.collection, lang.muxcontent.save_screenshot.history,
        lang.muxcontent.save_screenshot.both
    };
    char *video_preview_options[] = {
        lang.generic.disabled, lang.muxcontent.video_preview.delay_3, lang.muxcontent.video_preview.delay_5,
        lang.muxcontent.video_preview.delay_10
    };

    has_language_type = detect_language_type();
    has_theme_type = theme_has_font();
    has_custom_type = user_font_count() > 0;

    char *all_type_options[] = {
        lang.muxfont.type_options.language, lang.muxfont.type_options.theme, lang.muxfont.type_options.internal,
        lang.muxfont.type_options.custom
    };

    num_type_options = 0;
    if (has_language_type) dropdown_to_canonical[num_type_options++] = 0;
    if (has_theme_type) dropdown_to_canonical[num_type_options++] = 1;
    dropdown_to_canonical[num_type_options++] = 2;
    if (has_custom_type) dropdown_to_canonical[num_type_options++] = 3;

    char *type_options[4];
    for (int i = 0; i < num_type_options; i++) {
        type_options[i] = all_type_options[dropdown_to_canonical[i]];
    }

    char *size_options = generate_number_string(6, 64, 2, lang.muxfont.size_default, NULL, NULL, 0);

    INIT_OPTION_ITEM(-1, visual, battery, lang.muxvisual.battery, "battery", battery_display, 3);
    INIT_OPTION_ITEM(-1, visual, clock, lang.muxvisual.clock, "clock", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, network, lang.muxvisual.network, "network", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, bluetooth, lang.muxvisual.bluetooth, "bluetooth", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, sort_order, lang.muxvisual.sortorder, "sortorder", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, tag_order, lang.muxvisual.tagorder, "tagorder", hidden_visible, 2);
    INIT_OPTION_ITEM(-1, visual, header_title, lang.muxvisual.headertitle, "headertitle", hidden_visible, 2);
    INIT_OPTION_ITEM(
        -1, visual, element_transition, lang.muxvisual.elementtransition, "elementtransition", element_transition, 14
    );
    INIT_OPTION_ITEM(
        -1, visual, selection_animation, lang.muxvisual.selectionanimation, "selectionanimation", selection_animation, 6
    );
    INIT_OPTION_ITEM(-1, visual, selection_style, lang.muxvisual.selectionstyle, "selectionstyle", shake_direction, 5);
    INIT_OPTION_ITEM(-1, visual, list_glyph, lang.muxvisual.listglyph, "listglyph", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, render_shadows, lang.muxvisual.rendershadows, "rendershadows", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, notify_time, lang.muxvisual.notifytime, "notifytime", notify_time_options, 4);
    INIT_OPTION_ITEM(-1, visual, overlay_image, lang.muxvisual.overlay.image, "overlayimage", NULL, 0);
    INIT_OPTION_ITEM(
        -1, visual, overlay_transparency, lang.muxvisual.overlay.transparency, "overlaytransparency", NULL, 0
    );
    INIT_OPTION_ITEM(-1, visual, name_scroll, lang.muxvisual.namescroll, "namescroll", scroll_mode, 3);
    INIT_OPTION_ITEM(
        -1, visual, label_scroll_speed, lang.muxvisual.labelscrollspeed, "labelscrollspeed", label_scroll_speed, 4
    );
    INIT_OPTION_ITEM(-1, visual, name, lang.muxvisual.name.title, "name", visual_names, 4);
    INIT_OPTION_ITEM(-1, visual, dash, lang.muxvisual.dash, "dash", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, the_title_format, lang.muxvisual.thetitleformat, "thetitleformat", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(-1, visual, friendly_folder, lang.muxvisual.friendlyfolder, "friendlyfolder", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, title_include_root_drive, lang.muxvisual.titleincluderootdrive, "titleincluderootdrive",
        disabled_enabled, 2
    );
    INIT_OPTION_ITEM(-1, font, type, lang.muxfont.type, "type", type_options, num_type_options);
    INIT_OPTION_ITEM(-1, font, font_name, lang.muxfont.font_name, "fontname", NULL, 0);
    INIT_OPTION_ITEM(-1, font, list_size, lang.muxfont.list_size, "listsize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, header_size, lang.muxfont.header_size, "headersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, footer_size, lang.muxfont.footer_size, "footersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, panel_size, lang.muxfont.panel_size, "panelsize", NULL, 0);
    INIT_OPTION_ITEM(
        -1, visual, folder_item_count, lang.muxvisual.folderitemcount, "folderitemcount", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, menu_counter_folder, lang.muxvisual.menucounterfolder, "menucounterfolder", hidden_visible, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, menu_counter_file, lang.muxvisual.menucounterfile, "menucounterfile", hidden_visible, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, display_empty_folder, lang.muxvisual.displayemptyfolder, "displayemptyfolder", hidden_visible, 2
    );
    INIT_OPTION_ITEM(-1, visual, hidden, lang.muxvisual.hidden, "hidden", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, group_content, lang.muxvisual.groupcontent, "groupcontent", group_content_options, 5);
    INIT_OPTION_ITEM(-1, custom, sort, lang.muxvisual.sort, "sort", NULL, 0);
    INIT_OPTION_ITEM(
        -1, visual, content_collect, lang.muxvisual.contentcollect, "contentcollect", toggle_icon_visible, 3
    );
    INIT_OPTION_ITEM(
        -1, visual, content_history, lang.muxvisual.contenthistory, "contenthistory", toggle_icon_visible, 3
    );
    INIT_OPTION_ITEM(-1, visual, mixed_content, lang.muxvisual.mixedcontent, "mixedcontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, forward_history, lang.muxvisual.forwardhistory, "forwardhistory", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, content_width, lang.muxcontent.full_width, "width", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, video_preview, lang.muxcontent.video_preview.title, "videopreview", video_preview_options, 4
    );
    INIT_OPTION_ITEM(-1, visual, page_skip, lang.muxvisual.pageskip, "pageskip", page_skip_options, 2);
    INIT_OPTION_ITEM(-1, visual, box_art, lang.muxcontent.box_art.title, "boxart", boxart_image, 5);
    INIT_OPTION_ITEM(-1, visual, box_art_align, lang.muxcontent.box_art.align.title, "align", boxart_align, 9);
    INIT_OPTION_ITEM(
        -1, visual, box_art_transition, lang.muxcontent.box_art.transition.title, "boxarttransition", boxart_transition,
        14
    );
    INIT_OPTION_ITEM(-1, visual, box_art_scale, lang.muxcontent.box_art.scale, "boxartscale", NULL, 0);
    INIT_OPTION_ITEM(-1, visual, box_art_padding, lang.muxcontent.box_art.padding, "boxartpadding", NULL, 0);
    INIT_OPTION_ITEM(
        -1, visual, box_art_placeholder, lang.muxcontent.box_art.placeholder, "boxartplaceholder", disabled_enabled, 2
    );
    INIT_OPTION_ITEM(
        -1, visual, save_screenshot, lang.muxcontent.save_screenshot.title, "savescreenshot", save_screenshot_options, 4
    );
    INIT_OPTION_ITEM(-1, visual, grid_mode_content, lang.muxcontent.grid_mode, "gridmodecontent", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, box_art_hide, lang.muxcontent.grid_mode_art, "boxarthide", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, visual, launch_swap, lang.muxcontent.launch_swap.title, "launch_swap", launch_swap_options, 4);
    INIT_OPTION_ITEM(-1, visual, launchsplash, lang.muxcontent.launch_splash, "splash", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, visual, pickles_startup_messages, lang.muxcustom.pickles_startup_messages, "picklesstartupmessages",
        disabled_enabled, 2
    );
    INIT_OPTION_ITEM(-1, visual, shuffle, lang.muxcontent.shuffle, "shuffle", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, catalogue, lang.muxcustom.catalogue, "catalogue", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, config, lang.muxcustom.config, "config", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, logo, lang.muxcustom.logo, "logo", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme, lang.muxcustom.theme, "theme", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_resolution, lang.muxcustom.themeresolution, "resolution", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_scaling, lang.muxcustom.themescaling, "scaling", theme_scaling_options, 3);
    INIT_OPTION_ITEM(-1, custom, theme_alternate, lang.muxcustom.themealternate, "alternate", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, header_height, lang.muxthemeopt.header_height, "headerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, footer_height, lang.muxthemeopt.footer_height, "footerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, content_item_count, lang.muxthemeopt.content_item_count, "count", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, label_width, lang.muxthemeopt.label_width, "labelwidth", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, glyph_list, lang.muxthemeopt.glyph_list, "glyphlist", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, glyph_header, lang.muxthemeopt.glyph_header, "glyphheader", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, glyph_footer, lang.muxthemeopt.glyph_footer, "glyphfooter", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, glyph_grid, lang.muxthemeopt.glyph_grid, "glyphgrid", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, video_wallpaper, lang.muxcustom.videowallpaper, "videowallpaper", disabled_enabled, 2);
    INIT_OPTION_ITEM(
        -1, custom, background_scale, lang.muxcustom.backgroundscale, "backgroundscale", background_scale_options, 3
    );
    INIT_OPTION_ITEM(-1, custom, black_fade, lang.muxcustom.blackfade, "blackfade", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, music, lang.muxcustom.music.title, "music", music_options, 3);
    INIT_OPTION_ITEM(-1, custom, music_volume, lang.muxcustom.music.volume, "musicvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, sound, lang.muxcustom.sound.title, "sound", sound_options, 3);
    INIT_OPTION_ITEM(-1, custom, sound_volume, lang.muxcustom.sound.volume, "soundvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, chime, lang.muxcustom.chime, "chime", disabled_enabled, 2);

    populate_font_names();

    apply_theme_list_drop_down(&theme, ui_lbl_list_size_font, ui_dro_list_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_header_size_font, ui_dro_header_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_footer_size_font, ui_dro_footer_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_panel_size_font, ui_dro_panel_size_font, size_options);

    free(size_options);

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
    apply_theme_list_drop_down(
        &theme, ui_lbl_overlay_transparency_visual, ui_dro_overlay_transparency_visual, pct_values
    );

    char boxart_scale_values[MAX_BUFFER_SIZE];
    snprintf(
        boxart_scale_values, sizeof(boxart_scale_values), "%s\n%s", lang.generic.disabled,
        generate_number_string(1, 100, 1, NULL, "%", NULL, 1)
    );
    apply_theme_list_drop_down(&theme, ui_lbl_box_art_scale_visual, ui_dro_box_art_scale_visual, boxart_scale_values);

    char boxart_padding_values[MAX_BUFFER_SIZE];
    snprintf(
        boxart_padding_values, sizeof(boxart_padding_values), "%s\n%s", lang.generic.disabled,
        generate_number_string(1, 100, 1, NULL, "%", NULL, 1)
    );
    apply_theme_list_drop_down(
        &theme, ui_lbl_box_art_padding_visual, ui_dro_box_art_padding_visual, boxart_padding_values
    );

    char *height_options = generate_number_string(0, 64, 1, lang.muxthemeopt.size_default, NULL, NULL, 0);
    char *count_options = generate_number_string(1, 64, 1, lang.muxthemeopt.size_default, NULL, NULL, 0);
    char *width_options = generate_number_string(10, 100, 1, lang.muxthemeopt.size_default, "%", NULL, 1);

    char glyph_options[256];
    snprintf(
        glyph_options, sizeof(glyph_options), "%s\n%s\n%s\n8\n12\n16\n20\n24\n28\n32\n36\n40\n48\n56\n64\n80\n96\n128",
        lang.muxthemeopt.size_default, lang.muxthemeopt.glyph_auto, lang.muxthemeopt.glyph_native
    );

    apply_theme_list_drop_down(&theme, ui_lbl_header_height_custom, ui_dro_header_height_custom, height_options);
    apply_theme_list_drop_down(&theme, ui_lbl_footer_height_custom, ui_dro_footer_height_custom, height_options);
    apply_theme_list_drop_down(
        &theme, ui_lbl_content_item_count_custom, ui_dro_content_item_count_custom, count_options
    );
    apply_theme_list_drop_down(&theme, ui_lbl_label_width_custom, ui_dro_label_width_custom, width_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_list_custom, ui_dro_glyph_list_custom, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_header_custom, ui_dro_glyph_header_custom, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_footer_custom, ui_dro_glyph_footer_custom, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_grid_custom, ui_dro_glyph_grid_custom, glyph_options);

    free(height_options);
    free(count_options);
    free(width_options);

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

    if (alt_theme_count <= 0) HIDE_OPTION_ITEM(custom, theme_alternate);

    reset_ui_groups();

    static const list_frame frames[] = {
        {lang.muxvisual.section.header_bar, 0, 7},  {lang.muxvisual.section.appearance, 7, 8},
        {lang.muxvisual.section.labels, 15, 7},     {lang.muxvisual.section.font, 22, 6},
        {lang.muxvisual.section.folders, 28, 6},    {lang.muxvisual.section.content, 34, 8},
        {lang.muxvisual.section.box_art, 42, 8},    {lang.muxvisual.section.launching, 50, 4},
        {lang.muxcustom.section.packages, 54, 3},   {lang.muxcustom.section.theme, 57, 4},
        {lang.muxcustom.section.layout, 61, 4},     {lang.muxcustom.section.glyphs, 65, 4},
        {lang.muxcustom.section.background, 69, 3}, {lang.muxcustom.section.audio, 72, 5},
    };

    list_frame_init(
        &theme, ui_pnl_content, frames, A_SIZE(frames), ui_objects_panel, ui_objects, ui_objects_glyph,
        ui_objects_value, ui_count_dynamic
    );
    list_frame_apply();

    list_nav_move(list_frame_restore(), +1);
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_catalogue_custom || e_focused == ui_lbl_config_custom || e_focused == ui_lbl_sort_custom
        || e_focused == ui_lbl_logo_custom || e_focused == ui_lbl_theme_custom) {
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

    footer_nav_check_scroll();
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_frame_prev(void) {
    if (msgbox_active || dialogue_active(&save_dlg)) return;

    if (list_frame_move(-1)) {
        play_sound(snd_option);
        check_focus();
    }
}

static void handle_frame_next(void) {
    if (msgbox_active || dialogue_active(&save_dlg)) return;

    if (list_frame_move(+1)) {
        play_sound(snd_option);
        check_focus();
    }
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

    if (list_frame_focused()) {
        if (list_frame_move(-1)) {
            play_sound(snd_option);
            check_focus();
        }

        return;
    }

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    if (font_row_locked(focused)) return;

    move_option(focused, -1);

    if (focused == ui_dro_type_font) {
        populate_font_names();
        font_apply_lock();
        list_frame_apply();
        gen_step_movement(1, +1, 2, 0, 0);
    }

    if (font_row_focused()) apply_current_font_settings();

    refresh_overlay_preview();
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

    if (list_frame_focused()) {
        if (list_frame_move(+1)) {
            play_sound(snd_option);
            check_focus();
        }

        return;
    }

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    if (font_row_locked(focused)) return;

    move_option(focused, +1);

    if (focused == ui_dro_type_font) {
        populate_font_names();
        font_apply_lock();
        list_frame_apply();
        gen_step_movement(1, +1, 2, 0, 0);
    }

    if (font_row_focused()) apply_current_font_settings();

    refresh_overlay_preview();
}

static void restore_custom_options(void) {
    int canonical = config.settings.advanced.font;
    if (!has_theme_type && canonical == 1) canonical = 2;
    if (!has_language_type && canonical == 0) canonical = has_theme_type ? 1 : 2;

    config.settings.advanced.font = (int16_t) canonical;
    lv_dropdown_set_selected(ui_dro_type_font, type_to_dropdown(canonical));

    populate_font_names();
    select_font_name(config.settings.font.name);

    map_drop_down_to_index(ui_dro_list_size_font, config.settings.font.list_size, font_size_values, font_size_count, 0);
    map_drop_down_to_index(
        ui_dro_header_size_font, config.settings.font.header_size, font_size_values, font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_footer_size_font, config.settings.font.footer_size, font_size_values, font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_panel_size_font, config.settings.font.panel_size, font_size_values, font_size_count, 0
    );

#define VISUAL(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_visual, config.visual.NAME);
    VISUAL_CONFIG_ELEMENTS
#undef VISUAL

    {
        const int ddr = overlay_config_to_dropdown(config.visual.overlay_image);
        lv_dropdown_set_selected(ui_dro_overlay_image_visual, ddr < 0 || ddr >= overlay_count ? 0 : ddr);
    }

    lv_dropdown_set_selected(
        ui_dro_overlay_transparency_visual, int_to_pct(config.visual.overlay_transparency, 0, 255)
    );

    lv_dropdown_set_selected(ui_dro_box_art_visual, config.visual.box_art);
    lv_dropdown_set_selected(
        ui_dro_box_art_align_visual, config.visual.box_art_align > 0 ? config.visual.box_art_align - 1 : 0
    );
    lv_dropdown_set_selected(ui_dro_content_width_visual, config.visual.content_width);
    lv_dropdown_set_selected(ui_dro_launchsplash_visual, config.visual.launchsplash);
    lv_dropdown_set_selected(ui_dro_grid_mode_content_visual, config.visual.grid_mode_content);
    lv_dropdown_set_selected(ui_dro_box_art_hide_visual, 1 - config.visual.box_art_hide);

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

    lv_dropdown_set_selected(ui_dro_header_height_custom, (uint32_t) (config.settings.themeopt.header_height + 1));
    lv_dropdown_set_selected(ui_dro_footer_height_custom, (uint32_t) (config.settings.themeopt.footer_height + 1));
    lv_dropdown_set_selected(ui_dro_content_item_count_custom, (uint32_t) config.settings.themeopt.content_item_count);

    restore_glyph_dropdown(ui_dro_glyph_list_custom, config.settings.themeopt.glyph_size_list);
    restore_glyph_dropdown(ui_dro_glyph_header_custom, config.settings.themeopt.glyph_size_header);
    restore_glyph_dropdown(ui_dro_glyph_footer_custom, config.settings.themeopt.glyph_size_footer);
    restore_glyph_dropdown(ui_dro_glyph_grid_custom, config.settings.themeopt.glyph_size_grid);
    restore_width_dropdown(ui_dro_label_width_custom, config.settings.themeopt.label_width);
}

static int save_custom_options(void) {
    int is_modified = 0;
    int save_failed = 0;

    CHECK_AND_SAVE_STD(visual, battery, "visual/battery", INT, 0);
    CHECK_AND_SAVE_STD(visual, clock, "visual/clock", INT, 0);
    CHECK_AND_SAVE_STD(visual, network, "visual/network", INT, 0);
    CHECK_AND_SAVE_STD(visual, bluetooth, "visual/bluetooth", INT, 0);
    CHECK_AND_SAVE_STD(visual, sort_order, "visual/sortorder", INT, 0);
    CHECK_AND_SAVE_STD(visual, tag_order, "visual/tagorder", INT, 0);
    CHECK_AND_SAVE_STD(visual, header_title, "visual/headertitle", INT, 0);
    CHECK_AND_SAVE_STD(visual, element_transition, "visual/elementtransition", INT, 0);
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
    CHECK_AND_SAVE_STD(visual, notify_time, "visual/notifytime", INT, 0);

    {
        const int oi_current = lv_dropdown_get_selected(ui_dro_overlay_image_visual);
        if (oi_current != overlay_image_original) {
            is_modified++;
            if (!write_text_to_file(
                    CONF_CONFIG_PATH "visual/overlayimage", "w", INT, overlay_dropdown_to_config(oi_current)
                ))
                save_failed++;
        }
    }

    {
        const int ot_current = lv_dropdown_get_selected(ui_dro_overlay_transparency_visual);
        if (ot_current != overlay_transparency_original) {
            is_modified++;
            if (!write_text_to_file(
                    CONF_CONFIG_PATH "visual/overlaytransparency", "w", INT, pct_to_int(ot_current, 0, 255)
                ))
                save_failed++;
        }
    }

    CHECK_AND_SAVE_STD(visual, launch_swap, "visual/launch_swap", INT, 0);
    CHECK_AND_SAVE_STD(visual, shuffle, "visual/shuffle", INT, 0);
    CHECK_AND_SAVE_STD(visual, box_art, "visual/boxart", INT, 0);
    CHECK_AND_SAVE_STD(visual, box_art_align, "visual/boxartalign", INT, 1);
    CHECK_AND_SAVE_STD(visual, content_width, "visual/contentwidth", INT, 0);
    CHECK_AND_SAVE_STD(visual, page_skip, "visual/pageskip", INT, 0);
    CHECK_AND_SAVE_STD(visual, group_content, "visual/groupcontent", INT, 0);
    CHECK_AND_SAVE_STD(visual, launchsplash, "visual/launchsplash", INT, 0);
    CHECK_AND_SAVE_STD(visual, pickles_startup_messages, "visual/pickles_startup_messages", INT, 0);
    CHECK_AND_SAVE_STD(visual, grid_mode_content, "visual/gridmodecontent", INT, 0);

    // Stored the other way round to how it reads on screen
    if ((int) lv_dropdown_get_selected(ui_dro_box_art_hide_visual) != box_art_hide_original) {
        is_modified++;
        if (!write_text_to_file(
                CONF_CONFIG_PATH "visual/boxarthide", "w", INT,
                1 - (int) lv_dropdown_get_selected(ui_dro_box_art_hide_visual)
            ))
            save_failed++;
    }

    CHECK_AND_SAVE_STD(visual, box_art_transition, "visual/boxarttransition", INT, 0);
    CHECK_AND_SAVE_STD(visual, box_art_scale, "visual/boxartscale", INT, 0);
    CHECK_AND_SAVE_STD(visual, box_art_padding, "visual/boxartpadding", INT, 0);
    CHECK_AND_SAVE_STD(visual, box_art_placeholder, "visual/boxartplaceholder", INT, 0);
    CHECK_AND_SAVE_STD(visual, save_screenshot, "visual/savescreenshot", INT, 0);
    CHECK_AND_SAVE_STD(visual, video_preview, "visual/videopreview", INT, 0);

    const int modified_before_font = is_modified;

    if ((int) config.settings.advanced.font != type_to_canonical((uint32_t) type_original)) {
        is_modified++;
        if (!write_text_to_file(CONF_CONFIG_PATH "settings/advanced/font", "w", INT, config.settings.advanced.font))
            save_failed++;
    }
    CHECK_AND_SAVE_VAL(font, list_size, "settings/font/list_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, header_size, "settings/font/header_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, footer_size, "settings/font/footer_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, panel_size, "settings/font/panel_size", INT, font_size_values);

    if (config.settings.advanced.font != 1) {
        char name_current[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_dro_font_name_font, name_current, sizeof(name_current));
        if (strcasecmp(name_current, font_name_saved) != 0) {
            is_modified++;
            if (!write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, name_current)) save_failed++;
        }
    }

    if (is_modified != modified_before_font) refresh_resolution = 1;

    CHECK_AND_SAVE_STD(custom, video_wallpaper, "visual/video_wallpaper", INT, 0);
    CHECK_AND_SAVE_STD(custom, background_scale, "visual/background_scale", INT, 0);
    CHECK_AND_SAVE_STD(custom, music, "settings/general/bgm", INT, 0);
    CHECK_AND_SAVE_PCT(custom, music_volume, "settings/general/bgmvol", INT, 0, 100);
    CHECK_AND_SAVE_STD(custom, black_fade, "visual/blackfade", INT, 0);
    CHECK_AND_SAVE_STD(custom, sound, "settings/general/sound", INT, 0);
    CHECK_AND_SAVE_PCT(custom, sound_volume, "settings/general/soundvol", INT, 0, 100);
    CHECK_AND_SAVE_STD(custom, chime, "settings/general/chime", INT, 0);
    CHECK_AND_SAVE_STD(custom, theme_scaling, "settings/general/theme_scaling", INT, 0);
    CHECK_AND_SAVE_STD(custom, header_height, "settings/theme/header_height", INT, -1);
    CHECK_AND_SAVE_STD(custom, footer_height, "settings/theme/footer_height", INT, -1);
    CHECK_AND_SAVE_STD(custom, content_item_count, "settings/theme/content_item_count", INT, 0);

    save_glyph_dropdown(
        ui_dro_glyph_list_custom, glyph_list_original, CONF_CONFIG_PATH "settings/theme/glyph_size_list",
        &config.settings.themeopt.glyph_size_list, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_header_custom, glyph_header_original, CONF_CONFIG_PATH "settings/theme/glyph_size_header",
        &config.settings.themeopt.glyph_size_header, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_footer_custom, glyph_footer_original, CONF_CONFIG_PATH "settings/theme/glyph_size_footer",
        &config.settings.themeopt.glyph_size_footer, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_grid_custom, glyph_grid_original, CONF_CONFIG_PATH "settings/theme/glyph_size_grid",
        &config.settings.themeopt.glyph_size_grid, &is_modified
    );
    save_width_dropdown(
        ui_dro_label_width_custom, label_width_original, CONF_CONFIG_PATH "settings/theme/label_width",
        &config.settings.themeopt.label_width, &is_modified
    );

    char theme_resolution[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_dro_theme_resolution_custom, theme_resolution, sizeof(theme_resolution));
    const int idx_theme_resolution = get_theme_resolution_value(theme_resolution);

    if (lv_dropdown_get_selected(ui_dro_theme_resolution_custom) != theme_resolution_original) {
        is_modified++;

        if (!write_text_to_file(CONF_CONFIG_PATH "settings/general/theme_resolution", "w", INT, idx_theme_resolution))
            save_failed++;
        refresh_resolution = 1;
    }

    if (lv_dropdown_get_selected(ui_dro_theme_scaling_custom) != theme_scaling_original) {
        refresh_resolution = 1;
    }

    if (!lv_obj_has_flag(ui_pnl_theme_alternate_custom, LV_OBJ_FLAG_HIDDEN)) {
        char theme_alt[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_dro_theme_alternate_custom, theme_alt, sizeof(theme_alt));

        if (strcasecmp(theme_alt, theme_alt_original) != 0) {
            is_modified++;
            refresh_resolution = 1;

            char theme_active_txt_path[MAX_BUFFER_SIZE];
            snprintf(theme_active_txt_path, sizeof(theme_active_txt_path), "%s/active.txt", theme_base);
            if (!write_text_to_file(theme_active_txt_path, "w", CHAR, theme_alt)) save_failed++;

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

    if (is_modified > 0) {
        refresh_config = 1;
        refresh_device = 1;
        refresh_kiosk = 1;

        if (refresh_resolution && file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);

        run_tweak_script(lang.generic.saving);
    }

    REPORT_SAVE_FAILURE();

    if (file_exist(MUOS_PIK_LOAD)) remove(MUOS_PIK_LOAD);
    return 0;
}

typedef enum {
    menu_option = 0,
    menu_theme,
    menu_catalogue,
    menu_config,
    menu_sort,
    menu_logo,
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
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // battery
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // clock
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // network
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // bluetooth
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // sort_order
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // tag_order
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // header_title
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // element_transition
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // selection_animation
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // selection_style
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // list_glyph
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // render_shadows
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // notify_time
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // overlay_image
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // overlay_transparency
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // name_scroll
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // label_scroll_speed
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // name
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // dash
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // friendly_folder
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // the_title_format
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // title_include_root_drive
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // type
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // font_name
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // list_size
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // header_size
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // footer_size
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // panel_size
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // folder_item_count
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // menu_counter_folder
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // menu_counter_file
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // display_empty_folder
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // hidden
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // group_content
    {"sort", NULL, &kiosk_pass, menu_sort, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // content_collect
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // content_history
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // mixed_content
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // forward_history
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // content_width
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // video_preview
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // page_skip
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_align
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_transition
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_scale
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_padding
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_placeholder
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // save_screenshot
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // grid_mode_content
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // box_art_hide
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // launch_swap
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // launchsplash
    {NULL, NULL, &kiosk_pass, menu_option, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // shuffle
    {"catalogue", "package/catalogue", &kiosk.custom.catalogue, menu_catalogue, NULL},
    {"config", "package/config", &kiosk.custom.raconfig, menu_config, NULL},
    {"logo", NULL, &kiosk_pass, menu_logo, NULL},
    {"theme", "/theme", &kiosk.custom.theme, menu_theme, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // theme_resolution
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // theme_scaling
    {NULL, NULL, &kiosk_pass, menu_theme_alternate, visible_theme_alternate},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // header_height
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // footer_height
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // content_item_count
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // label_width
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // glyph_list
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // glyph_header
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // glyph_footer
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // glyph_grid
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // video_wallpaper
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // background_scale
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // black_fade
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // music
    {NULL, NULL, &kiosk_pass, menu_music_volume, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // sound
    {NULL, NULL, &kiosk_pass, menu_sound_volume, NULL},
    {NULL, NULL, &kiosk_pass, menu_option, NULL}, // chime
};

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
        dialogue_open(&save_dlg, &theme);

        return;
    }

    save_custom_options();

    list_frame_remember(lv_group_get_focused(ui_group));
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, entry->mux_name);
    if (entry->launch_path) write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, entry->launch_path);

    play_sound(snd_confirm);
    toast_message(lang.generic.loading, tst_wait_f);

    load_mux(target_mux);

    mux_input_stop();
}

static void handle_a(void) {
    if (dialogue_active(&msg_dlg) || msgbox_active || hold_call) return;

    if (dialogue_active(&save_dlg)) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;

        dialogue_mark_silent(&save_dlg);
        dialogue_dismiss(&save_dlg);

        if (pending_submenu) {
            pending_submenu = 0;

            if (opt != mux_unsaved_save) revert_font_settings();

            if (opt == mux_unsaved_save && save_custom_options() < 0) {
                dialogue_open(&msg_dlg, &theme);
                return;
            }

            play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
            toast_message(lang.generic.loading, tst_wait_f);
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, pending_pdi);

            if (pending_pik[0]) write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, pending_pik);

            load_mux(pending_mux_load);
            mux_input_stop();

            return;
        }

        if (opt != mux_unsaved_save) revert_font_settings();

        if (opt == mux_unsaved_save && save_custom_options() < 0) {
            dialogue_open(&msg_dlg, &theme);
            return;
        }

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        list_frame_remember_section();
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

        mux_input_stop();

        return;
    }

    const int row = list_frame_current_row();
    if (row < 0 || row >= (int) A_SIZE(custom_menu_entries)) return;

    const menu_entry *entry = &custom_menu_entries[row];
    if (entry->visible && !entry->visible()) return;

    switch (entry->action) {
        case menu_catalogue:
        case menu_config:
            navigate_to_submenu(entry, "picker");
            break;

        case menu_logo:
            navigate_to_submenu(entry, "logo");
            break;

        case menu_sort:
            navigate_to_submenu(entry, "sort");
            break;

        case menu_theme:
            navigate_to_submenu(entry, entry->action == menu_theme ? "theme" : "picker");
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
                dialogue_open(&msg_dlg, &theme);
                break;
            }

            mux_input_flush_queue();

            list_frame_remember(ui_lbl_theme_alternate_custom);
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

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&msg_dlg)) {
        dialogue_mark_cancelled(&msg_dlg);
        dialogue_dismiss(&msg_dlg);
        return;
    }

    if (dialogue_active(&save_dlg)) {
        dialogue_mark_cancelled(&save_dlg);
        dialogue_dismiss(&save_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_custom_modified()) {
        dialogue_open(&save_dlg, &theme);
        return;
    }

    play_sound(snd_back);

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

    if (save_custom_options() < 0) {
        dialogue_open(&msg_dlg, &theme);
        return;
    }

    mux_input_stop();
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

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || dialogue_active(&save_dlg)) return;

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
                [mux_input_x] = handle_x,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_frame_prev,
                [mux_input_r1] = handle_frame_next,
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
            [mux_input_l1] = handle_frame_prev,
            [mux_input_r1] = handle_frame_next,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxcustom.title, lang.muxcustom.overview);

    mux_input_task(&input_opts);

    return 0;
}
