#include <string.h>
#include "../common/fileio.h"
#include "../common/ini.h"
#include "../common/miniz/miniz.h"
#include "../common/options.h"
#include "../common/strutil.h"
#include "options.h"
#include "paths.h"

struct core_option_entry options_list[OPTIONS_MAX];
int options_count = 0;
bool options_dirty = false;

static char core_ini_path[MAX_BUFFER_SIZE] = "";
static char content_ini_path[MAX_BUFFER_SIZE] = "";
static char directory_ini_path[MAX_BUFFER_SIZE] = "";

static int baseline_indices[OPTIONS_MAX];

static mini_t *ovr_core = NULL;
static mini_t *ovr_directory = NULL;
static mini_t *ovr_content = NULL;

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

static void apply_override(struct core_option_entry *e) {
    mini_t *inis[] = {ovr_core, ovr_directory, ovr_content};

    for (int p = 0; p < 3; p++) {
        if (!inis[p]) continue;

        const char *saved = get_ini_string(inis[p], "options", e->key, "");
        if (!saved || !*saved) continue;

        for (int v = 0; v < e->value_count; v++) {
            if (strcmp(e->values[v], saved) == 0) {
                e->current_index = v;
                break;
            }
        }
    }
}

void options_reset(void) {
    options_count = 0;
    memset(options_list, 0, sizeof(options_list));
}

void options_store_v1(const struct retro_core_option_definition *defs) {
    options_reset();
    open_overrides();

    for (int i = 0; defs[i].key && options_count < OPTIONS_MAX; i++) {
        struct core_option_entry *e = &options_list[options_count];
        snprintf(e->key, sizeof(e->key), "%s", defs[i].key);
        snprintf(e->label, sizeof(e->label), "%s", defs[i].desc ? defs[i].desc : defs[i].key);

        e->value_count = 0;
        int default_index = 0;

        for (int v = 0; defs[i].values[v].value && e->value_count < OPTIONS_MAX_VALUES; v++) {
            snprintf(e->values[e->value_count], sizeof(e->values[e->value_count]), "%s", defs[i].values[v].value);

            if (defs[i].default_value && strcmp(defs[i].values[v].value, defs[i].default_value) == 0) {
                default_index = e->value_count;
            }

            e->value_count++;
        }

        e->current_index = default_index;
        apply_override(e);
        options_count++;
    }

    close_overrides();
    options_dirty = true;
}

void options_store_legacy(const struct retro_variable *vars) {
    options_reset();
    open_overrides();

    for (int i = 0; vars[i].key && options_count < OPTIONS_MAX; i++) {
        struct core_option_entry *e = &options_list[options_count];
        snprintf(e->key, sizeof(e->key), "%s", vars[i].key);

        const char *desc_sep = strchr(vars[i].value, ';');
        if (desc_sep) {
            const int len = (int) (desc_sep - vars[i].value);
            snprintf(e->label, sizeof(e->label), "%.*s", len, vars[i].value);
        } else {
            snprintf(e->label, sizeof(e->label), "%s", vars[i].key);
        }

        e->value_count = 0;
        const char *cursor = desc_sep ? desc_sep + 1 : vars[i].value;
        while (*cursor == ' ')
            cursor++;

        while (*cursor && e->value_count < OPTIONS_MAX_VALUES) {
            const char *sep = strchr(cursor, '|');
            size_t len = sep ? (size_t) (sep - cursor) : strlen(cursor);
            if (len >= sizeof(e->values[0])) len = sizeof(e->values[0]) - 1;

            memcpy(e->values[e->value_count], cursor, len);
            e->values[e->value_count][len] = '\0';
            e->value_count++;

            if (!sep) break;
            cursor = sep + 1;
        }

        e->current_index = 0;
        apply_override(e);
        options_count++;
    }

    close_overrides();
    options_dirty = true;
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
    const char *core_base = strrchr(core_path_arg, '/');
    core_base = core_base ? core_base + 1 : core_path_arg;
    snprintf(core_name, sizeof(core_name), "%s", core_base);

    char *ext = strstr(core_name, "_libretro.so");
    if (ext) *ext = '\0';
    snprintf(core_ini_path, sizeof(core_ini_path), "%s/%s.ini", RETRO_OPT_PATH, core_name);
    create_directories(core_ini_path, 1);

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;
    snprintf(content_ini_path, sizeof(content_ini_path), "%s/content/%s.ini", RETRO_OPT_PATH, content_base);
    create_directories(content_ini_path, 1);

    const char *content_dir = get_content_path((char *) content_path);
    const mz_ulong dir_crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char *) content_dir, strlen(content_dir));
    snprintf(
        directory_ini_path, sizeof(directory_ini_path), "%s/directory/%08lX.ini", RETRO_OPT_PATH,
        (unsigned long) dir_crc
    );
    create_directories(directory_ini_path, 1);
}

void options_capture_baseline(void) {
    snapshot_baseline();
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
