#include <math.h>
#include <dirent.h>
#include "muxshare.h"
#include "ui/ui_muxcolour.h"
#include <common/ui/list_frame.h>
#include <common/ui/osk.h>

#define COLOUR(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(COLOUR_ELEMENTS) };
#undef COLOUR

enum {
    field_mode = 0,
    field_red,
    field_green,
    field_blue,
    field_light,
    field_gradient_red,
    field_gradient_green,
    field_gradient_blue,
    field_gradient_light,
    field_count
};
enum { mode_theme = 0, mode_custom, mode_automatic, mode_gradient, mode_none };
enum { preset_reset = 0, preset_first_scheme };

#define SCHEME_PRESETS    OPT_SHARE_PATH "colour"
#define SCHEME_USER       RUN_STORAGE_PATH "colour"
#define SCHEME_MAX        32
#define SCHEME_NAME_LIMIT 48

#define CHANNEL_MAX 255
#define LIGHT_MAX   100
#define BAR_TRACK   0x303030

static const uint32_t channel_fills[] = {0xD03838, 0x38B048, 0x3874E0};

static const char *field_names[field_count] = {"mode",           "red",           "green",
                                               "blue",           "light",         "gradient_red",
                                               "gradient_green", "gradient_blue", "gradient_light"};

static const char *row_udata[] = {
#define COLOUR(NAME, UDATA) UDATA,
    COLOUR_ELEMENTS
#undef COLOUR
};

static const int modes_plain[] = {mode_theme, mode_custom};
static const int modes_text[] = {mode_theme, mode_custom, mode_automatic};
static const int modes_fill[] = {mode_theme, mode_custom, mode_none};
static const int modes_background[] = {mode_theme, mode_custom, mode_gradient};

static const struct {
    const int *list;
    int count;
} role_modes[colour_role_count] = {
    [colour_role_highlight] = {modes_fill, A_SIZE(modes_fill)},
    [colour_role_highlight_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_highlight_shadow] = {modes_text, A_SIZE(modes_text)},
    [colour_role_background] = {modes_background, A_SIZE(modes_background)},
    [colour_role_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_text_shadow] = {modes_text, A_SIZE(modes_text)},
    [colour_role_glyphs] = {modes_text, A_SIZE(modes_text)},
    [colour_role_border] = {modes_plain, A_SIZE(modes_plain)},
    [colour_role_header_background] = {modes_fill, A_SIZE(modes_fill)},
    [colour_role_header_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_status_icons] = {modes_text, A_SIZE(modes_text)},
    [colour_role_footer_background] = {modes_fill, A_SIZE(modes_fill)},
    [colour_role_footer_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_dialogue_background] = {modes_plain, A_SIZE(modes_plain)},
    [colour_role_dialogue_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_help_background] = {modes_plain, A_SIZE(modes_plain)},
    [colour_role_help_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_keyboard_background] = {modes_plain, A_SIZE(modes_plain)},
    [colour_role_keyboard_text] = {modes_text, A_SIZE(modes_text)},
    [colour_role_progress_bar] = {modes_plain, A_SIZE(modes_plain)},
};

static struct {
    const char *key;
    char *setting;
    char saved[16];
    int first_row;
    int row_count;
    int mode;
    int original_mode;
    int has_custom;
    int has_gradient;
    uint32_t colours[2];
    uint32_t original_colours[2];
} roles[colour_role_count];

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

typedef struct {
    char name[SCHEME_NAME_LIMIT + 1];
    char path[MAX_BUFFER_SIZE];
    int user;
    int folder;
} colour_scheme;

static colour_scheme schemes[SCHEME_MAX];
static int scheme_count;
static int pending_scheme = -1;
static char scheme_folder[MAX_BUFFER_SIZE];

static lv_obj_t *ui_pnl_entry_colour;
static lv_obj_t *ui_txt_entry_colour;

static mux_dialogue save_dlg;
static mux_dialogue warn_dlg;
static mux_dialogue delete_dlg;
static mux_dialogue reset_dlg;
static mux_dialogue *save_prompt = &save_dlg;
static int hold_streak = 0;
static unsigned int unreadable = 0;

static int field_of(const char *udata) {
    for (int f = 0; f < field_count; f++)
        if (strcmp(udata, field_names[f]) == 0) return f;
    return -1;
}

static lv_obj_t *field_value(const int role, const int field) {
    return ui_objects_value[roles[role].first_row + field];
}

static int has_mode(const int role, const int mode) {
    for (int i = 0; i < role_modes[role].count; i++)
        if (role_modes[role].list[i] == mode) return 1;
    return 0;
}

static int mode_index(const int role) {
    for (int i = 0; i < role_modes[role].count; i++)
        if (role_modes[role].list[i] == roles[role].mode) return i;
    return 0;
}

static int uses_colour(const int role) {
    return roles[role].mode == mode_custom || roles[role].mode == mode_gradient;
}

static void init_rows(void) {
    memset(roles, 0, sizeof(roles));
    scheme_count = 0;
    pending_scheme = -1;
    scheme_folder[0] = '\0';
    save_prompt = &save_dlg;
    hold_streak = 0;

    int role = -1;
    for (int i = 0; i < ui_count_dynamic; i++) {
        const int field = field_of(row_udata[i]);
        if (field < 0) continue;
        if (field == field_mode) roles[++role].first_row = i;
        roles[role].row_count++;
    }
}

static uint32_t darker(const uint32_t colour) {
    double hue, saturation, light;
    colour_to_hsl(colour, &hue, &saturation, &light);
    return colour_from_hsl(hue, saturation, light * 0.4);
}

static void role_from_value(const int r, const char *value) {
    roles[r].colours[0] = theme_role_colour(r);
    roles[r].colours[1] = r == colour_role_background ? theme_background_gradient() : roles[r].colours[0];
    roles[r].mode = mode_theme;

    if (has_mode(r, mode_automatic) && strcmp(value, CUSTOM_COLOUR_AUTO) == 0) {
        roles[r].mode = mode_automatic;
    } else if (has_mode(r, mode_none) && strcmp(value, CUSTOM_COLOUR_NONE) == 0) {
        roles[r].mode = mode_none;
    } else if (custom_colour_value(value, &roles[r].colours[0])) {
        roles[r].mode = mode_custom;
    } else if (has_mode(r, mode_gradient) && custom_gradient_value(value, &roles[r].colours[0], &roles[r].colours[1])) {
        roles[r].mode = mode_gradient;
    }

    roles[r].has_custom = uses_colour(r);
    roles[r].has_gradient = roles[r].mode == mode_gradient;
}

static void init_roles(void) {
    char *settings[] = {
        config.settings.themeopt.colour_highlight,
        config.settings.themeopt.colour_highlight_text,
        config.settings.themeopt.colour_highlight_shadow,
        config.settings.themeopt.colour_background,
        config.settings.themeopt.colour_text,
        config.settings.themeopt.colour_text_shadow,
        config.settings.themeopt.colour_glyphs,
        config.settings.themeopt.colour_border,
        config.settings.themeopt.colour_header_background,
        config.settings.themeopt.colour_header_text,
        config.settings.themeopt.colour_status_icons,
        config.settings.themeopt.colour_footer_background,
        config.settings.themeopt.colour_footer_text,
        config.settings.themeopt.colour_dialogue_background,
        config.settings.themeopt.colour_dialogue_text,
        config.settings.themeopt.colour_help_background,
        config.settings.themeopt.colour_help_text,
        config.settings.themeopt.colour_keyboard_background,
        config.settings.themeopt.colour_keyboard_text,
        config.settings.themeopt.colour_progress_bar
    };
    const char *keys[] = {
        "highlight",
        "highlight_text",
        "highlight_shadow",
        "background",
        "text",
        "text_shadow",
        "glyphs",
        "border",
        "header_background",
        "header_text",
        "status_icons",
        "footer_background",
        "footer_text",
        "dialogue_background",
        "dialogue_text",
        "help_background",
        "help_text",
        "keyboard_background",
        "keyboard_text",
        "progress_bar"
    };

    for (int r = 0; r < colour_role_count; r++) {
        roles[r].key = keys[r];
        roles[r].setting = settings[r];
        snprintf(roles[r].saved, sizeof(roles[r].saved), "%s", settings[r]);
        role_from_value(r, settings[r]);
        roles[r].original_mode = roles[r].mode;
        roles[r].original_colours[0] = roles[r].colours[0];
        roles[r].original_colours[1] = roles[r].colours[1];
    }
}

static uint32_t effective_colour(int role);

static uint32_t effective_bottom(void) {
    switch (roles[colour_role_background].mode) {
        case mode_gradient:
            return roles[colour_role_background].colours[1];
        case mode_custom:
            return roles[colour_role_background].colours[0];
        default:
            return theme_background_gradient();
    }
}

static uint32_t effective_colour(const int role) {
    const int background_set = roles[colour_role_background].mode != mode_theme;

    switch (roles[role].mode) {
        case mode_custom:
        case mode_gradient:
            return roles[role].colours[0];
        case mode_none:
            return role == colour_role_footer_background ? effective_bottom()
                                                         : effective_colour(colour_role_background);
        case mode_automatic:
            switch (role) {
                case colour_role_highlight_text:
                    return readable_text_colour(effective_colour(colour_role_highlight));
                case colour_role_highlight_shadow:
                    return shadow_colour_for(
                        effective_colour(colour_role_highlight_text), effective_colour(colour_role_highlight)
                    );
                case colour_role_text_shadow:
                    return shadow_colour_for(
                        effective_colour(colour_role_text), effective_colour(colour_role_background)
                    );
                case colour_role_text:
                case colour_role_glyphs:
                    return readable_text_colour_between(effective_colour(colour_role_background), effective_bottom());
                case colour_role_header_text:
                case colour_role_status_icons:
                    return readable_text_colour(effective_colour(colour_role_header_background));
                case colour_role_footer_text:
                    return readable_text_colour(effective_colour(colour_role_footer_background));
                case colour_role_dialogue_text:
                    return readable_text_colour(effective_colour(colour_role_dialogue_background));
                case colour_role_help_text:
                    return readable_text_colour(effective_colour(colour_role_help_background));
                case colour_role_keyboard_text:
                    return readable_text_colour(effective_colour(colour_role_keyboard_background));
                default:
                    break;
            }
            break;
        default:
            break;
    }

    switch (role) {
        case colour_role_highlight:
        case colour_role_header_background:
            if (background_set && theme_role_colour(role) == theme_role_colour(colour_role_background))
                return effective_colour(colour_role_background);
            break;
        case colour_role_footer_background:
            if (background_set && theme_role_colour(role) == theme_background_gradient()) return effective_bottom();
            break;
        default:
            break;
    }

    return theme_role_colour(role);
}

static const struct {
    int text;
    int background;
    double minimum;
} readable_pairs[] = {
    {colour_role_highlight_text, colour_role_highlight, 4.5},
    {colour_role_text, colour_role_background, 4.5},
    {colour_role_glyphs, colour_role_background, 3.0},
    {colour_role_header_text, colour_role_header_background, 4.5},
    {colour_role_footer_text, colour_role_footer_background, 4.5},
    {colour_role_dialogue_text, colour_role_dialogue_background, 4.5},
    {colour_role_help_text, colour_role_help_background, 4.5},
    {colour_role_keyboard_text, colour_role_keyboard_background, 4.5},
    {colour_role_status_icons, colour_role_header_background, 3.0},
    {colour_role_highlight, colour_role_background, 1.2},
};

static double pair_contrast(const int text, const int background) {
    const uint32_t colour = effective_colour(text);
    const double contrast = colour_contrast(colour, effective_colour(background));
    return background == colour_role_background ? fmin(contrast, colour_contrast(colour, effective_bottom()))
                                                : contrast;
}

static char *section_label(const int role) {
    char *labels[] = {
        lang.muxcolour.section.highlight,
        lang.muxcolour.section.highlight_text,
        lang.muxcolour.section.highlight_shadow,
        lang.muxcolour.section.background,
        lang.muxcolour.section.text,
        lang.muxcolour.section.text_shadow,
        lang.muxcolour.section.glyphs,
        lang.muxcolour.section.border,
        lang.muxcolour.section.header_background,
        lang.muxcolour.section.header_text,
        lang.muxcolour.section.status_icons,
        lang.muxcolour.section.footer_background,
        lang.muxcolour.section.footer_text,
        lang.muxcolour.section.dialogue_background,
        lang.muxcolour.section.dialogue_text,
        lang.muxcolour.section.help_background,
        lang.muxcolour.section.help_text,
        lang.muxcolour.section.keyboard_background,
        lang.muxcolour.section.keyboard_text,
        lang.muxcolour.section.progress_bar
    };
    return role >= 0 && role < colour_role_count ? labels[role] : "";
}

static unsigned int unreadable_pairs(void) {
    unsigned int failing = 0;
    for (size_t i = 0; i < A_SIZE(readable_pairs); i++) {
        if (readable_pairs[i].text == colour_role_highlight && roles[colour_role_highlight].mode == mode_none) continue;
        if (pair_contrast(readable_pairs[i].text, readable_pairs[i].background) < readable_pairs[i].minimum)
            failing |= 1u << i;
    }
    return failing;
}

static void warn_unreadable(void) {
    const unsigned int failing = unreadable_pairs();
    const unsigned int fresh = failing & ~unreadable;
    unreadable = failing;
    if (!fresh) return;

    for (size_t i = 0; i < A_SIZE(readable_pairs); i++) {
        if (!(fresh & 1u << i)) continue;

        char message[MAX_BUFFER_SIZE];
        snprintf(
            message, sizeof(message), lang.muxcolour.hard_to_read, section_label(readable_pairs[i].text),
            section_label(readable_pairs[i].background)
        );
        toast_message(message, tst_wait_m);
        return;
    }
}

static int colour_light(const uint32_t colour) {
    double hue, saturation, light;
    colour_to_hsl(colour, &hue, &saturation, &light);
    return (int) lround(light * LIGHT_MAX);
}

static void style_value(
    lv_obj_t *value, const uint32_t fill, const uint32_t end, const int main_stop, const int grad_stop,
    const uint32_t text
) {
    const lv_style_selector_t states[] = {MU_OBJ_MAIN_DEFAULT, MU_OBJ_MAIN_FOCUS};

    lv_obj_add_flag(value, LV_OBJ_FLAG_USER_1);
    for (size_t s = 0; s < A_SIZE(states); s++) {
        lv_obj_set_style_bg_color(value, lv_color_hex(fill), states[s]);
        lv_obj_set_style_bg_grad_color(value, lv_color_hex(end), states[s]);
        lv_obj_set_style_bg_grad_dir(value, LV_GRAD_DIR_HOR, states[s]);
        lv_obj_set_style_bg_main_stop(value, (lv_coord_t) main_stop, states[s]);
        lv_obj_set_style_bg_grad_stop(value, (lv_coord_t) grad_stop, states[s]);
        lv_obj_set_style_bg_opa(value, LV_OPA_COVER, states[s]);
        lv_obj_set_style_radius(value, 4, states[s]);
        lv_obj_set_style_text_color(value, lv_color_hex(text), states[s]);
    }
}

static void style_clear(lv_obj_t *value) {
    lv_obj_clear_flag(value, LV_OBJ_FLAG_USER_1);
    lv_obj_set_style_bg_opa(value, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(value, LV_OPA_TRANSP, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_color(value, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(value, lv_color_hex(theme.list_focus.text), MU_OBJ_MAIN_FOCUS);
}

static void style_bar(lv_obj_t *value, const uint32_t fill, const int stop) {
    style_value(value, fill, BAR_TRACK, stop, stop, readable_text_colour(stop > 200 ? fill : BAR_TRACK));
}

static void refresh_values(void) {
    for (int r = 0; r < colour_role_count; r++) {
        const uint32_t top = effective_colour(r);
        const uint32_t bottom = roles[r].mode == mode_gradient ? roles[r].colours[1] : top;
        if (roles[r].mode == mode_none) {
            style_clear(field_value(r, field_mode));
        } else {
            style_value(field_value(r, field_mode), top, bottom, 0, 255, readable_text_colour_between(top, bottom));
        }

        for (int slot = 0; slot < 2; slot++) {
            const int base = slot ? field_gradient_red : field_red;
            if (base >= roles[r].row_count) continue;

            const uint32_t colour = roles[r].colours[slot];
            const int channels[] = {(int) (colour >> 16 & 0xFF), (int) (colour >> 8 & 0xFF), (int) (colour & 0xFF)};
            for (int c = 0; c < 3; c++) {
                lv_dropdown_set_selected(field_value(r, base + c), (uint16_t) channels[c]);
                style_bar(field_value(r, base + c), channel_fills[c], channels[c] * 255 / CHANNEL_MAX);
            }

            const int light = colour_light(colour);
            lv_dropdown_set_selected(field_value(r, base + 3), (uint16_t) light);
            style_bar(field_value(r, base + 3), colour, light * 255 / LIGHT_MAX);
        }
    }
}

static void apply_visibility(void) {
    for (int r = 0; r < colour_role_count; r++) {
        for (int f = field_red; f < roles[r].row_count; f++) {
            const int shown = f < field_gradient_red ? uses_colour(r) : roles[r].mode == mode_gradient;
            list_frame_set_suppressed(roles[r].first_row + f, !shown);
        }
    }
}

static int any_colour_modified(void) {
    for (int r = 0; r < colour_role_count; r++) {
        if (roles[r].mode != roles[r].original_mode) return 1;
        if (uses_colour(r) && roles[r].colours[0] != roles[r].original_colours[0]) return 1;
        if (roles[r].mode == mode_gradient && roles[r].colours[1] != roles[r].original_colours[1]) return 1;
    }
    return 0;
}

static void role_value(const int role, char *value, const size_t size) {
    const uint32_t top = roles[role].colours[0] & 0xFFFFFF;
    const uint32_t bottom = roles[role].colours[1] & 0xFFFFFF;

    switch (roles[role].mode) {
        case mode_automatic:
            snprintf(value, size, "%s", CUSTOM_COLOUR_AUTO);
            break;
        case mode_none:
            snprintf(value, size, "%s", CUSTOM_COLOUR_NONE);
            break;
        case mode_custom:
            snprintf(value, size, "%06X", top);
            break;
        case mode_gradient:
            snprintf(value, size, "%06X:%06X", top, bottom);
            break;
        default:
            snprintf(value, size, "%s", "");
            break;
    }
}

static void preview_colours(void) {
    for (int r = 0; r < colour_role_count; r++)
        role_value(r, roles[r].setting, MAX_BUFFER_SIZE);

    preview_custom_colours(&theme);
    apply_list_shadows();

    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(theme.system.background), MU_OBJ_MAIN_DEFAULT);
    refresh_screen_gradient(&theme, &device);

    lv_obj_set_style_bg_color(ui_pnl_header, lv_color_hex(theme.header.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, theme.header.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_title, lv_color_hex(theme.header.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_datetime, lv_color_hex(theme.datetime.text), MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnl_footer, lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha, MU_OBJ_MAIN_DEFAULT);

    const struct {
        lv_obj_t *glyph;
        lv_obj_t *text;
        const struct footer_glyph *colour;
    } nav[] = {
        {ui_lbl_nav_lr_glyph, ui_lbl_nav_lr, &theme.nav.lr}, {ui_lbl_nav_a_glyph, ui_lbl_nav_a, &theme.nav.a},
        {ui_lbl_nav_b_glyph, ui_lbl_nav_b, &theme.nav.b},    {ui_lbl_nav_x_glyph, ui_lbl_nav_x, &theme.nav.x},
        {ui_lbl_nav_y_glyph, ui_lbl_nav_y, &theme.nav.y},
    };
    for (size_t i = 0; i < A_SIZE(nav); i++) {
        lv_obj_set_style_img_recolor(nav[i].glyph, lv_color_hex(nav[i].colour->glyph), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(nav[i].text, lv_color_hex(nav[i].colour->text), MU_OBJ_MAIN_DEFAULT);
    }

    update_battery_capacity(ui_sta_capacity, &theme);
    update_battery_percent_label(ui_lbl_battery_percent, &theme);
    update_network_status(ui_sta_network, &theme, 0);
    update_bluetooth_status(ui_sta_bluetooth, &theme);
}

static void save_colour_options(void) {
    int save_failed = 0;

    for (int r = 0; r < colour_role_count; r++) {
        char value[16];
        role_value(r, value, sizeof(value));
        if (strcmp(value, roles[r].saved) == 0) continue;

        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), CONF_CONFIG_PATH "settings/theme/colour_%s", roles[r].key);
        if (!write_text_to_file_atomic(path, CHAR, value)) {
            save_failed++;
            continue;
        }
        snprintf(roles[r].saved, sizeof(roles[r].saved), "%s", value);
        refresh_config = 1;
    }

    REPORT_SAVE_FAILURE();
}

static int focused_scheme(void);

static int scheme_folder_user(void) {
    return strncmp(scheme_folder, SCHEME_USER "/", strlen(SCHEME_USER) + 1) == 0;
}

static int scheme_savable(void) {
    return list_frame_current() == 0 && (!scheme_folder[0] || scheme_folder_user());
}

static void show_nav(lv_obj_t *glyph, lv_obj_t *label, const int visible) {
    lv_obj_t *parts[] = {glyph, label};

    for (size_t i = 0; i < A_SIZE(parts); i++) {
        if (visible) {
            lv_obj_clear_flag(parts[i], MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(parts[i], MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

static void update_nav(void) {
    const int presets = list_frame_current() == 0;
    const int on_row = !list_frame_focused();
    const int scheme = presets ? focused_scheme() : -1;
    const int removable = scheme >= 0 && schemes[scheme].user && !schemes[scheme].folder;

    show_nav(ui_lbl_nav_a_glyph, ui_lbl_nav_a, presets && on_row);
    show_nav(ui_lbl_nav_lr_glyph, ui_lbl_nav_lr, !presets || !on_row);
    show_nav(ui_lbl_nav_x_glyph, ui_lbl_nav_x, !presets || removable);
    show_nav(ui_lbl_nav_y_glyph, ui_lbl_nav_y, scheme_savable());
    lv_label_set_text(ui_lbl_nav_a, scheme >= 0 && schemes[scheme].folder ? lang.generic.open : lang.generic.select);
    lv_label_set_text(ui_lbl_nav_x, presets ? lang.generic.remove : lang.generic.reset);
}

static void check_focus(void) {
    list_frame_reposition();
    update_nav();
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

static void set_mode(const int role, const int mode) {
    const uint32_t previous = effective_colour(role);
    const uint32_t previous_bottom = role == colour_role_background ? effective_bottom() : previous;
    const int focused_row = list_frame_current_row();

    roles[role].mode = mode;
    lv_dropdown_set_selected(field_value(role, field_mode), (uint16_t) mode_index(role));

    if (uses_colour(role) && !roles[role].has_custom) {
        roles[role].colours[0] = previous;
        roles[role].has_custom = 1;
    }

    if (mode == mode_gradient && !roles[role].has_gradient) {
        roles[role].colours[1] =
            previous_bottom == roles[role].colours[0] ? darker(roles[role].colours[0]) : previous_bottom;
        roles[role].has_gradient = 1;
    }

    apply_visibility();
    list_frame_apply();
    const int steps = list_frame_steps_to_row(focused_row);
    if (steps > 0) gen_step_movement(steps, +1, 2, 0, 0);
    check_focus();
}

static void roles_changed(void) {
    const int focused_row = list_frame_current_row();

    for (int r = 0; r < colour_role_count; r++)
        lv_dropdown_set_selected(field_value(r, field_mode), (uint16_t) mode_index(r));

    apply_visibility();
    list_frame_apply();
    const int steps = list_frame_steps_to_row(focused_row);
    if (steps > 0) gen_step_movement(steps, +1, 2, 0, 0);
    check_focus();

    preview_colours();
    refresh_values();
    warn_unreadable();
}

static int compare_schemes(const void *a, const void *b) {
    return strcasecmp(((const colour_scheme *) a)->name, ((const colour_scheme *) b)->name);
}

static void scan_schemes(const char *dir, const int user, const int folders) {
    DIR *folder = opendir(dir);
    if (!folder) return;

    const int first = scheme_count;
    const struct dirent *entry;
    while ((entry = readdir(folder)) && scheme_count < SCHEME_MAX) {
        if (entry->d_name[0] == '.') continue;

        colour_scheme *scheme = &schemes[scheme_count];
        snprintf(scheme->path, sizeof(scheme->path), "%s/%s", dir, entry->d_name);

        const int is_folder = dir_exist(scheme->path);
        const char *dot = strrchr(entry->d_name, '.');
        if (is_folder != folders || (!is_folder && (!dot || strcasecmp(dot, ".ini") != 0))) continue;

        const int length = is_folder ? (int) strlen(entry->d_name) : (int) (dot - entry->d_name);
        snprintf(scheme->name, sizeof(scheme->name), "%.*s", length, entry->d_name);
        scheme->user = user;
        scheme->folder = is_folder;
        scheme_count++;
    }

    closedir(folder);
    qsort(schemes + first, (size_t) (scheme_count - first), sizeof(schemes[0]), compare_schemes);
}

static int load_scheme_list(const char *select) {
    scheme_count = 0;
    if (scheme_folder[0]) {
        scan_schemes(scheme_folder, scheme_folder_user(), 0);
    } else {
        scan_schemes(SCHEME_PRESETS, 0, 1);
        scan_schemes(SCHEME_USER, 1, 1);
        scan_schemes(SCHEME_PRESETS, 0, 0);
        scan_schemes(SCHEME_USER, 1, 0);
    }

    list_frame_set_suppressed(preset_reset, scheme_folder[0] != '\0');
    list_frame_set_bar_hidden(scheme_folder[0] != '\0');

    int focus = -1;
    for (int i = 0; i < SCHEME_MAX; i++) {
        const int row = preset_first_scheme + i;
        if (i < scheme_count) {
            lv_label_set_text(ui_objects[row], schemes[i].name);
            apply_theme_list_glyph(&theme, ui_objects_glyph[row], mux_module, schemes[i].folder ? "folder" : "scheme");
            if (select && strcmp(schemes[i].name, select) == 0) focus = row;
        }
        list_frame_set_suppressed(row, i >= scheme_count);
    }

    return focus;
}

static void reload_schemes(const char *select, const int fallback) {
    const int focus = load_scheme_list(select);

    nav_suppress_next_shake();
    list_frame_apply();
    const int steps = list_frame_steps_to_row(focus >= 0 ? focus : fallback);
    if (steps > 0) gen_step_movement(steps, +1, 2, 0, 0);
    check_focus();
    nav_unsuppress_shake();
}

static void open_scheme_folder(const int index) {
    play_sound(snd_confirm);
    snprintf(scheme_folder, sizeof(scheme_folder), "%s", schemes[index].path);
    reload_schemes(NULL, preset_first_scheme);
}

static void leave_scheme_folder(void) {
    char name[SCHEME_NAME_LIMIT + 1];
    const char *slash = strrchr(scheme_folder, '/');
    snprintf(name, sizeof(name), "%s", slash ? slash + 1 : scheme_folder);

    play_sound(snd_back);
    scheme_folder[0] = '\0';
    reload_schemes(name, preset_first_scheme);
}

static int focused_scheme(void) {
    if (list_frame_focused()) return -1;

    const int index = list_frame_current_row() - preset_first_scheme;
    return index >= 0 && index < scheme_count ? index : -1;
}

static void load_colour_scheme(const int index) {
    FILE *file = fopen(schemes[index].path, "r");
    if (!file) {
        play_sound(snd_error);
        return;
    }

    char values[colour_role_count][16] = {{0}};
    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        const char *key = str_trim(line);
        const char *value = str_trim(equals + 1);
        for (int r = 0; r < colour_role_count; r++)
            if (strcmp(key, roles[r].key) == 0) snprintf(values[r], sizeof(values[r]), "%s", value);
    }
    fclose(file);

    for (int r = 0; r < colour_role_count; r++)
        role_from_value(r, values[r]);

    play_sound(snd_confirm);
    roles_changed();
    toast_message(lang.muxcolour.scheme_loaded, tst_wait_s);
}

static void reset_all(void) {
    for (int r = 0; r < colour_role_count; r++)
        roles[r].mode = mode_theme;

    play_sound(snd_confirm);
    roles_changed();
    toast_message(lang.muxcolour.reset_done, tst_wait_s);
}

static void save_scheme(const char *name) {
    char trimmed[SCHEME_NAME_LIMIT + 1];
    snprintf(trimmed, sizeof(trimmed), "%s", str_trim((char *) name));
    if (!trimmed[0]) snprintf(trimmed, sizeof(trimmed), "%s", lang.muxcolour.default_name);

    char safe[SCHEME_NAME_LIMIT + 1];
    str_safe_filename(trimmed, safe, sizeof(safe), "Scheme");

    const char *dir = scheme_folder_user() ? scheme_folder : SCHEME_USER;
    char saved_name[SCHEME_NAME_LIMIT + 8];
    char path[MAX_BUFFER_SIZE];
    snprintf(saved_name, sizeof(saved_name), "%s", safe);
    snprintf(path, sizeof(path), "%s/%s.ini", dir, saved_name);
    for (int i = 2; file_exist(path) && i < 100; i++) {
        snprintf(saved_name, sizeof(saved_name), "%s (%d)", safe, i);
        snprintf(path, sizeof(path), "%s/%s.ini", dir, saved_name);
    }

    create_directories(dir, 0);
    FILE *file = fopen(path, "w");
    int written = file != NULL;
    for (int r = 0; written && r < colour_role_count; r++) {
        if (roles[r].mode == mode_theme) continue;

        char value[16];
        role_value(r, value, sizeof(value));
        written = fprintf(file, "%s=%s\n", roles[r].key, value) > 0;
    }
    if (file && fclose(file) != 0) written = 0;

    if (!written) {
        play_sound(snd_error);
        toast_message(lang.muxcolour.scheme_failed, tst_wait_m);
        return;
    }

    reload_schemes(saved_name, list_frame_current_row());
    toast_message(lang.muxcolour.scheme_saved, tst_wait_s);
}

static void delete_scheme(const int index) {
    if (index < 0 || index >= scheme_count || !schemes[index].user || remove(schemes[index].path) != 0) {
        play_sound(snd_error);
        return;
    }

    char previous[SCHEME_NAME_LIMIT + 1];
    snprintf(previous, sizeof(previous), "%s", index > 0 ? schemes[index - 1].name : "");
    reload_schemes(previous, list_frame_current_row());
    toast_message(lang.muxcolour.scheme_removed, tst_wait_s);
}

static void open_name_entry(void) {
    lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

    key_show = 1;
    osk_show(ui_pnl_entry_colour);
    lv_textarea_set_text(ui_txt_entry_colour, lang.muxcolour.default_name);
}

static void close_name_entry(void) {
    key_show = 0;
    close_osk(key_entry, ui_group, ui_txt_entry_colour, ui_pnl_entry_colour);
}

static void handle_keyboard_ok_press(void) {
    char name[SCHEME_NAME_LIMIT + 1];
    snprintf(name, sizeof(name), "%s", lv_textarea_get_text(ui_txt_entry_colour));

    close_name_entry();
    save_scheme(name);
}

static void handle_keyboard_press(void) {
    play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void start_save(void) {
    for (int r = 0; r < colour_role_count; r++) {
        if (roles[r].mode == mode_theme) continue;

        play_sound(snd_confirm);
        open_name_entry();
        return;
    }

    play_sound(snd_error);
    toast_message(lang.muxcolour.scheme_empty, tst_wait_m);
}

static void preset_action(const int row) {
    switch (row) {
        case preset_reset:
            play_sound(snd_confirm);
            dialogue_open(&reset_dlg, &theme);
            break;
        default:
            if (focused_scheme() < 0) break;
            if (schemes[focused_scheme()].folder) {
                open_scheme_folder(focused_scheme());
            } else {
                load_colour_scheme(focused_scheme());
            }
            break;
    }
}

static mux_dialogue *open_dialogue(void) {
    if (dialogue_active(save_prompt)) return save_prompt;
    if (dialogue_active(&delete_dlg)) return &delete_dlg;
    if (dialogue_active(&reset_dlg)) return &reset_dlg;
    return NULL;
}

static void change_bar(const int role, const int field, const int step) {
    lv_obj_t *value = field_value(role, field);
    const int slot = field >= field_gradient_red;
    const int channel = field - (slot ? field_gradient_red : field_red);
    const int maximum = channel == 3 ? LIGHT_MAX : CHANNEL_MAX;
    const int current = (int) lv_dropdown_get_selected(value);
    const int next = current + step < 0 ? 0 : current + step > maximum ? maximum : current + step;
    if (next == current) return;

    play_sound(snd_option);

    uint32_t *colour = &roles[role].colours[slot];
    if (channel == 3) {
        double hue, saturation, light;
        colour_to_hsl(*colour, &hue, &saturation, &light);
        *colour = colour_from_hsl(hue, saturation, (double) next / LIGHT_MAX);
    } else {
        const int shift = 16 - channel * 8;
        *colour = (*colour & ~(0xFFu << shift)) | (uint32_t) next << shift;
    }
}

static int row_role(const int row) {
    for (int r = colour_role_count - 1; r >= 0; r--)
        if (row >= roles[r].first_row) return r;
    return 0;
}

static void change_value(const int direction, const int held) {
    if (key_show) {
        if (direction < 0) {
            key_left();
        } else {
            key_right();
        }
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (msgbox_active || dlg) {
        if (dlg) dialogue_handle_dpad(dlg, &theme, direction, swap_axis);
        return;
    }

    if (list_frame_focused()) {
        if (list_frame_move(direction)) {
            play_sound(snd_option);
            check_focus();
        }
        return;
    }

    const int row = list_frame_current_row();
    if (row < 0 || row >= ui_count_dynamic) return;

    if (row < roles[0].first_row) return;

    const int role = row_role(row);
    const int field = row - roles[role].first_row;

    if (field == field_mode) {
        if (held) return;
        const int count = role_modes[role].count;
        play_sound(snd_option);
        set_mode(role, role_modes[role].list[(mode_index(role) + direction + count) % count]);
    } else {
        hold_streak = held ? hold_streak + 1 : 0;
        const int step = hold_streak > 12 ? 16 : hold_streak > 4 ? 4 : 1;
        change_bar(role, field, direction * step);
    }

    preview_colours();
    refresh_values();
    warn_unreadable();
}

static void handle_option_prev(void) {
    change_value(-1, 0);
}

static void handle_option_next(void) {
    change_value(+1, 0);
}

static void handle_option_prev_hold(void) {
    change_value(-1, 1);
}

static void handle_option_next_hold(void) {
    change_value(+1, 1);
}

static void handle_frame_prev(void) {
    if (key_show) {
        key_swap_back();
        return;
    }

    if (msgbox_active || open_dialogue()) return;
    if (list_frame_move(-1)) {
        play_sound(snd_option);
        check_focus();
    }
}

static void handle_frame_next(void) {
    if (key_show) {
        key_swap();
        return;
    }

    if (msgbox_active || open_dialogue()) return;
    if (list_frame_move(+1)) {
        play_sound(snd_option);
        check_focus();
    }
}

static void handle_dpad_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg) {
        dialogue_handle_dpad(dlg, &theme, -1, !swap_axis);
        return;
    }
    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg) {
        dialogue_handle_dpad(dlg, &theme, +1, !swap_axis);
        return;
    }
    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (key_show || open_dialogue()) return;
    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (key_show || open_dialogue()) return;
    handle_list_nav_down_hold();
}

static void leave(void) {
    for (int r = 0; r < colour_role_count; r++)
        snprintf(roles[r].setting, MAX_BUFFER_SIZE, "%s", roles[r].saved);

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");
    load_mux("custom");
    mux_input_stop();
}

static void handle_a(void) {
    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (dialogue_active(&reset_dlg)) {
        const mux_confirm_opt choice = (mux_confirm_opt) reset_dlg.selected;
        dialogue_dismiss(&reset_dlg);
        if (choice == mux_confirm_yep) reset_all();
        return;
    }

    if (dialogue_active(&delete_dlg)) {
        const mux_confirm_opt choice = (mux_confirm_opt) delete_dlg.selected;
        dialogue_dismiss(&delete_dlg);
        if (choice == mux_confirm_yep) delete_scheme(pending_scheme);
        pending_scheme = -1;
        return;
    }

    if (dialogue_active(save_prompt)) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_prompt->selected;
        dialogue_dismiss(save_prompt);
        if (opt == mux_unsaved_save) save_colour_options();
        leave();
        return;
    }

    if (msgbox_active || hold_call) return;

    const int row = list_frame_focused() ? -1 : list_frame_current_row();
    if (row >= 0 && row < roles[0].first_row) {
        preset_action(row);
        return;
    }

    change_value(+1, 0);
}

static void handle_b(void) {
    if (hold_call) return;

    if (key_show) {
        key_backspace(ui_txt_entry_colour);
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg) {
        dialogue_mark_cancelled(dlg);
        dialogue_dismiss(dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (scheme_folder[0] && list_frame_current() == 0) {
        leave_scheme_folder();
        return;
    }

    save_prompt = unreadable_pairs() ? &warn_dlg : &save_dlg;
    if (any_colour_modified() && save_prompt == &warn_dlg) {
        dialogue_open(save_prompt, &theme);
        return;
    }

    if (dialogue_guard_unsaved(save_prompt, &theme, any_colour_modified())) return;

    play_sound(snd_back);
    if (any_colour_modified()) save_colour_options();
    leave();
}

static void handle_x(void) {
    if (key_show) {
        close_name_entry();
        return;
    }

    if (msgbox_active || open_dialogue() || hold_call || list_frame_current() < 0) return;

    if (list_frame_current() == 0) {
        const int index = focused_scheme();
        if (index < 0 || schemes[index].folder) return;

        if (!schemes[index].user) {
            play_sound(snd_error);
            toast_message(lang.muxcolour.scheme_builtin, tst_wait_s);
            return;
        }

        play_sound(snd_confirm);
        pending_scheme = index;
        dialogue_open(&delete_dlg, &theme);
        return;
    }

    const int role = list_frame_current() - 1;
    if (role >= colour_role_count || roles[role].mode == mode_theme) return;

    play_sound(snd_confirm);
    set_mode(role, mode_theme);
    preview_colours();
    refresh_values();
    warn_unreadable();
}

static void handle_y(void) {
    if (key_show) {
        key_space(ui_txt_entry_colour);
        return;
    }

    if (msgbox_active || open_dialogue() || hold_call || !scheme_savable()) return;
    start_save();
}

static void handle_select(void) {
    if (key_show) key_clear(ui_txt_entry_colour);
}

static void handle_start(void) {
    if (key_show) handle_keyboard_ok_press();
}

static void on_key_event(const struct input_event ev) {
    if (!key_show) return;

    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_ok_press();
    } else if (ev.code == KEY_ESC && ev.value == 1) {
        close_name_entry();
    } else {
        process_key_event(&ev, ui_txt_entry_colour);
    }
}

static void show_help(void) {
    if (list_frame_focused()) {
        list_frame_help();
        return;
    }

    const struct help_msg help_messages[] = {
        {"scheme", lang.muxcolour.help.scheme},
        {"schemereset", lang.muxcolour.help.scheme_reset},
        {"mode", lang.muxcolour.help.mode},
        {"red", lang.muxcolour.help.red},
        {"green", lang.muxcolour.help.green},
        {"blue", lang.muxcolour.help.blue},
        {"light", lang.muxcolour.help.light},
        {"gradient_red", lang.muxcolour.help.gradient},
        {"gradient_green", lang.muxcolour.help.gradient},
        {"gradient_blue", lang.muxcolour.help.gradient},
        {"gradient_light", lang.muxcolour.help.gradient},
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_name_entry(void) {
    ui_pnl_entry_colour = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_colour, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_colour, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_colour, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_colour, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_colour, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_colour, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_colour, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_colour, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_colour, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_colour, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_colour, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_pnl_entry_colour, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_colour, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_colour, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_colour = lv_textarea_create(ui_pnl_entry_colour);
    lv_obj_set_width(ui_txt_entry_colour, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_colour, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_colour, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_colour, SCHEME_NAME_LIMIT);
    lv_textarea_set_one_line(ui_txt_entry_colour, 1);
    lv_textarea_set_placeholder_text(ui_txt_entry_colour, lang.muxcolour.name_prompt);
    lv_obj_set_style_radius(ui_txt_entry_colour, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_colour, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_colour, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_colour, 2, MU_OBJ_MAIN_DEFAULT);
}

static void handle_help(void) {
    if (key_show || msgbox_active || progress_onscreen != -1 || hold_call || open_dialogue()) return;

    play_sound(snd_info_open);
    show_help();
}

static char *field_label(const char *udata) {
    if (strcmp(udata, "scheme") == 0) return lang.muxcolour.scheme;
    if (strcmp(udata, "schemereset") == 0) return lang.muxcolour.scheme_reset;

    char *labels[field_count] = {lang.muxcolour.colour,         lang.muxcolour.red,
                                 lang.muxcolour.green,          lang.muxcolour.blue,
                                 lang.muxcolour.light,          lang.muxcolour.gradient_red,
                                 lang.muxcolour.gradient_green, lang.muxcolour.gradient_blue,
                                 lang.muxcolour.gradient_light};
    return labels[field_of(udata)];
}

static const char *field_glyph(const char *udata) {
    return strncmp(udata, "gradient_", 9) == 0 ? udata + 9 : udata;
}

static char *mode_label(const int mode) {
    char *labels[] = {
        lang.muxcolour.theme, lang.muxcolour.custom, lang.muxcolour.automatic, lang.muxcolour.gradient,
        lang.muxcolour.none
    };
    return labels[mode];
}

static void init_navigation_group(void) {
#define COLOUR(NAME, UDATA) INIT_OPTION_ITEM(-1, colour, NAME, field_label(UDATA), field_glyph(UDATA), NULL, 0);
    COLOUR_ELEMENTS
#undef COLOUR

    char *channels = generate_number_string(0, CHANNEL_MAX, 1, NULL, NULL, NULL, 1);
    char *lights = generate_number_string(0, LIGHT_MAX, 1, NULL, "%", NULL, 1);

    for (int r = 0; r < colour_role_count; r++) {
        char modes[MAX_BUFFER_SIZE * 4] = "";
        for (int i = 0; i < role_modes[r].count; i++) {
            const size_t used = strlen(modes);
            snprintf(modes + used, sizeof(modes) - used, "%s%s", i ? "\n" : "", mode_label(role_modes[r].list[i]));
        }

        for (int f = 0; f < roles[r].row_count; f++) {
            const int row = roles[r].first_row + f;
            const int channel = (f - field_red) % 4;
            const char *options = f == field_mode ? modes : channel == 3 ? lights : channels;
            apply_theme_list_drop_down(&theme, ui_objects[row], ui_objects_value[row], (char *) options);
            lv_obj_set_width(
                ui_objects_value[row], lv_obj_get_style_width(ui_objects_value[row], LV_PART_MAIN) * 3 / 5
            );
        }

        lv_dropdown_set_selected(field_value(r, field_mode), (uint16_t) mode_index(r));
    }

    free(channels);
    free(lights);

    list_frame frames[colour_role_count + 1] = {{lang.muxcolour.section.presets, 0, roles[0].first_row}};
    for (int r = 0; r < colour_role_count; r++)
        frames[r + 1] = (list_frame) {section_label(r), roles[r].first_row, roles[r].row_count};

    reset_ui_groups();
    list_frame_init(
        &theme, ui_pnl_content, frames, colour_role_count + 1, ui_objects_panel, ui_objects, ui_objects_glyph,
        ui_objects_value, ui_count_dynamic
    );

    apply_visibility();
    load_scheme_list(NULL);
    list_nav_move(list_frame_restore(), +1);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.save, 0},
                                  {NULL, NULL, 0}});

#define COLOUR(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_colour, UDATA);
    COLOUR_ELEMENTS
#undef COLOUR

    overlay_display();
}

int muxcolour_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxcolour.title);
    init_muxcolour(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_rows();
    init_roles();
    init_navigation_group();
    refresh_values();
    unreadable = unreadable_pairs();
    check_focus();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );
    dialogue_init_unsaved(
        &warn_dlg, &theme, ui_screen, lang.generic.unsaved, lang.muxcolour.save_warning, lang.generic.save,
        lang.generic.discard, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &delete_dlg, &theme, ui_screen, lang.muxcolour.delete_title, lang.muxcolour.delete_desc, lang.generic.remove,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.muxcolour.reset_title, lang.muxcolour.reset_desc, lang.generic.reset,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    init_name_entry();
    init_osk(ui_pnl_entry_colour, ui_txt_entry_colour, 0, 0, SCHEME_NAME_LIMIT);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_select] = handle_select,
                [mux_input_start] = handle_start,
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
            [mux_input_dpad_left] = handle_option_prev_hold,
            [mux_input_dpad_right] = handle_option_next_hold,
            [mux_input_l1] = handle_frame_prev,
            [mux_input_r1] = handle_frame_next,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
