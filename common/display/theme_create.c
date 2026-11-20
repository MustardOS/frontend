#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/platform/device.h>
#include <common/display/theme.h>
#include <common/display/theme_create.h>
#include <common/display/theme_lab.h>
#include <common/storage/fileio.h>
#include <common/ui/font.h>
#include <common/ui/nav/content_layout.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/style/tweak.h>

#define THEME_JSON_SIZE (256 * 1024)
#define SCHEME_SIZE     (64 * 1024)

static const char *const content_modules[] = {"muxplore", "muxcollect", "muxhistory", "muxlaunch"};

static struct mux_config saved_config;
static struct mux_config clean_config;
static struct mux_config custom_config;
static struct mux_config module_config;
static struct mux_device saved_device;

static struct theme_config clean_global;
static struct theme_config custom_global;
static struct theme_config clean_module;
static struct theme_config custom_module;
static struct theme_config expected_module;

static int theme_reserved(const char *name) {
    return strcasecmp(name, "MustardOS") == 0;
}

static int folder_name(const char *name, char *out, const size_t size) {
    size_t used = 0;
    for (const char *c = name; *c && used + 1 < size; c++) {
        const unsigned char ch = (unsigned char) *c;
        out[used++] = (char) (isalnum(ch) || ch == ' ' || ch == '-' || ch == '_' || ch == '(' || ch == ')' ? ch : '_');
    }
    out[used] = '\0';

    while (used > 0 && out[used - 1] == ' ')
        out[--used] = '\0';

    return out[0] && out[0] != ' ' && out[0] != '.';
}

static int resolution_name(const char *name) {
    int width, height;
    char extra;
    return sscanf(name, "%dx%d%c", &width, &height, &extra) == 2;
}

static int copy_tree(const char *source, const char *target, const char *keep_dim, const int top) {
    DIR *dir = opendir(source);
    if (!dir) return 0;

    mkdir(target, 0755);

    int ok = 1;
    const struct dirent *entry;
    while (ok && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char from[PATH_MAX];
        char to[PATH_MAX];
        snprintf(from, sizeof(from), "%s/%s", source, entry->d_name);
        snprintf(to, sizeof(to), "%s/%s", target, entry->d_name);

        struct stat info;
        if (stat(from, &info) != 0) continue;

        if (S_ISDIR(info.st_mode)) {
            if (top && strcmp(entry->d_name, "glyph") == 0) continue;
            if (top && keep_dim && resolution_name(entry->d_name) && strcmp(entry->d_name, keep_dim) != 0) continue;
            ok = copy_tree(from, to, keep_dim, 0);
        } else {
            ok = copy_file(from, to) == 0;
        }
    }

    closedir(dir);
    return ok;
}

static void clear_groups(struct mux_config *settings) {
    settings->settings.themeopt.header_height = -1;
    settings->settings.themeopt.footer_height = -1;
    settings->settings.themeopt.content_item_count = 0;
    settings->settings.themeopt.glyph_size_list = -2;
    settings->settings.themeopt.glyph_size_header = -2;
    settings->settings.themeopt.glyph_size_footer = -2;
    settings->settings.themeopt.glyph_size_grid = -2;
    settings->settings.themeopt.label_width = 0;

    for (int i = 0; i < content_layout_count; i++)
        content_layout_reset(&settings->settings.layout[i]);

    char *colours = settings->settings.themeopt.colour_highlight;
    memset(colours, 0, (size_t) (settings->settings.themeopt.colour_progress_bar + MAX_BUFFER_SIZE - colours));
}

static void copy_groups(struct mux_config *target, const struct mux_config *source, const unsigned include) {
    if (include & theme_include_colours) {
        char *colours = target->settings.themeopt.colour_highlight;
        const char *from = source->settings.themeopt.colour_highlight;
        memcpy(colours, from, (size_t) (target->settings.themeopt.colour_progress_bar + MAX_BUFFER_SIZE - colours));
    }

    if (include & theme_include_layouts) {
        target->settings.themeopt.header_height = source->settings.themeopt.header_height;
        target->settings.themeopt.footer_height = source->settings.themeopt.footer_height;
        target->settings.themeopt.content_item_count = source->settings.themeopt.content_item_count;
        target->settings.themeopt.glyph_size_list = source->settings.themeopt.glyph_size_list;
        target->settings.themeopt.glyph_size_header = source->settings.themeopt.glyph_size_header;
        target->settings.themeopt.glyph_size_footer = source->settings.themeopt.glyph_size_footer;
        target->settings.themeopt.glyph_size_grid = source->settings.themeopt.glyph_size_grid;
        target->settings.themeopt.label_width = source->settings.themeopt.label_width;

        for (int i = 0; i < content_layout_count; i++)
            target->settings.layout[i] = source->settings.layout[i];
    }
}

static void load_for(const char *module, const struct mux_config *settings, struct theme_config *out, const int plain) {
    theme_alternate_disable(plain);
    snprintf(mux_module, sizeof(mux_module), "%s", module);
    config = *settings;
    load_theme(out, &config, &device);
}

static size_t add(char *out, const size_t used, const char *format, const char *text) {
    if (used >= THEME_JSON_SIZE) return used;
    return used + (size_t) snprintf(out + used, THEME_JSON_SIZE - used, format, text);
}

static int build_json(const unsigned include, const int menu_style, char *out, char *scheme) {
    const int grid_style = menu_style_index("grid");

    load_for("muxcustom", &clean_config, &clean_global, 1);
    load_for("muxcustom", &custom_config, &custom_global, 0);
    theme_scheme_diff(&clean_global, &custom_global, scheme, SCHEME_SIZE);

    size_t used = add(out, 0, "{\"scheme\":%s,\"modules\":{", scheme);
    int modules = 0;

    for (size_t i = 0; i < A_SIZE(content_modules); i++) {
        const int launch = strcmp(content_modules[i], "muxlaunch") == 0;

        module_config = clean_config;
        if (launch) module_config.settings.themeopt.menu_style = (int16_t) grid_style;
        load_for(content_modules[i], &module_config, &clean_module, 1);

        module_config = custom_config;
        if (launch) module_config.settings.themeopt.menu_style = (int16_t) grid_style;
        load_for(content_modules[i], &module_config, &custom_module, 0);

        if (launch && (include & theme_include_menu) && menu_style > menu_style_theme) {
            snprintf(
                custom_module.misc.menu_style_name, sizeof(custom_module.misc.menu_style_name), "%s",
                menu_style_key(menu_style)
            );
        }

        expected_module = clean_module;
        theme_scheme_overlay(&expected_module, &clean_global, &custom_global);
        theme_scheme_diff(&expected_module, &custom_module, scheme, SCHEME_SIZE);

        used = add(out, used, modules++ ? ",\"%s\":{" : "\"%s\":{", content_modules[i]);
        used = add(out, used, "\"scheme\":%s", scheme);

        if (launch && (include & theme_include_menu) && menu_style > menu_style_theme
            && menu_style_customised(menu_style)) {
            module_config = custom_config;
            module_config.settings.themeopt.menu_style = (int16_t) menu_style;
            load_for("muxlaunch", &module_config, &custom_module, 0);

            if (style_tweaks_document(scheme, SCHEME_SIZE)) {
                used = add(out, used, ",\"styles\":{\"%s\":", menu_style_key(menu_style));
                used = add(out, used, "%s}", scheme);
            }
        }

        used = add(out, used, "%s", "}");
    }

    used = add(out, used, "%s", "}}");
    return used < THEME_JSON_SIZE;
}

static int write_text(const char *folder, const char *name, const char *text) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", folder, name);
    return write_text_to_file(path, "w", CHAR, text);
}

int theme_create(const char *name, const unsigned include, char *folder, const size_t folder_size) {
    char safe[MAX_BUFFER_SIZE];
    if (!name || !folder_name(name, safe, sizeof(safe))) return theme_create_name;

    char target[PATH_MAX];
    snprintf(target, sizeof(target), RUN_STORAGE_PATH "theme/%s", safe);
    if (theme_reserved(safe) || dir_exist(target)) return theme_create_exists;

    char font[PATH_MAX] = "";
    if (include & theme_include_font) {
        const size_t length = font_custom_file(font, sizeof(font)) ? strlen(font) : 0;
        if (length < 4 || strcasecmp(font + length - 4, ".ttf") != 0) font[0] = '\0';
    }

    char *out = malloc(THEME_JSON_SIZE);
    char *scheme = malloc(SCHEME_SIZE);
    if (!out || !scheme) {
        free(out);
        free(scheme);
        return theme_create_failed;
    }

    saved_config = config;
    saved_device = device;
    const char *saved_base = theme_base;

    char saved_module[MAX_BUFFER_SIZE];
    char saved_dim[sizeof(mux_dim)];
    snprintf(saved_module, sizeof(saved_module), "%s", mux_module);
    memcpy(saved_dim, mux_dim, sizeof(saved_dim));

    const int menu_style = menu_style_resolve(saved_config.settings.themeopt.menu_style);

    clean_config = saved_config;
    clear_groups(&clean_config);
    custom_config = clean_config;
    copy_groups(&custom_config, &saved_config, include);

    content_layout_folder_use(0);
    theme_base = INTERNAL_THEME;

    const int built = build_json(include, menu_style, out, scheme);

    char keep_dim[sizeof(mux_dim)];
    snprintf(keep_dim, sizeof(keep_dim), "%s", mux_dim);
    const size_t keep_length = strlen(keep_dim);
    if (keep_length && keep_dim[keep_length - 1] == '/') keep_dim[keep_length - 1] = '\0';

    theme_alternate_disable(0);
    config = saved_config;
    device = saved_device;
    theme_base = saved_base;
    snprintf(mux_module, sizeof(mux_module), "%s", saved_module);
    memcpy(mux_dim, saved_dim, sizeof(saved_dim));
    load_theme(&theme, &config, &device);

    char dim_path[PATH_MAX];
    snprintf(dim_path, sizeof(dim_path), "%s/%s", INTERNAL_THEME, keep_dim);

    int ok = built && copy_tree(INTERNAL_THEME, target, dir_exist(dim_path) ? keep_dim : NULL, 1);
    ok = ok && write_text(target, "theme.json", out);
    ok = ok && write_text(target, "active.txt", "");
    ok = ok && write_text(target, "name.txt", name);
    ok = ok && write_text(target, "credits.txt", "Created on device with Create Theme");
    ok = ok && write_text(target, "lab.txt", "MustardOS");

    if (ok && font[0]) {
        char font_path[PATH_MAX];
        snprintf(font_path, sizeof(font_path), "%s/font/default.ttf", target);
        create_directories(font_path, 1);
        ok = copy_file(font, font_path) == 0;
    }

    free(out);
    free(scheme);

    if (!ok) {
        remove_directory_recursive(target);
        return theme_create_failed;
    }

    snprintf(folder, folder_size, "%s", safe);
    return theme_create_ok;
}

static int theme_folder_listed(const char *name) {
    if (name[0] == '.' || strchr(name, '/') || theme_reserved(name)) return 0;
    if (strcmp(name, "active") == 0 || strcmp(name, "override") == 0) return 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s", name);
    return dir_exist(path);
}

int theme_lab_own(const char *folder) {
    if (!folder || !*folder || folder[0] == '.' || strchr(folder, '/')) return 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/lab.txt", folder);
    return file_exist(path);
}

int theme_delete(const char *folder) {
    if (!theme_lab_own(folder)) return theme_create_name;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s", folder);
    return remove_directory_recursive(path) == 0 ? theme_create_ok : theme_create_failed;
}

int theme_remix(const char *source, char *folder, const size_t folder_size) {
    const int builtin = source && theme_reserved(source);
    if (!source || (!builtin && !theme_folder_listed(source))) return theme_create_name;

    char from[PATH_MAX];
    snprintf(from, sizeof(from), RUN_STORAGE_PATH "theme/%s", source);
    if (builtin) snprintf(from, sizeof(from), "%s", INTERNAL_THEME);

    char base[MAX_BUFFER_SIZE];
    char wanted[MAX_BUFFER_SIZE];
    snprintf(wanted, sizeof(wanted), "%s Remix", source);
    if (!folder_name(wanted, base, sizeof(base))) return theme_create_name;

    char safe[MAX_BUFFER_SIZE];
    char target[PATH_MAX];
    int found = 0;
    for (int copy = 1; copy < 100 && !found; copy++) {
        if (copy == 1) {
            snprintf(safe, sizeof(safe), "%s", base);
        } else {
            snprintf(safe, sizeof(safe), "%.200s %d", base, copy);
        }
        snprintf(target, sizeof(target), RUN_STORAGE_PATH "theme/%s", safe);
        found = !dir_exist(target);
    }
    if (!found) return theme_create_exists;

    int ok = copy_tree(from, target, NULL, 0);
    ok = ok && write_text(target, "name.txt", safe);
    ok = ok && write_text(target, "lab.txt", source);
    ok = ok && lab_scheme_to_json(safe) >= 0;

    if (!ok) {
        remove_directory_recursive(target);
        return theme_create_failed;
    }

    snprintf(folder, folder_size, "%s", safe);
    return theme_create_ok;
}

int theme_rename(const char *folder, const char *name, char *out, const size_t out_size) {
    if (!theme_lab_own(folder)) return theme_create_name;

    char safe[MAX_BUFFER_SIZE];
    if (!name || !folder_name(name, safe, sizeof(safe))) return theme_create_name;

    char from[PATH_MAX];
    char target[PATH_MAX];
    snprintf(from, sizeof(from), RUN_STORAGE_PATH "theme/%s", folder);
    snprintf(target, sizeof(target), RUN_STORAGE_PATH "theme/%s", safe);

    if (theme_reserved(safe)) return theme_create_exists;
    if (strcmp(folder, safe) != 0) {
        if (dir_exist(target)) return theme_create_exists;
        if (file_rename(from, target) != 0) return theme_create_failed;
    }

    if (!write_text(target, "name.txt", name)) return theme_create_failed;

    snprintf(out, out_size, "%s", safe);
    return theme_create_ok;
}
