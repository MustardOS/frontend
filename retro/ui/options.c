#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "../coredef/coredef.h"
#include "options.h"
#include "../core/paths.h"

struct core_option_entry options_list[OPTIONS_MAX];
int options_count = 0;
struct core_option_category options_categories[OPTIONS_MAX_CATEGORIES];
int options_category_count = 0;
bool options_dirty = false;

static char active_core_name[MAX_BUFFER_SIZE] = "";
static char core_ini_path[MAX_BUFFER_SIZE] = "";
static char content_ini_path[MAX_BUFFER_SIZE] = "";
static char directory_ini_path[MAX_BUFFER_SIZE] = "";

static int baseline_indices[OPTIONS_MAX];

static mini_t *ovr_core = NULL;
static mini_t *ovr_directory = NULL;
static mini_t *ovr_content = NULL;

static void warn_if_truncated(const char *dropped_key) {
    if (!dropped_key) return;

    LOG_WARN(mux_module, "core options truncated at %d entries - '%s' onwards were dropped", OPTIONS_MAX, dropped_key);
}

// A core is free to offer as many values as it likes, and some offer well over
// a hundred (Like FBNEO), so the list is sized to whatever it actually declares
static char (*alloc_values(const int count)) [OPTIONS_VALUE_LEN] {
    if (count <= 0) return NULL;

    return calloc((size_t) count, sizeof(char[OPTIONS_VALUE_LEN]));
}

static void store_values(
    struct core_option_entry *e, const struct retro_core_option_value *values, const char *default_value
) {
    e->values = NULL;
    e->value_count = 0;
    e->current_index = 0;

    int total = 0;
    for (int v = 0; values[v].value; v++)
        total++;

    e->values = alloc_values(total);
    if (!e->values) {
        if (total > 0) LOG_ERROR(mux_module, "no memory for the %d values of core option '%s'", total, e->key);
        return;
    }

    for (int v = 0; v < total; v++)
        snprintf(e->values[v], OPTIONS_VALUE_LEN, "%s", values[v].value);

    e->value_count = total;

    if (!default_value) return;

    for (int v = 0; v < total; v++) {
        if (strcmp(e->values[v], default_value) != 0) continue;

        e->current_index = v;
        return;
    }

    LOG_WARN(mux_module, "core option '%s' default '%s' is not in its own value list", e->key, default_value);
}

static void open_overrides(void) {
    ovr_core = core_ini_path[0] ? mini_try_load(core_ini_path) : NULL;
    ovr_directory = directory_ini_path[0] ? mini_try_load(directory_ini_path) : NULL;
    ovr_content = content_ini_path[0] ? mini_try_load(content_ini_path) : NULL;
}

static void close_overrides(void) {
    if (ovr_core) mini_free(ovr_core);
    if (ovr_directory) mini_free(ovr_directory);
    if (ovr_content) mini_free(ovr_content);

    ovr_core = NULL;
    ovr_directory = NULL;
    ovr_content = NULL;
}

static void apply_value(struct core_option_entry *e, const char *value) {
    if (!value || !*value) return;

    for (int v = 0; v < e->value_count; v++) {
        if (strcmp(e->values[v], value) == 0) {
            e->current_index = v;
            return;
        }
    }
}

static void apply_override(struct core_option_entry *e) {
    apply_value(e, coredef_lookup(active_core_name, e->key));
    mini_t *inis[] = {ovr_core, ovr_directory, ovr_content};

    for (int p = 0; p < 3; p++) {
        if (!inis[p]) continue;

        apply_value(e, get_ini_string(inis[p], "options", e->key, ""));
    }
}

void options_reset(void) {
    for (int i = 0; i < options_count; i++)
        free(options_list[i].values);

    options_count = 0;
    memset(options_list, 0, sizeof(options_list));
    options_category_count = 0;
    memset(options_categories, 0, sizeof(options_categories));
}

void options_store_v1(const struct retro_core_option_definition *defs) {
    options_reset();
    open_overrides();

    int i = 0;
    for (; defs[i].key && options_count < OPTIONS_MAX; i++) {
        struct core_option_entry *e = &options_list[options_count];
        snprintf(e->key, sizeof(e->key), "%s", defs[i].key);
        snprintf(e->label, sizeof(e->label), "%s", defs[i].desc ? defs[i].desc : defs[i].key);

        store_values(e, defs[i].values, defs[i].default_value);
        apply_override(e);
        options_count++;
    }

    warn_if_truncated(defs[i].key);

    close_overrides();
    options_dirty = true;
}

static int category_key_is_valid(const char *key, const struct retro_core_option_v2_category *categories) {
    if (!key || !*key || !categories) return 0;

    for (int i = 0; categories[i].key; i++) {
        if (strcmp(categories[i].key, key) == 0) return 1;
    }

    return 0;
}

void options_store_v2(const struct retro_core_options_v2 *opts) {
    options_reset();
    open_overrides();

    if (opts->categories) {
        for (int i = 0; opts->categories[i].key && options_category_count < OPTIONS_MAX_CATEGORIES; i++) {
            struct core_option_category *c = &options_categories[options_category_count];
            snprintf(c->key, sizeof(c->key), "%s", opts->categories[i].key);
            snprintf(
                c->label, sizeof(c->label), "%s",
                opts->categories[i].desc ? opts->categories[i].desc : opts->categories[i].key
            );
            options_category_count++;
        }
    }

    const struct retro_core_option_v2_definition *defs = opts->definitions;

    int i = 0;
    for (; defs[i].key && options_count < OPTIONS_MAX; i++) {
        struct core_option_entry *e = &options_list[options_count];
        snprintf(e->key, sizeof(e->key), "%s", defs[i].key);

        const int categorised = category_key_is_valid(defs[i].category_key, opts->categories);
        if (categorised) snprintf(e->category_key, sizeof(e->category_key), "%s", defs[i].category_key);

        const char *label = categorised && defs[i].desc_categorized && *defs[i].desc_categorized
                                ? defs[i].desc_categorized
                                : defs[i].desc;
        snprintf(e->label, sizeof(e->label), "%s", label ? label : defs[i].key);

        store_values(e, defs[i].values, defs[i].default_value);
        apply_override(e);
        options_count++;
    }

    warn_if_truncated(defs[i].key);

    close_overrides();
    options_dirty = true;
}

void options_store_legacy(const struct retro_variable *vars) {
    options_reset();
    open_overrides();

    int i = 0;
    for (; vars[i].key && options_count < OPTIONS_MAX; i++) {
        struct core_option_entry *e = &options_list[options_count];
        snprintf(e->key, sizeof(e->key), "%s", vars[i].key);

        const char *desc_sep = strchr(vars[i].value, ';');
        if (desc_sep) {
            const int len = (int) (desc_sep - vars[i].value);
            snprintf(e->label, sizeof(e->label), "%.*s", len, vars[i].value);
        } else {
            snprintf(e->label, sizeof(e->label), "%s", vars[i].key);
        }

        e->values = NULL;
        e->value_count = 0;
        e->current_index = 0;

        const char *start = desc_sep ? desc_sep + 1 : vars[i].value;
        while (*start == ' ')
            start++;

        int total = *start ? 1 : 0;
        for (const char *scan = start; *scan; scan++)
            if (*scan == '|') total++;

        e->values = alloc_values(total);

        const char *cursor = start;
        while (e->values && *cursor && e->value_count < total) {
            const char *sep = strchr(cursor, '|');
            size_t len = sep ? (size_t) (sep - cursor) : strlen(cursor);
            if (len >= OPTIONS_VALUE_LEN) len = OPTIONS_VALUE_LEN - 1;

            memcpy(e->values[e->value_count], cursor, len);
            e->values[e->value_count][len] = '\0';
            e->value_count++;

            if (!sep) break;
            cursor = sep + 1;
        }
        apply_override(e);
        options_count++;
    }

    warn_if_truncated(vars[i].key);

    close_overrides();
    options_dirty = true;
}

int options_find(const char *key) {
    for (int i = 0; i < options_count; i++) {
        if (strcmp(options_list[i].key, key) == 0) return i;
    }

    return -1;
}

int options_set(const char *key, const char *value) {
    const int index = options_find(key);
    if (index < 0) return 0;

    struct core_option_entry *e = &options_list[index];
    for (int i = 0; i < e->value_count; i++) {
        if (strcmp(e->values[i], value) != 0) continue;

        if (e->current_index != i) {
            e->current_index = i;
            options_dirty = true;
        }

        return 1;
    }

    return 0;
}

const char *options_get_value(const char *key) {
    for (int i = 0; i < options_count; i++) {
        if (strcmp(options_list[i].key, key) == 0) {
            if (options_list[i].value_count == 0) return NULL;
            return options_list[i].values[options_list[i].current_index];
        }
    }

    return NULL;
}

void options_cycle(const int index, const int direction) {
    if (index < 0 || index >= options_count) return;

    struct core_option_entry *e = &options_list[index];
    if (e->value_count == 0) return;

    e->current_index = (e->current_index + direction + e->value_count) % e->value_count;
    options_dirty = true;
}

static void options_save(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) ini = mini_create(path);
    if (!ini) return;

    for (int i = 0; i < options_count; i++) {
        const struct core_option_entry *e = &options_list[i];
        if (e->value_count == 0) continue;
        mini_set_string(ini, "options", e->key, e->values[e->current_index]);
    }

    mini_save(ini, 0);
    mini_free(ini);
}

static void snapshot_baseline(void) {
    for (int i = 0; i < options_count; i++)
        baseline_indices[i] = options_list[i].current_index;
}

void options_init_paths(const char *core_path_arg, const char *content_path) {
    char core_name[MAX_BUFFER_SIZE];
    if (!core_get_name(core_path_arg, core_name, sizeof(core_name))
        || !str_copy_checked(active_core_name, sizeof(active_core_name), core_name)
        || !str_format_checked(core_ini_path, sizeof(core_ini_path), "%s/core/%s.ini", RETRO_OPT_PATH, core_name)) {
        LOG_ERROR(mux_module, "Core options path is too long");
        return;
    }
    create_directories(core_ini_path, 1);

    char rel_dir[MAX_BUFFER_SIZE];
    if (!core_content_rel_dir(content_path, rel_dir, sizeof(rel_dir))) {
        LOG_ERROR(mux_module, "Core options directory path is too long");
        return;
    }

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    if (!str_copy_checked(content_stem, sizeof(content_stem), content_base)) {
        LOG_ERROR(mux_module, "Core options content name is too long");
        return;
    }
    char *content_dot = strrchr(content_stem, '.');
    if (content_dot) *content_dot = '\0';

    if (*rel_dir) {
        if (!str_format_checked(
                content_ini_path, sizeof(content_ini_path), "%s/content/%s/%s.ini", RETRO_OPT_PATH, rel_dir,
                content_stem
            )
            || !str_format_checked(
                directory_ini_path, sizeof(directory_ini_path), "%s/directory/%s/directory.ini", RETRO_OPT_PATH, rel_dir
            )) {
            LOG_ERROR(mux_module, "Core options hierarchy path is too long");
            return;
        }
    } else {
        if (!str_format_checked(
                content_ini_path, sizeof(content_ini_path), "%s/content/%s.ini", RETRO_OPT_PATH, content_stem
            )
            || !str_format_checked(
                directory_ini_path, sizeof(directory_ini_path), "%s/directory/directory.ini", RETRO_OPT_PATH
            )) {
            LOG_ERROR(mux_module, "Core options hierarchy path is too long");
            return;
        }
    }
    create_directories(content_ini_path, 1);
    create_directories(directory_ini_path, 1);
}

void options_log_resolved(void) {
    LOG_INFO(mux_module, "core options resolved: %d entries", options_count);

    for (int i = 0; i < options_count; i++) {
        const struct core_option_entry *e = &options_list[i];

        LOG_INFO(
            mux_module, "  %s = %s (%d of %d)", e->key, e->value_count ? e->values[e->current_index] : "?",
            e->current_index + 1, e->value_count
        );
    }
}

void options_capture_baseline(void) {
    snapshot_baseline();
    options_log_resolved();
}

void options_profile_capture(int indices[OPTIONS_MAX]) {
    if (!indices) return;

    for (int i = 0; i < OPTIONS_MAX; i++)
        indices[i] = i < options_count ? options_list[i].current_index : -1;
}

int options_profile_matches(const int indices[OPTIONS_MAX]) {
    if (!indices) return 0;

    for (int i = 0; i < options_count; i++)
        if (indices[i] != options_list[i].current_index) return 0;

    return 1;
}

int options_profile_baseline_matches(void) {
    for (int i = 0; i < options_count; i++)
        if (baseline_indices[i] != options_list[i].current_index) return 0;

    return 1;
}

int options_profile_value_index(const int option_index, const char *value) {
    if (option_index < 0 || option_index >= options_count || !value) return -1;

    const struct core_option_entry *entry = &options_list[option_index];
    for (int value_index = 0; value_index < entry->value_count; value_index++)
        if (strcmp(entry->values[value_index], value) == 0) return value_index;

    return -1;
}

void options_profile_apply(const int indices[OPTIONS_MAX], const unsigned char present[OPTIONS_MAX]) {
    int changed = 0;

    for (int i = 0; i < options_count; i++) {
        int next = baseline_indices[i];
        if (indices && present && present[i] && indices[i] >= 0 && indices[i] < options_list[i].value_count)
            next = indices[i];

        if (options_list[i].current_index == next) continue;
        options_list[i].current_index = next;
        changed = 1;
    }

    if (changed) options_dirty = true;
}

int options_is_dirty(void) {
    for (int i = 0; i < options_count; i++) {
        if (options_list[i].current_index != baseline_indices[i]) return 1;
    }
    return 0;
}

void options_discard(void) {
    for (int i = 0; i < options_count; i++)
        options_list[i].current_index = baseline_indices[i];
}

void options_save_content(void) {
    options_save(content_ini_path);
    snapshot_baseline();
}

void options_save_core(void) {
    options_save(core_ini_path);
    snapshot_baseline();
}

void options_save_directory(void) {
    options_save(directory_ini_path);
    snapshot_baseline();
}

static int delete_saved_option_file(const char *path) {
    if (!path || !*path) return 1;
    if (unlink(path) == 0 || errno == ENOENT) return 1;

    LOG_ERROR(mux_module, "Could not delete saved core options '%s': %s", path, strerror(errno));
    return 0;
}

int options_delete_saved_overrides(void) {
    int ok = 1;
    if (!delete_saved_option_file(content_ini_path)) ok = 0;
    if (!delete_saved_option_file(directory_ini_path)) ok = 0;
    if (!delete_saved_option_file(core_ini_path)) ok = 0;
    return ok;
}
