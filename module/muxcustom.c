#include "muxshare.h"
#include <common/ui/list_frame.h>
#include <common/ui/orientation.h>
#include <common/ui/font_info.h>
#include <harfbuzz/hb.h>
#include <math.h>
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
static char font_directory_saved[MAX_BUFFER_SIZE];
static int16_t font_face_saved;
static int16_t font_width_saved;
static int16_t font_italic_saved;
static int16_t font_list_size_saved;
static int16_t font_header_size_saved;
static int16_t font_footer_size_saved;
static int16_t font_panel_size_saved;
static int has_theme_type;
static int dropdown_to_canonical[4];
static int has_custom_type;
static int num_type_options;
static int font_directory_visible;
static int font_directory_has_root;

typedef struct {
    char *name;
    char *label;
    char *path;
    unsigned int face_index;
} font_option;

static font_option *font_options;
static size_t font_option_count;

#define FONT_SIZE_OPTION_MAX 128
static int active_font_size_values[FONT_SIZE_OPTION_MAX];
static int active_font_size_count;
static int font_size_options_visible = 1;

enum font_axis_id { FONT_AXIS_WIDTH, FONT_AXIS_ITALIC, FONT_AXIS_COUNT };

typedef struct {
    uint32_t tag;
    int values[128];
    int value_count;
    int visible;
} font_axis_option;

static font_axis_option font_axes[FONT_AXIS_COUNT] = {
    [FONT_AXIS_WIDTH] = {.tag = HB_TAG('w', 'd', 't', 'h')}, [FONT_AXIS_ITALIC] = {.tag = HB_TAG('i', 't', 'a', 'l')}
};

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
static int init_custom_menu_schema(lv_obj_t **panels, lv_obj_t **labels, lv_obj_t **glyphs, lv_obj_t **values);
static void apply_custom_menu_nav(void);

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

#define CONTENT_ITEM_COUNT_MIN 3

static void restore_count_dropdown(lv_obj_t *dropdown, const int16_t stored) {
    uint32_t idx = 0;

    // A stored count below the minimum predates the minimum being raised.
    if (stored > 0) idx = (uint32_t) (stored > CONTENT_ITEM_COUNT_MIN ? stored - CONTENT_ITEM_COUNT_MIN + 1 : 1);

    lv_dropdown_set_selected(dropdown, idx);
}

static void
save_count_dropdown(const lv_obj_t *dropdown, const int original, const char *key, int16_t *cfg, int *is_modified) {
    const uint32_t sel = lv_dropdown_get_selected(dropdown);
    if ((int) sel == original) return;

    const int16_t val = (int16_t) (sel >= 1 ? (int) sel + CONTENT_ITEM_COUNT_MIN - 1 : 0);
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

static int font_file_extension(const char *name) {
    const size_t length = name ? strlen(name) : 0;
    if (length < 4) return 0;

    const char *extension = name + length - 4;
    return strcasecmp(extension, ".ttf") == 0 || strcasecmp(extension, ".otf") == 0
           || strcasecmp(extension, ".ttc") == 0 || strcasecmp(extension, ".pcf") == 0
           || strcasecmp(extension, ".bdf") == 0;
}

static int bitmap_font_extension(const char *name) {
    const size_t length = name ? strlen(name) : 0;
    if (length < 4) return 0;

    const char *extension = name + length - 4;
    return strcasecmp(extension, ".pcf") == 0 || strcasecmp(extension, ".bdf") == 0;
}

static void stored_font_name(char *out, const size_t out_size, const char *filename) {
    const size_t length = strlen(filename);
    if (length > 4 && strcasecmp(filename + length - 4, ".ttf") == 0)
        snprintf(out, out_size, "%.*s", (int) (length - 4), filename);
    else
        snprintf(out, out_size, "%s", filename);
}

static void displayed_font_name(char *out, const size_t out_size, const char *filename) {
    const size_t length = strlen(filename);
    if (length > 4 && font_file_extension(filename))
        snprintf(out, out_size, "%.*s", (int) (length - 4), filename);
    else
        snprintf(out, out_size, "%s", filename);
}

static int font_option_exists(const char *name, const unsigned int face_index) {
    for (size_t i = 0; i < font_option_count; i++)
        if (font_options[i].face_index == face_index && strcasecmp(font_options[i].name, name) == 0) return 1;

    return 0;
}

static int add_font_option(
    const char *name, const char *label, const char *path, const unsigned int face_index, const int dedupe
) {
    if (dedupe && font_option_exists(name, face_index)) return 0;

    font_option *options = realloc(font_options, (font_option_count + 1) * sizeof(*options));
    if (!options) return 0;

    font_options = options;
    font_option *option = &font_options[font_option_count];
    option->name = strdup(name);
    option->label = strdup(label);
    option->path = strdup(path);
    option->face_index = face_index;
    if (option->name && option->label && option->path) {
        font_option_count++;
        return 1;
    }

    free(option->name);
    free(option->label);
    free(option->path);
    return 0;
}

static int add_font_options_from(const char *dir, const int dedupe) {
    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return 0;

    int added = 0;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;

        if (name[0] == '.') {
            free(entries[i]);
            continue;
        }

        if (font_file_extension(name)) {
            char stored[MAX_BUFFER_SIZE];
            stored_font_name(stored, sizeof(stored), name);
            char displayed[MAX_BUFFER_SIZE];
            displayed_font_name(displayed, sizeof(displayed), name);

            char path[MAX_BUFFER_SIZE];
            snprintf(path, sizeof(path), "%s/%s", dir, name);

            font_info_t info;
            const int valid = font_info_read(path, 0, &info);
            const unsigned int face_count = valid && info.face_count ? info.face_count : 1;
            for (unsigned int face = 0; face < face_count && face <= INT16_MAX; face++) {
                if (face > 0 && !font_info_read(path, face, &info)) continue;
                const char *label = valid || face > 0 ? info.display : displayed;
                added += add_font_option(stored, label, path, face, dedupe);
            }

            free(entries[i]);
            continue;
        }

        free(entries[i]);
    }

    free(entries);

    return added;
}

static int directory_has_font(const char *dir) {
    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return 0;

    int found = 0;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        if (!found && name[0] != '.' && font_file_extension(name)) found = 1;
        free(entries[i]);
    }
    free(entries);

    return found;
}

typedef struct {
    char **names;
    size_t count;
} font_directory_list;

static int font_directory_compare(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcasecmp(*a, *b);
}

static void font_directory_list_add(font_directory_list *list, const char *name) {
    for (size_t i = 0; i < list->count; i++)
        if (strcasecmp(list->names[i], name) == 0) return;

    char **names = realloc(list->names, (list->count + 1) * sizeof(*names));
    if (!names) return;

    list->names = names;
    list->names[list->count] = strdup(name);
    if (list->names[list->count]) list->count++;
}

static void font_directory_list_scan(font_directory_list *list, const char *base) {
    struct dirent **entries;
    const int n = scandir(base, &entries, NULL, alphasort);
    if (n < 0) return;

    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        if (name[0] != '.') {
            char path[MAX_BUFFER_SIZE];
            snprintf(path, sizeof(path), "%s/%s", base, name);

            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode) && directory_has_font(path))
                font_directory_list_add(list, name);
        }
        free(entries[i]);
    }
    free(entries);
}

static void font_directory_list_free(font_directory_list *list) {
    for (size_t i = 0; i < list->count; i++)
        free(list->names[i]);
    free(list->names);
    list->names = NULL;
    list->count = 0;
}

static void selected_font_directory(char *out, const size_t out_size) {
    out[0] = '\0';
    if (!font_directory_visible) return;

    if (font_directory_has_root && lv_dropdown_get_selected(ui_dro_font_directory_font) == 0) return;
    lv_dropdown_get_selected_str(ui_dro_font_directory_font, out, out_size);
}

static void populate_font_directories(const char *preferred) {
    lv_dropdown_clear_options(ui_dro_font_directory_font);
    font_directory_visible = 0;
    font_directory_has_root = 0;

    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    if (canonical_type != 0 && canonical_type != 3) return;

    font_directory_list directories = {0};
    if (canonical_type == 0) {
        font_directory_list_scan(&directories, INTERNAL_FONTS);
    } else {
        const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};
        for (size_t m = 0; m < A_SIZE(mounts); m++) {
            if (!mounts[m] || !*mounts[m]) continue;

            char base[MAX_BUFFER_SIZE];
            snprintf(base, sizeof(base), "%s/%s", mounts[m], USER_FONTS);
            remove_double_slashes(base);

            if (directory_has_font(base)) font_directory_has_root = 1;
            font_directory_list_scan(&directories, base);
        }
    }

    font_directory_visible = directories.count > 0;
    if (!font_directory_visible) {
        font_directory_list_free(&directories);
        return;
    }

    if (font_directory_has_root)
        lv_dropdown_add_option(ui_dro_font_directory_font, lang.muxfont.directory_root, LV_DROPDOWN_POS_LAST);

    qsort(directories.names, directories.count, sizeof(*directories.names), font_directory_compare);
    for (size_t i = 0; i < directories.count; i++)
        lv_dropdown_add_option(ui_dro_font_directory_font, directories.names[i], LV_DROPDOWN_POS_LAST);

    const char *wanted = preferred;
    if ((!wanted || !*wanted) && canonical_type == 0) wanted = config.settings.general.language;

    int32_t selected = wanted && *wanted ? lv_dropdown_get_option_index(ui_dro_font_directory_font, wanted) : -1;
    if (selected < 0 && font_directory_has_root && (!preferred || !*preferred)) selected = 0;
    lv_dropdown_set_selected(ui_dro_font_directory_font, selected >= 0 ? (uint32_t) selected : 0);

    font_directory_list_free(&directories);
}

static int populate_user_font_names(const char *directory) {
    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    int added = 0;
    for (size_t m = 0; m < A_SIZE(mounts); m++) {
        if (!mounts[m] || !*mounts[m]) continue;

        char dir[MAX_BUFFER_SIZE];
        if (directory && *directory)
            snprintf(dir, sizeof(dir), "%s/%s/%s", mounts[m], USER_FONTS, directory);
        else
            snprintf(dir, sizeof(dir), "%s/%s", mounts[m], USER_FONTS);
        remove_double_slashes(dir);

        // The same font on two storages should only be offered once
        added += add_font_options_from(dir, 1);
    }

    return added;
}

static void clear_font_options(void) {
    for (size_t i = 0; i < font_option_count; i++) {
        free(font_options[i].name);
        free(font_options[i].label);
        free(font_options[i].path);
    }
    free(font_options);
    font_options = NULL;
    font_option_count = 0;
}

static int font_option_compare(const void *left, const void *right) {
    const font_option *a = left;
    const font_option *b = right;
    const int label = strcasecmp(a->label, b->label);
    if (label) return label;
    const int name = strcasecmp(a->name, b->name);
    if (name) return name;
    return (a->face_index > b->face_index) - (a->face_index < b->face_index);
}

static void render_font_options(void) {
    qsort(font_options, font_option_count, sizeof(*font_options), font_option_compare);

    for (size_t i = 0; i < font_option_count; i++) {
        char label[MAX_BUFFER_SIZE];
        const int duplicate =
            (i > 0 && strcasecmp(font_options[i - 1].label, font_options[i].label) == 0)
            || (i + 1 < font_option_count && strcasecmp(font_options[i + 1].label, font_options[i].label) == 0);
        if (duplicate) {
            char displayed[MAX_BUFFER_SIZE];
            displayed_font_name(displayed, sizeof(displayed), font_options[i].name);
            snprintf(
                label, sizeof(label), "%.*s - %.*s", (int) ((sizeof(label) - 4) / 2), font_options[i].label,
                (int) ((sizeof(label) - 4) / 2), displayed
            );
        } else
            snprintf(label, sizeof(label), "%s", font_options[i].label);

        lv_dropdown_add_option(ui_dro_font_name_font, label, LV_DROPDOWN_POS_LAST);
    }
}

static int font_option_index(const char *name, const unsigned int face_index) {
    const char *leaf = name ? strrchr(name, '/') : NULL;
    name = leaf ? leaf + 1 : name;

    for (size_t i = 0; i < font_option_count; i++)
        if (name && font_options[i].face_index == face_index && strcasecmp(font_options[i].name, name) == 0)
            return (int) i;

    return -1;
}

static void select_font_name(const char *wanted, const unsigned int face_index) {
    int idx = font_option_index(wanted, face_index);
    if (idx < 0) idx = font_option_index(DEFAULT_FONT_NAME, 0);
    lv_dropdown_set_selected(ui_dro_font_name_font, idx >= 0 ? (uint32_t) idx : 0);
}

static lv_obj_t *font_axis_dropdown(const enum font_axis_id axis) {
    switch (axis) {
        case FONT_AXIS_WIDTH:
            return ui_dro_width_font;
        case FONT_AXIS_ITALIC:
            return ui_dro_italic_font;
        default:
            return NULL;
    }
}

static lv_obj_t *font_axis_label(const enum font_axis_id axis) {
    switch (axis) {
        case FONT_AXIS_WIDTH:
            return ui_lbl_width_font;
        case FONT_AXIS_ITALIC:
            return ui_lbl_italic_font;
        default:
            return NULL;
    }
}

static int16_t font_axis_config(const enum font_axis_id axis) {
    switch (axis) {
        case FONT_AXIS_WIDTH:
            return config.settings.font.width;
        case FONT_AXIS_ITALIC:
            return config.settings.font.italic;
        default:
            return 0;
    }
}

static void font_axis_set_config(const enum font_axis_id axis, const int16_t value) {
    switch (axis) {
        case FONT_AXIS_WIDTH:
            config.settings.font.width = value;
            break;
        case FONT_AXIS_ITALIC:
            config.settings.font.italic = value;
            break;
        default:
            break;
    }
}

static void font_axis_add_value(font_axis_option *axis, const int value) {
    if (axis->value_count >= A_SIZE(axis->values)) return;
    for (int i = 0; i < axis->value_count; i++)
        if (axis->values[i] == value) return;
    axis->values[axis->value_count++] = value;
}

static int font_axis_step(const enum font_axis_id axis, const int minimum, const int maximum) {
    const int range = maximum - minimum;
    if (axis == FONT_AXIS_WIDTH) return range > 40 ? 5 : 1;
    return 1;
}

static void populate_font_axis(const enum font_axis_id axis_id, const font_option *font) {
    font_axis_option *axis = &font_axes[axis_id];
    lv_obj_t *dropdown = font_axis_dropdown(axis_id);
    lv_dropdown_clear_options(dropdown);
    lv_dropdown_add_option(dropdown, lang.muxfont.size_default, LV_DROPDOWN_POS_LAST);
    axis->values[0] = 0;
    axis->value_count = 1;
    axis->visible = 0;

    if (!font) {
        lv_dropdown_set_selected(dropdown, 0);
        return;
    }

    font_axis_info_t info;
    if (!font_info_axis(font->path, font->face_index, axis->tag, &info)) {
        lv_dropdown_set_selected(dropdown, 0);
        return;
    }

    axis->visible = 1;
    if (axis_id == FONT_AXIS_ITALIC) {
        font_axis_add_value(axis, 1);
    } else {
        int minimum = (int) ceilf(info.minimum);
        int maximum = (int) floorf(info.maximum);
        if (minimum < INT16_MIN + 1) minimum = INT16_MIN + 1;
        if (maximum > INT16_MAX) maximum = INT16_MAX;
        const int step = font_axis_step(axis_id, minimum, maximum);
        font_axis_add_value(axis, minimum);
        int value = minimum;
        if (step > 1) value = (int) ceil((double) minimum / step) * step;
        for (; value <= maximum && axis->value_count < A_SIZE(axis->values) - 1; value += step)
            font_axis_add_value(axis, value);
        font_axis_add_value(axis, maximum);
    }

    for (int i = 1; i < axis->value_count; i++) {
        char value[32];
        if (axis_id == FONT_AXIS_ITALIC)
            snprintf(value, sizeof(value), "%s", lang.generic.enabled);
        else
            snprintf(value, sizeof(value), "%d", axis->values[i]);
        lv_dropdown_add_option(dropdown, value, LV_DROPDOWN_POS_LAST);
    }

    int selected = 0;
    const int configured = font_axis_config(axis_id);
    for (int i = 1; i < axis->value_count; i++)
        if (axis->values[i] == configured) selected = i;
    if (configured && !selected) font_axis_set_config(axis_id, 0);
    lv_dropdown_set_selected(dropdown, (uint32_t) selected);
}

static void populate_font_axes(void) {
    const uint32_t selected = lv_dropdown_get_selected(ui_dro_font_name_font);
    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    const font_option *font = canonical_type != 1 && selected < font_option_count ? &font_options[selected] : NULL;
    for (int axis = 0; axis < FONT_AXIS_COUNT; axis++)
        populate_font_axis((enum font_axis_id) axis, font);
}

static void apply_font_axis_visibility(void) {
    for (int axis = 0; axis < FONT_AXIS_COUNT; axis++)
        list_frame_set_suppressed(
            list_frame_row_of(font_axis_label((enum font_axis_id) axis)), !font_axes[axis].visible
        );
}

static void apply_font_axis_settings(void) {
    for (int axis = 0; axis < FONT_AXIS_COUNT; axis++) {
        const uint32_t selected = lv_dropdown_get_selected(font_axis_dropdown((enum font_axis_id) axis));
        const int value = selected < (uint32_t) font_axes[axis].value_count ? font_axes[axis].values[selected] : 0;
        font_axis_set_config((enum font_axis_id) axis, (int16_t) value);
    }
}

static void populate_font_names(const char *preferred, const unsigned int preferred_face) {
    lv_dropdown_clear_options(ui_dro_font_name_font);
    clear_font_options();

    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    char directory[MAX_BUFFER_SIZE];
    selected_font_directory(directory, sizeof(directory));

    if (canonical_type == 3) {
        populate_user_font_names(directory);
    } else {
        char dir[MAX_BUFFER_SIZE];
        if (canonical_type == 0 && directory[0])
            snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", directory);
        else
            snprintf(dir, sizeof(dir), "%s", INTERNAL_FONTS);

        add_font_options_from(dir, 0);
    }

    render_font_options();
    if (!font_option_count) lv_dropdown_add_option(ui_dro_font_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);
    select_font_name(preferred, preferred_face);
    populate_font_axes();
}

static int selected_font_size_value(lv_obj_t *dropdown, const int fallback) {
    if (active_font_size_count <= 0) return fallback;
    const uint32_t selected = lv_dropdown_get_selected(dropdown);
    return selected < (uint32_t) active_font_size_count ? active_font_size_values[selected] : 0;
}

static void select_font_size_value(lv_obj_t *dropdown, const int value) {
    int selected = 0;
    for (int i = 0; i < active_font_size_count; i++)
        if (active_font_size_values[i] == value) selected = i;
    lv_dropdown_set_selected(dropdown, (uint32_t) selected);
}

static void populate_font_size_options(void) {
    const int selected_values[] = {
        selected_font_size_value(ui_dro_list_size_font, config.settings.font.list_size),
        selected_font_size_value(ui_dro_header_size_font, config.settings.font.header_size),
        selected_font_size_value(ui_dro_footer_size_font, config.settings.font.footer_size),
        selected_font_size_value(ui_dro_panel_size_font, config.settings.font.panel_size)
    };

    active_font_size_values[0] = 0;
    active_font_size_count = 1;

    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    const uint32_t selected_font = lv_dropdown_get_selected(ui_dro_font_name_font);
    const int fixed_theme = canonical_type == 1 && !theme_font_is_scalable();
    if (!fixed_theme && selected_font < font_option_count && bitmap_font_extension(font_options[selected_font].path)) {
        int sizes[FONT_SIZE_OPTION_MAX - 1];
        const int count = font_info_fixed_sizes(
            font_options[selected_font].path, font_options[selected_font].face_index, sizes, A_SIZE(sizes)
        );
        for (int i = 0; i < count && active_font_size_count < FONT_SIZE_OPTION_MAX; i++) {
            if (sizes[i] > 0 && sizes[i] <= INT16_MAX) active_font_size_values[active_font_size_count++] = sizes[i];
        }
    } else if (!fixed_theme) {
        for (int i = 1; i < font_size_count && active_font_size_count < FONT_SIZE_OPTION_MAX; i++)
            active_font_size_values[active_font_size_count++] = font_size_values[i];
    }

    font_size_options_visible = active_font_size_count > 2;

    lv_obj_t *const dropdowns[] = {
        ui_dro_list_size_font, ui_dro_header_size_font, ui_dro_footer_size_font, ui_dro_panel_size_font
    };
    for (size_t d = 0; d < A_SIZE(dropdowns); d++) {
        lv_dropdown_clear_options(dropdowns[d]);
        lv_dropdown_add_option(dropdowns[d], lang.muxfont.size_default, LV_DROPDOWN_POS_LAST);
        for (int i = 1; i < active_font_size_count; i++) {
            char value[16];
            snprintf(value, sizeof(value), "%d", active_font_size_values[i]);
            lv_dropdown_add_option(dropdowns[d], value, LV_DROPDOWN_POS_LAST);
        }
        select_font_size_value(dropdowns[d], selected_values[d]);
    }
}

static void apply_font_size_visibility(void) {
    lv_obj_t *const labels[] = {
        ui_lbl_list_size_font, ui_lbl_header_size_font, ui_lbl_footer_size_font, ui_lbl_panel_size_font
    };
    for (size_t i = 0; i < A_SIZE(labels); i++)
        list_frame_set_suppressed(list_frame_row_of(labels[i]), !font_size_options_visible);
}

static void apply_font_name_visibility(void) {
    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    const int compiled_theme = canonical_type == 1 && !theme_font_is_scalable() && theme_font_is_compiled();
    list_frame_set_suppressed(list_frame_row_of(ui_lbl_font_name_font), compiled_theme);
}

static void apply_current_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));

    if (config.settings.advanced.font == 0 || config.settings.advanced.font == 3)
        selected_font_directory(config.settings.font.directory, sizeof(config.settings.font.directory));
    else
        config.settings.font.directory[0] = '\0';

    uint32_t idx = lv_dropdown_get_selected(ui_dro_list_size_font);
    config.settings.font.list_size =
        (int16_t) (idx < (uint32_t) active_font_size_count ? active_font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_header_size_font);
    config.settings.font.header_size =
        (int16_t) (idx < (uint32_t) active_font_size_count ? active_font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_footer_size_font);
    config.settings.font.footer_size =
        (int16_t) (idx < (uint32_t) active_font_size_count ? active_font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_panel_size_font);
    config.settings.font.panel_size =
        (int16_t) (idx < (uint32_t) active_font_size_count ? active_font_size_values[idx] : 0);

    const uint32_t selected_font = lv_dropdown_get_selected(ui_dro_font_name_font);
    if (selected_font < font_option_count) {
        snprintf(config.settings.font.name, sizeof(config.settings.font.name), "%s", font_options[selected_font].name);
        config.settings.font.face = (int16_t) font_options[selected_font].face_index;
    } else {
        config.settings.font.name[0] = '\0';
        config.settings.font.face = 0;
    }
    apply_font_axis_settings();

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts_preview();
    list_frame_refresh_text_geometry();
    lv_obj_invalidate(ui_screen);
}

static void revert_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_to_canonical((uint32_t) type_original);
    config.settings.font.list_size = font_list_size_saved;
    config.settings.font.header_size = font_header_size_saved;
    config.settings.font.footer_size = font_footer_size_saved;
    config.settings.font.panel_size = font_panel_size_saved;
    snprintf(config.settings.font.directory, sizeof(config.settings.font.directory), "%s", font_directory_saved);
    snprintf(config.settings.font.name, sizeof(config.settings.font.name), "%s", font_name_saved);
    config.settings.font.face = font_face_saved;
    config.settings.font.width = font_width_saved;
    config.settings.font.italic = font_italic_saved;

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts_preview();
    list_frame_refresh_text_geometry();
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

    LOCK_ROW(font_directory);
    LOCK_ROW(font_name);
    LOCK_ROW(list_size);
    LOCK_ROW(header_size);
    LOCK_ROW(footer_size);
    LOCK_ROW(panel_size);
#undef LOCK_ROW

    // A dimmed row stays on screen but drops out of navigation
    lv_obj_t *const rows[] = {ui_lbl_font_directory_font, ui_lbl_font_name_font,   ui_lbl_list_size_font,
                              ui_lbl_header_size_font,    ui_lbl_footer_size_font, ui_lbl_panel_size_font};
    for (size_t i = 0; i < A_SIZE(rows); i++)
        list_frame_set_inert(list_frame_row_of(rows[i]), font_locked);
}

// A dimmed row is inert rather than merely faded
static int font_row_locked(const lv_obj_t *focused) {
    if (!font_locked) return 0;

    return focused == ui_dro_font_directory_font || focused == ui_dro_font_name_font || focused == ui_dro_list_size_font
           || focused == ui_dro_header_size_font || focused == ui_dro_footer_size_font
           || focused == ui_dro_panel_size_font;
}

static int font_row(const lv_obj_t *focused) {
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

    snprintf(font_directory_saved, sizeof(font_directory_saved), "%s", config.settings.font.directory);
    snprintf(font_name_saved, sizeof(font_name_saved), "%s", config.settings.font.name);
    font_face_saved = config.settings.font.face;
    font_width_saved = config.settings.font.width;
    font_italic_saved = config.settings.font.italic;
    font_list_size_saved = config.settings.font.list_size;
    font_header_size_saved = config.settings.font.header_size;
    font_footer_size_saved = config.settings.font.footer_size;
    font_panel_size_saved = config.settings.font.panel_size;

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

    has_theme_type = theme_has_font();
    has_custom_type = user_font_count() > 0;

    char *all_type_options[] = {
        lang.muxfont.type_options.language, lang.muxfont.type_options.theme, lang.muxfont.type_options.internal,
        lang.muxfont.type_options.custom
    };

    num_type_options = 0;
    dropdown_to_canonical[num_type_options++] = 0;
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
    INIT_OPTION_ITEM(-1, font, font_directory, lang.muxfont.font_directory, "font", NULL, 0);
    INIT_OPTION_ITEM(-1, font, font_name, lang.muxfont.font_name, "fontname", NULL, 0);
    INIT_OPTION_ITEM(-1, font, width, lang.muxfont.width, "width", NULL, 0);
    INIT_OPTION_ITEM(-1, font, italic, lang.muxfont.italic, "fontname", NULL, 0);
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
    INIT_OPTION_ITEM(-1, visual, shuffle, lang.muxcontent.shuffle, "shuffle", disabled_enabled, 2);
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
    INIT_OPTION_ITEM(-1, custom, black_fade, lang.muxcustom.blackfade, "blackfade", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, custom, catalogue, lang.muxcustom.catalogue, "catalogue", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, config, lang.muxcustom.config, "config", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, logo, lang.muxcustom.logo, "logo", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme, lang.muxcustom.theme, "theme", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_resolution, lang.muxcustom.themeresolution, "resolution", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, theme_scaling, lang.muxcustom.themescaling, "scaling", theme_scaling_options, 3);
    INIT_OPTION_ITEM(-1, custom, theme_alternate, lang.muxcustom.themealternate, "alternate", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, random_theme, lang.muxcustom.randomtheme, "randomtheme", disabled_enabled, 2);
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
    INIT_OPTION_ITEM(-1, custom, music, lang.muxcustom.music.title, "music", music_options, 3);
    INIT_OPTION_ITEM(-1, custom, music_volume, lang.muxcustom.music.volume, "musicvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, sound, lang.muxcustom.sound.title, "sound", sound_options, 3);
    INIT_OPTION_ITEM(-1, custom, sound_volume, lang.muxcustom.sound.volume, "soundvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, custom, chime, lang.muxcustom.chime, "chime", disabled_enabled, 2);

    populate_font_directories(config.settings.font.directory);
    populate_font_names(
        config.settings.font.name, config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0
    );
    apply_theme_list_drop_down(&theme, ui_lbl_list_size_font, ui_dro_list_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_header_size_font, ui_dro_header_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_footer_size_font, ui_dro_footer_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_panel_size_font, ui_dro_panel_size_font, size_options);
    populate_font_size_options();

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
    char *count_options =
        generate_number_string(CONTENT_ITEM_COUNT_MIN, 64, 1, lang.muxthemeopt.size_default, NULL, NULL, 0);
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

    init_custom_menu_schema(ui_objects_panel, ui_objects, ui_objects_glyph, ui_objects_value);

    list_frame_set_suppressed(list_frame_row_of(ui_lbl_font_directory_font), !font_directory_visible);
    apply_font_name_visibility();
    apply_font_axis_visibility();
    apply_font_size_visibility();

    list_nav_move(list_frame_restore(), +1);
}

static void check_focus(void) {
    list_frame_reposition();
    apply_custom_menu_nav();
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

static int option_kiosk_locked(void);

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
    if (font_row_locked(focused) || option_kiosk_locked()) return;
    const int focused_row = list_frame_current_row();

    move_option(focused, -1);

    if (focused == ui_dro_type_font) {
        populate_font_directories(NULL);
        populate_font_names(
            config.settings.font.name, config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0
        );
        list_frame_set_suppressed(list_frame_row_of(ui_lbl_font_directory_font), !font_directory_visible);
        apply_font_name_visibility();
        font_apply_lock();
    } else if (focused == ui_dro_font_directory_font) {
        char previous[MAX_BUFFER_SIZE];
        const uint32_t selected = lv_dropdown_get_selected(ui_dro_font_name_font);
        const unsigned int previous_face = selected < font_option_count ? font_options[selected].face_index : 0;
        snprintf(previous, sizeof(previous), "%s", selected < font_option_count ? font_options[selected].name : "");
        populate_font_names(previous, previous_face);
    }

    if (focused == ui_dro_type_font || focused == ui_dro_font_directory_font || focused == ui_dro_font_name_font) {
        if (focused == ui_dro_font_name_font) populate_font_axes();
        populate_font_size_options();
        apply_font_axis_visibility();
        apply_font_name_visibility();
        apply_font_size_visibility();
        list_frame_apply();
        const int steps = list_frame_steps_to_row(focused_row);
        if (steps > 0) gen_step_movement(steps, +1, 2, 0, 0);
        check_focus();
    }

    if (font_row(focused)) apply_current_font_settings();
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
    if (font_row_locked(focused) || option_kiosk_locked()) return;
    const int focused_row = list_frame_current_row();

    move_option(focused, +1);

    if (focused == ui_dro_type_font) {
        populate_font_directories(NULL);
        populate_font_names(
            config.settings.font.name, config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0
        );
        list_frame_set_suppressed(list_frame_row_of(ui_lbl_font_directory_font), !font_directory_visible);
        apply_font_name_visibility();
        font_apply_lock();
    } else if (focused == ui_dro_font_directory_font) {
        char previous[MAX_BUFFER_SIZE];
        const uint32_t selected = lv_dropdown_get_selected(ui_dro_font_name_font);
        const unsigned int previous_face = selected < font_option_count ? font_options[selected].face_index : 0;
        snprintf(previous, sizeof(previous), "%s", selected < font_option_count ? font_options[selected].name : "");
        populate_font_names(previous, previous_face);
    }

    if (focused == ui_dro_type_font || focused == ui_dro_font_directory_font || focused == ui_dro_font_name_font) {
        if (focused == ui_dro_font_name_font) populate_font_axes();
        populate_font_size_options();
        apply_font_axis_visibility();
        apply_font_name_visibility();
        apply_font_size_visibility();
        list_frame_apply();
        const int steps = list_frame_steps_to_row(focused_row);
        if (steps > 0) gen_step_movement(steps, +1, 2, 0, 0);
        check_focus();
    }

    if (font_row(focused)) apply_current_font_settings();
    refresh_overlay_preview();
}

static void restore_custom_options(void) {
    int canonical = config.settings.advanced.font;
    if (!has_theme_type && canonical == 1) canonical = 2;

    config.settings.advanced.font = (int16_t) canonical;
    lv_dropdown_set_selected(ui_dro_type_font, type_to_dropdown(canonical));

    char directory[MAX_BUFFER_SIZE];
    snprintf(directory, sizeof(directory), "%s", config.settings.font.directory);
    if (!directory[0]) {
        const char *slash = strrchr(config.settings.font.name, '/');
        if (slash)
            snprintf(
                directory, sizeof(directory), "%.*s", (int) (slash - config.settings.font.name),
                config.settings.font.name
            );
    }

    populate_font_directories(directory);
    populate_font_names(
        config.settings.font.name, config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0
    );
    select_font_name(
        config.settings.font.name, config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0
    );
    populate_font_size_options();
    list_frame_set_suppressed(list_frame_row_of(ui_lbl_font_directory_font), !font_directory_visible);
    apply_font_name_visibility();
    apply_font_axis_visibility();
    apply_font_size_visibility();
    list_frame_apply();

    map_drop_down_to_index(
        ui_dro_list_size_font, config.settings.font.list_size, active_font_size_values, active_font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_header_size_font, config.settings.font.header_size, active_font_size_values, active_font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_footer_size_font, config.settings.font.footer_size, active_font_size_values, active_font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_panel_size_font, config.settings.font.panel_size, active_font_size_values, active_font_size_count, 0
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
    lv_dropdown_set_selected(ui_dro_random_theme_custom, config.settings.advanced.random_theme);

    lv_dropdown_set_selected(ui_dro_header_height_custom, (uint32_t) (config.settings.themeopt.header_height + 1));
    lv_dropdown_set_selected(ui_dro_footer_height_custom, (uint32_t) (config.settings.themeopt.footer_height + 1));
    restore_count_dropdown(ui_dro_content_item_count_custom, config.settings.themeopt.content_item_count);

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
        if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/advanced/font", INT, config.settings.advanced.font))
            save_failed++;
    }
#define SAVE_FONT_SIZE(NAME, FILE)                                                                                     \
    do {                                                                                                               \
        if (config.settings.font.NAME != font_##NAME##_saved) {                                                        \
            is_modified++;                                                                                             \
            if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/font/" FILE, INT, config.settings.font.NAME))    \
                save_failed++;                                                                                         \
        }                                                                                                              \
    } while (0)
    SAVE_FONT_SIZE(list_size, "list_size");
    SAVE_FONT_SIZE(header_size, "header_size");
    SAVE_FONT_SIZE(footer_size, "footer_size");
    SAVE_FONT_SIZE(panel_size, "panel_size");
#undef SAVE_FONT_SIZE

    if (config.settings.advanced.font != 1) {
        if (strcasecmp(config.settings.font.directory, font_directory_saved) != 0) {
            is_modified++;
            if (!write_text_to_file_atomic(
                    CONF_CONFIG_PATH "settings/font/directory", CHAR, config.settings.font.directory
                ))
                save_failed++;
        }

        if (strcasecmp(config.settings.font.name, font_name_saved) != 0) {
            is_modified++;
            if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/font/name", CHAR, config.settings.font.name))
                save_failed++;
        }

        if (config.settings.font.face != font_face_saved) {
            is_modified++;
            if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/font/face", INT, config.settings.font.face))
                save_failed++;
        }

#define SAVE_FONT_AXIS(NAME, FILE)                                                                                     \
    do {                                                                                                               \
        if (config.settings.font.NAME != font_##NAME##_saved) {                                                        \
            is_modified++;                                                                                             \
            if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/font/" FILE, INT, config.settings.font.NAME))    \
                save_failed++;                                                                                         \
        }                                                                                                              \
    } while (0)
        SAVE_FONT_AXIS(width, "width");
        SAVE_FONT_AXIS(italic, "italic");
#undef SAVE_FONT_AXIS
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
    CHECK_AND_SAVE_STD(custom, random_theme, "settings/advanced/random_theme", INT, 0);
    CHECK_AND_SAVE_STD(custom, header_height, "settings/theme/header_height", INT, -1);
    CHECK_AND_SAVE_STD(custom, footer_height, "settings/theme/footer_height", INT, -1);
    save_count_dropdown(
        ui_dro_content_item_count_custom, content_item_count_original,
        CONF_CONFIG_PATH "settings/theme/content_item_count", &config.settings.themeopt.content_item_count, &is_modified
    );

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

        if (!write_text_to_file_atomic(CONF_CONFIG_PATH "settings/general/theme_resolution", INT, idx_theme_resolution))
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

typedef enum {
    menu_section_header = 0,
    menu_section_appearance,
    menu_section_labels,
    menu_section_font,
    menu_section_folders,
    menu_section_content,
    menu_section_box_art,
    menu_section_grid,
    menu_section_launching,
    menu_section_packages,
    menu_section_theme,
    menu_section_layout,
    menu_section_glyphs,
    menu_section_background,
    menu_section_audio,
    menu_section_count
} menu_section;

typedef enum { menu_nav_change = 0, menu_nav_select, menu_nav_change_set } menu_nav;

typedef int (*visible_fn)(void);

typedef struct {
    const char *id;
    menu_section section;
    const char *mux_name;
    const char *launch_path;
    int16_t *kiosk_flag;
    menu_action action;
    visible_fn visible;
    menu_nav nav;
} menu_entry;

static int16_t kiosk_pass = 0;

#define CUSTOM_MENU_SCHEMA(ROW)                                                                                        \
    ROW(visual, battery, "battery", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)              \
    ROW(visual, clock, "clock", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                  \
    ROW(visual, network, "network", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)              \
    ROW(visual, bluetooth, "bluetooth", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)          \
    ROW(visual, sort_order, "sortorder", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)         \
    ROW(visual, tag_order, "tagorder", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)           \
    ROW(visual, header_title, "headertitle", header, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)     \
    ROW(visual, element_transition, "elementtransition", appearance, menu_option, NULL, NULL, &kiosk.setting.visual,   \
        NULL, change)                                                                                                  \
    ROW(visual, selection_animation, "selectionanimation", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, \
        NULL, change)                                                                                                  \
    ROW(visual, selection_style, "selectionstyle", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,   \
        change)                                                                                                        \
    ROW(visual, list_glyph, "listglyph", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)     \
    ROW(visual, render_shadows, "rendershadows", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,     \
        change)                                                                                                        \
    ROW(visual, notify_time, "notifytime", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)   \
    ROW(visual, overlay_image, "overlayimage", appearance, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,       \
        change)                                                                                                        \
    ROW(visual, overlay_transparency, "overlaytransparency", appearance, menu_option, NULL, NULL,                      \
        &kiosk.setting.visual, NULL, change)                                                                           \
    ROW(visual, name_scroll, "namescroll", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)       \
    ROW(visual, label_scroll_speed, "labelscrollspeed", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,  \
        change)                                                                                                        \
    ROW(visual, name, "name", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                    \
    ROW(visual, dash, "dash", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                    \
    ROW(visual, the_title_format, "thetitleformat", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, friendly_folder, "friendlyfolder", labels, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,       \
        change)                                                                                                        \
    ROW(visual, title_include_root_drive, "titleincluderootdrive", labels, menu_option, NULL, NULL,                    \
        &kiosk.setting.visual, NULL, change)                                                                           \
    ROW(font, type, "type", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                        \
    ROW(font, font_directory, "font", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)              \
    ROW(font, font_name, "fontname", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)               \
    ROW(font, width, "width", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                      \
    ROW(font, italic, "italic", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)                    \
    ROW(font, list_size, "listsize", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)               \
    ROW(font, header_size, "headersize", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)           \
    ROW(font, footer_size, "footersize", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)           \
    ROW(font, panel_size, "panelsize", font, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)             \
    ROW(visual, folder_item_count, "folderitemcount", folders, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,   \
        change)                                                                                                        \
    ROW(visual, menu_counter_folder, "menucounterfolder", folders, menu_option, NULL, NULL, &kiosk.setting.visual,     \
        NULL, change)                                                                                                  \
    ROW(visual, menu_counter_file, "menucounterfile", folders, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,   \
        change)                                                                                                        \
    ROW(visual, display_empty_folder, "displayemptyfolder", folders, menu_option, NULL, NULL, &kiosk.setting.visual,   \
        NULL, change)                                                                                                  \
    ROW(visual, hidden, "hidden", folders, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)               \
    ROW(visual, group_content, "groupcontent", folders, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)  \
    ROW(custom, sort, "sort", content, menu_sort, "sort", NULL, &kiosk_pass, NULL, select)                             \
    ROW(visual, content_collect, "contentcollect", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, content_history, "contenthistory", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, mixed_content, "mixedcontent", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)  \
    ROW(visual, forward_history, "forwardhistory", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, content_width, "width", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)         \
    ROW(visual, video_preview, "videopreview", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)  \
    ROW(visual, page_skip, "pageskip", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)          \
    ROW(visual, shuffle, "shuffle", content, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)             \
    ROW(visual, box_art, "boxart", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)              \
    ROW(visual, box_art_align, "align", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)         \
    ROW(visual, box_art_transition, "boxarttransition", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, \
        change)                                                                                                        \
    ROW(visual, box_art_scale, "boxartscale", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)   \
    ROW(visual, box_art_padding, "boxartpadding", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,       \
        change)                                                                                                        \
    ROW(visual, box_art_placeholder, "boxartplaceholder", box_art, menu_option, NULL, NULL, &kiosk.setting.visual,     \
        NULL, change)                                                                                                  \
    ROW(visual, save_screenshot, "savescreenshot", box_art, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, grid_mode_content, "gridmodecontent", grid, menu_option, NULL, NULL, &kiosk.setting.visual, NULL,      \
        change)                                                                                                        \
    ROW(visual, box_art_hide, "boxarthide", grid, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)        \
    ROW(visual, launch_swap, "launch_swap", launching, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)   \
    ROW(visual, launchsplash, "splash", launching, menu_option, NULL, NULL, &kiosk.setting.visual, NULL, change)       \
    ROW(visual, pickles_startup_messages, "picklesstartupmessages", launching, menu_option, NULL, NULL,                \
        &kiosk.setting.visual, NULL, change)                                                                           \
    ROW(custom, black_fade, "blackfade", launching, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                \
    ROW(custom, catalogue, "catalogue", packages, menu_catalogue, "catalogue", "package/catalogue",                    \
        &kiosk.custom.catalogue, NULL, select)                                                                         \
    ROW(custom, config, "config", packages, menu_config, "config", "package/config", &kiosk.custom.raconfig, NULL,     \
        select)                                                                                                        \
    ROW(custom, logo, "logo", theme, menu_logo, "logo", NULL, &kiosk_pass, NULL, select)                               \
    ROW(custom, theme, "theme", theme, menu_theme, "theme", "/theme", &kiosk.custom.theme, NULL, select)               \
    ROW(custom, theme_resolution, "resolution", theme, menu_option, NULL, NULL, &kiosk_pass, NULL, change)             \
    ROW(custom, theme_scaling, "scaling", theme, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                   \
    ROW(custom, theme_alternate, "alternate", theme, menu_theme_alternate, NULL, NULL, &kiosk_pass,                    \
        visible_theme_alternate, change_set)                                                                           \
    ROW(custom, random_theme, "randomtheme", layout, menu_option, NULL, NULL, &kiosk_pass, NULL, change)               \
    ROW(custom, header_height, "headerheight", layout, menu_option, NULL, NULL, &kiosk_pass, NULL, change)             \
    ROW(custom, footer_height, "footerheight", layout, menu_option, NULL, NULL, &kiosk_pass, NULL, change)             \
    ROW(custom, content_item_count, "count", layout, menu_option, NULL, NULL, &kiosk_pass, NULL, change)               \
    ROW(custom, label_width, "labelwidth", layout, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                 \
    ROW(custom, glyph_list, "glyphlist", glyphs, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                   \
    ROW(custom, glyph_header, "glyphheader", glyphs, menu_option, NULL, NULL, &kiosk_pass, NULL, change)               \
    ROW(custom, glyph_footer, "glyphfooter", glyphs, menu_option, NULL, NULL, &kiosk_pass, NULL, change)               \
    ROW(custom, glyph_grid, "glyphgrid", glyphs, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                   \
    ROW(custom, video_wallpaper, "videowallpaper", background, menu_option, NULL, NULL, &kiosk_pass, NULL, change)     \
    ROW(custom, background_scale, "backgroundscale", background, menu_option, NULL, NULL, &kiosk_pass, NULL, change)   \
    ROW(custom, music, "music", audio, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                             \
    ROW(custom, music_volume, "musicvolume", audio, menu_music_volume, NULL, NULL, &kiosk_pass, NULL, change_set)      \
    ROW(custom, sound, "sound", audio, menu_option, NULL, NULL, &kiosk_pass, NULL, change)                             \
    ROW(custom, sound_volume, "soundvolume", audio, menu_sound_volume, NULL, NULL, &kiosk_pass, NULL, change_set)      \
    ROW(custom, chime, "chime", audio, menu_option, NULL, NULL, &kiosk_pass, NULL, change)

#define MENU_ENTRY(MODULE, NAME, ID, SECTION, ACTION, MUX, PATH, KIOSK, VISIBLE, NAV)                                  \
    {ID, menu_section_##SECTION, MUX, PATH, KIOSK, ACTION, VISIBLE, menu_nav_##NAV},
static const menu_entry custom_menu_entries[] = {CUSTOM_MENU_SCHEMA(MENU_ENTRY)};
#undef MENU_ENTRY

_Static_assert(A_SIZE(custom_menu_entries) == ui_count_dynamic, "custom menu schema must describe every row");

static int option_kiosk_locked(void) {
    const int row = list_frame_current_row();
    if (row < 0 || row >= (int) A_SIZE(custom_menu_entries)) return 0;
    if (!is_ksk(*custom_menu_entries[row].kiosk_flag)) return 0;

    kiosk_denied();
    return 1;
}

static const char *custom_menu_section_label(const menu_section section) {
    switch (section) {
        case menu_section_header:
            return lang.muxvisual.section.header_bar;
        case menu_section_appearance:
            return lang.muxvisual.section.appearance;
        case menu_section_labels:
            return lang.muxvisual.section.labels;
        case menu_section_font:
            return lang.muxvisual.section.font;
        case menu_section_folders:
            return lang.muxvisual.section.folders;
        case menu_section_content:
            return lang.muxvisual.section.content;
        case menu_section_box_art:
            return lang.muxvisual.section.box_art;
        case menu_section_grid:
            return lang.muxvisual.section.grid;
        case menu_section_launching:
            return lang.muxvisual.section.launching;
        case menu_section_packages:
            return lang.muxcustom.section.packages;
        case menu_section_theme:
            return lang.muxcustom.section.theme;
        case menu_section_layout:
            return lang.muxcustom.section.layout;
        case menu_section_glyphs:
            return lang.muxcustom.section.glyphs;
        case menu_section_background:
            return lang.muxcustom.section.background;
        case menu_section_audio:
            return lang.muxcustom.section.audio;
        default:
            return "";
    }
}

static int init_custom_menu_schema(lv_obj_t **panels, lv_obj_t **labels, lv_obj_t **glyphs, lv_obj_t **values) {
#define MENU_LABEL(MODULE, NAME, ID, SECTION, ACTION, MUX, PATH, KIOSK, VISIBLE, NAV) ui_lbl_##NAME##_##MODULE,
    lv_obj_t *const expected_labels[] = {CUSTOM_MENU_SCHEMA(MENU_LABEL)};
#undef MENU_LABEL

    list_frame frames[menu_section_count];
    int frame_count = 0;
    int first = 0;

    for (int i = 0; i < ui_count_dynamic; i++) {
        if (labels[i] != expected_labels[i]) {
            LOG_ERROR(mux_module, "custom menu schema mismatch at row %d (%s)", i, custom_menu_entries[i].id);
            return 0;
        }

        lv_obj_set_user_data(labels[i], (void *) custom_menu_entries[i].id);

        const int boundary =
            i + 1 == ui_count_dynamic || custom_menu_entries[i + 1].section != custom_menu_entries[i].section;
        if (!boundary) continue;

        frames[frame_count++] =
            (list_frame) {custom_menu_section_label(custom_menu_entries[i].section), first, i - first + 1};
        first = i + 1;
    }

    if (frame_count != menu_section_count) {
        LOG_ERROR(mux_module, "custom menu schema has %d sections, expected %d", frame_count, menu_section_count);
        return 0;
    }

    return list_frame_init(
        &theme, ui_pnl_content, frames, frame_count, panels, labels, glyphs, values, ui_count_dynamic
    );
}

static void apply_custom_menu_nav(void) {
    const int row = list_frame_current_row();
    const menu_nav nav =
        row >= 0 && row < (int) A_SIZE(custom_menu_entries) ? custom_menu_entries[row].nav : menu_nav_change;
    const int show_a = nav == menu_nav_select || nav == menu_nav_change_set;
    const int show_lr = nav == menu_nav_change || nav == menu_nav_change_set;

    if (show_a) {
        lv_label_set_text(ui_lbl_nav_a, nav == menu_nav_select ? lang.generic.select : lang.generic.set);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (show_lr) {
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

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

            list_frame_remember(lv_group_get_focused(ui_group));
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
    clear_font_options();

    return 0;
}
