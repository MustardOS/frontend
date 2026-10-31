#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <json/json.h>
#include <common/base/options.h>
#include <common/base/strutil.h>
#include <common/config/config.h>
#include <common/content/core/coredb.h>
#include <common/platform/board.h>
#include <common/platform/device.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>

#if defined(__aarch64__)
#define COREDB_ARCH "aarch64"
#elif defined(__arm__)
#define COREDB_ARCH "arm32"
#elif defined(__x86_64__)
#define COREDB_ARCH "x64"
#elif defined(__i386__)
#define COREDB_ARCH "x86"
#else
#define COREDB_ARCH "unknown"
#endif

#define COREDB_NAMESPACE_MAX 64
#define COREDB_SYSTEM_MAX    256

#define COREDB_TAG_PICKLES   "mu-"
#define COREDB_TAG_RETROARCH "lr-"
#define COREDB_TAG_EXTERNAL  "ext-"

// TODO: Fix this up - I think it's the java core that has this
#define COREDB_SUFFIX_STANDALONE " - standalone"

#define COREDB_DIR "core"
#define COREDB_EXT ".json"

static const char *const runtime_group[core_runtime_count] = {"libretro", "libretro", "external"};

static const char *const runtime_prefix[core_runtime_count] = {
    COREDB_TAG_PICKLES, COREDB_TAG_RETROARCH, COREDB_TAG_EXTERNAL
};

static const char *const runtime_name[core_runtime_count] = {"pickles", "retroarch", "external"};

struct coredb_entry {
    char id[COREDB_NAME_MAX];
    char name_space[COREDB_NAME_MAX];
    char *raw;
    struct json node;
};

static struct coredb_entry *entries;
static int entry_count;
static int loaded;
static long long loaded_signature;

static time_t file_mtime(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 ? info.st_mtime : 0;
}

static char namespaces[COREDB_NAMESPACE_MAX][COREDB_NAME_MAX];
static int namespace_count;
static int namespace_extra = -1;

static struct coredb_system system_cache[COREDB_SYSTEM_MAX];
static char system_cache_key[COREDB_NAME_MAX];
static int system_cache_count = -1;
static int system_cache_extra = -1;

static int extra_shown(void) {
    return config.settings.advanced.extra_cores != 0;
}

static int marked_extra(const struct json entry) {
    const struct json value = json_object_get(entry, "extra");
    return json_exists(value) && json_int(value) != 0;
}

const char *coredb_runtime_label(const enum core_runtime runtime) {
    switch (runtime) {
        case core_runtime_pickles:
            return "Pickles";
        case core_runtime_retroarch:
            return "RetroArch";
        case core_runtime_external:
            return "External";
        default:
            return "";
    }
}

void coredb_assign_tag(const char *id, const enum core_runtime runtime, char *out, const size_t out_size) {
    switch (runtime) {
        case core_runtime_pickles:
            snprintf(out, out_size, COREDB_TAG_PICKLES "%s", id);
            break;
        case core_runtime_external:
            snprintf(out, out_size, COREDB_TAG_EXTERNAL "%s", id);
            break;
        default:
            snprintf(out, out_size, "%s", id);
            break;
    }
}

enum core_runtime coredb_assign_runtime(const char *stored, const char **id_out) {
    if (strncasecmp(stored, COREDB_TAG_PICKLES, strlen(COREDB_TAG_PICKLES)) == 0) {
        if (id_out) *id_out = stored + strlen(COREDB_TAG_PICKLES);
        return core_runtime_pickles;
    }

    if (strncasecmp(stored, COREDB_TAG_EXTERNAL, strlen(COREDB_TAG_EXTERNAL)) == 0) {
        if (id_out) *id_out = stored + strlen(COREDB_TAG_EXTERNAL);
        return core_runtime_external;
    }

    if (id_out) *id_out = stored;
    return core_runtime_retroarch;
}

static int strip_standalone(const char *id, char *out, const size_t out_size) {
    const size_t len = strlen(id);
    const size_t suffix = strlen(COREDB_SUFFIX_STANDALONE);

    if (len <= suffix || strcasecmp(id + len - suffix, COREDB_SUFFIX_STANDALONE) != 0) return 0;

    snprintf(out, out_size, "%.*s", (int) (len - suffix), id);
    return 1;
}

int coredb_assign_resolve(
    const char *system, const char *stored, char *id_out, const size_t id_size, enum core_runtime *runtime_out
) {
    if (!stored || !*stored) return 0;

    const char *id = stored;
    enum core_runtime runtime = coredb_assign_runtime(stored, &id);

    if (!coredb_core_find(system, runtime, id, NULL)) {
        runtime = runtime == core_runtime_external ? core_runtime_retroarch : core_runtime_external;

        if (!coredb_core_find(system, runtime, id, NULL)) {
            char base[COREDB_NAME_MAX];
            if (!strip_standalone(id, base, sizeof(base))
                || !coredb_core_find(system, core_runtime_external, base, NULL))
                return 0;

            snprintf(id_out, id_size, "%s", base);
            if (runtime_out) *runtime_out = core_runtime_external;
            return 1;
        }
    }

    snprintf(id_out, id_size, "%s", id);
    if (runtime_out) *runtime_out = runtime;
    return 1;
}

// Check 'em - since eventually we'll do x86 and maybe armhf once again
static const char *device_fact(const char *key) {
    if (strcmp(key, "device") == 0) return device.board.name;
    if (strcmp(key, "arch") == 0) return COREDB_ARCH;

    return NULL;
}

static int value_in_array(const struct json array, const char *wanted) {
    if (!wanted || !*wanted) return 0;

    for (struct json item = json_first(array); json_exists(item); item = json_next(item)) {
        char value[COREDB_NAME_MAX];
        json_string_copy(item, value, sizeof(value));
        if (strcasecmp(value, wanted) == 0) return 1;
    }

    return 0;
}

static int requirements_met(const struct json entry) {
    const struct json require = json_object_get(entry, "require");
    if (!json_exists(require)) return 1;

    for (struct json key = json_first(require); json_exists(key); key = json_next(json_next(key))) {
        char name[COREDB_NAME_MAX];
        json_string_copy(key, name, sizeof(name));

        const char *fact = device_fact(name);
        if (!fact) continue;

        if (!value_in_array(json_next(key), fact)) return 0;
    }

    return 1;
}

static int namespace_known(const char *name) {
    for (int i = 0; i < namespace_count; i++)
        if (strcmp(namespaces[i], name) == 0) return 1;

    return 0;
}

static int namespace_compare(const void *a, const void *b) {
    return strcasecmp((const char *) a, (const char *) b);
}

static int system_listed(const char *id, const struct json system) {
    if (!extra_shown() && marked_extra(system)) return 0;

    return coredb_core_count(id, core_runtime_pickles) > 0 || coredb_core_count(id, core_runtime_external) > 0;
}

static void collect_namespaces(void) {
    namespace_count = 0;
    namespace_extra = extra_shown();

    for (int e = 0; e < entry_count; e++) {
        const char *id = entries[e].id;
        const struct json system = entries[e].node;
        if (!system_listed(id, system)) continue;

        const char *name = entries[e].name_space;
        if (namespace_known(name) || namespace_count >= COREDB_NAMESPACE_MAX) continue;
        snprintf(namespaces[namespace_count++], COREDB_NAME_MAX, "%s", name);
    }

    qsort(namespaces, (size_t) namespace_count, COREDB_NAME_MAX, namespace_compare);
}

static char *read_manifest(const char *path) {
    char *data = read_all_char_from(path);
    if (!data || !json_valid(data) || json_type(json_parse(data)) != JSON_OBJECT) {
        free(data);
        return NULL;
    }

    return data;
}

static int entry_known(const char *id) {
    for (int e = 0; e < entry_count; e++)
        if (strcmp(entries[e].id, id) == 0) return 1;

    return 0;
}

static int definition_id(const char *file, char *out, const size_t out_size) {
    const size_t len = strlen(file);
    const size_t ext = strlen(COREDB_EXT);

    if (file[0] == '.' || len <= ext || strcmp(file + len - ext, COREDB_EXT) != 0) return 0;
    if (len - ext >= out_size) return 0;

    snprintf(out, out_size, "%.*s", (int) (len - ext), file);
    return 1;
}

static void load_namespace(const char *dir, const char *name_space) {
    DIR *handle = opendir(dir);
    if (!handle) return;

    const struct dirent *item;
    while ((item = readdir(handle))) {
        char id[COREDB_NAME_MAX];
        if (!definition_id(item->d_name, id, sizeof(id)) || entry_known(id)) continue;

        char path[COREDB_PATH_MAX * 3];
        snprintf(path, sizeof(path), "%s/%s", dir, item->d_name);

        char *data = read_manifest(path);
        if (!data) {
            LOG_WARN(mux_module, "coredb: skipping unreadable definition %s", path);
            continue;
        }

        struct coredb_entry *grown = realloc(entries, sizeof(*entries) * (size_t) (entry_count + 1));
        if (!grown) {
            free(data);
            break;
        }

        entries = grown;
        snprintf(entries[entry_count].id, sizeof(entries[entry_count].id), "%s", id);
        snprintf(entries[entry_count].name_space, sizeof(entries[entry_count].name_space), "%s", name_space);
        entries[entry_count].raw = data;
        entries[entry_count].node = json_parse(data);
        entry_count++;
    }

    closedir(handle);
}

static int namespace_dir(const char *dir, const struct dirent *item, char *path, const size_t path_size) {
    if (item->d_name[0] == '.' || strlen(item->d_name) >= COREDB_NAME_MAX) return 0;

    snprintf(path, path_size, "%s/%s", dir, item->d_name);

    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static void load_dir(const char *dir) {
    DIR *handle = opendir(dir);
    if (!handle) return;

    const struct dirent *item;
    while ((item = readdir(handle))) {
        char path[COREDB_PATH_MAX * 2];
        if (namespace_dir(dir, item, path, sizeof(path))) load_namespace(path, item->d_name);
    }

    closedir(handle);
}

static long long dir_signature(const char *dir, const int depth) {
    long long signature = (long long) file_mtime(dir);
    if (!depth) return signature;

    DIR *handle = opendir(dir);
    if (!handle) return signature;

    const struct dirent *item;
    while ((item = readdir(handle))) {
        if (item->d_name[0] == '.') continue;

        char path[COREDB_PATH_MAX * 3];
        snprintf(path, sizeof(path), "%s/%s", dir, item->d_name);
        signature = signature * 31 + dir_signature(path, depth - 1) + 1;
    }

    closedir(handle);
    return signature;
}

static long long source_signature(void) {
    return dir_signature(INFO_MNF_PATH "/" COREDB_DIR, 2) * 31 + dir_signature(STORE_LOC_MANIFEST "/" COREDB_DIR, 1);
}

static int entry_compare(const void *a, const void *b) {
    return strcmp(((const struct coredb_entry *) a)->id, ((const struct coredb_entry *) b)->id);
}

int coredb_load(void) {
    const long long signature = source_signature();
    if (loaded && signature == loaded_signature) return 1;
    if (loaded) coredb_free();

    load_dir(INFO_MNF_PATH "/" COREDB_DIR);
    load_dir(STORE_LOC_MANIFEST "/" COREDB_DIR);

    if (!entry_count) {
        LOG_WARN(mux_module, "coredb: no usable definitions at %s", STORE_LOC_MANIFEST "/" COREDB_DIR);
        return 0;
    }

    qsort(entries, (size_t) entry_count, sizeof(*entries), entry_compare);
    loaded_signature = signature;

    collect_namespaces();
    loaded = 1;
    return 1;
}

void coredb_free(void) {
    for (int e = 0; e < entry_count; e++)
        free(entries[e].raw);

    free(entries);
    entries = NULL;
    entry_count = 0;
    loaded_signature = 0;
    system_cache_count = -1;

    namespace_count = 0;
    loaded = 0;
}

static void refresh_namespaces(void) {
    if (loaded && namespace_extra != extra_shown()) collect_namespaces();
}

int coredb_namespace_count(void) {
    refresh_namespaces();
    return namespace_count;
}

const char *coredb_namespace_at(const int index) {
    refresh_namespaces();
    return index >= 0 && index < namespace_count ? namespaces[index] : "";
}

static int gather_systems(const char *name_space, struct coredb_system *out, const int limit) {
    int count = 0;

    for (int e = 0; e < entry_count && count < limit; e++) {
        const char *id = entries[e].id;
        const struct json system = entries[e].node;
        if (name_space && strcmp(entries[e].name_space, name_space) != 0) continue;
        if (!system_listed(id, system)) continue;

        snprintf(out[count].id, COREDB_NAME_MAX, "%s", id);
        json_string_copy(json_object_get(system, "catalogue"), out[count].name, COREDB_NAME_MAX);
        if (!out[count].name[0]) snprintf(out[count].name, COREDB_NAME_MAX, "%s", id);
        snprintf(out[count].name_space, COREDB_NAME_MAX, "%s", entries[e].name_space);
        count++;
    }

    return count;
}

static int system_compare(const void *a, const void *b) {
    return strcasecmp(((const struct coredb_system *) a)->name, ((const struct coredb_system *) b)->name);
}

static int systems_for(const char *name_space) {
    if (system_cache_count >= 0 && system_cache_extra == extra_shown()
        && strcmp(system_cache_key, name_space ? name_space : "") == 0)
        return system_cache_count;

    system_cache_extra = extra_shown();
    system_cache_count = gather_systems(name_space, system_cache, COREDB_SYSTEM_MAX);
    qsort(system_cache, (size_t) system_cache_count, sizeof(system_cache[0]), system_compare);
    snprintf(system_cache_key, sizeof(system_cache_key), "%s", name_space ? name_space : "");

    return system_cache_count;
}

int coredb_system_count(const char *name_space) {
    return systems_for(name_space);
}

int coredb_system_at(const char *name_space, const int index, struct coredb_system *out) {
    const int count = systems_for(name_space);
    if (index < 0 || index >= count || !out) return 0;

    *out = system_cache[index];
    return 1;
}

static const struct coredb_entry *system_entry(const char *system) {
    if (!system) return NULL;

    for (int e = 0; e < entry_count; e++)
        if (strcmp(entries[e].id, system) == 0) return &entries[e];

    return NULL;
}

static struct json system_node(const char *system) {
    const struct coredb_entry *entry = system_entry(system);
    return entry ? entry->node : (struct json) {0};
}

static struct json group_node(const char *system, const enum core_runtime runtime) {
    if (runtime >= core_runtime_count) return (struct json) {0};

    const struct json node = system_node(system);
    return json_exists(node) ? json_object_get(node, runtime_group[runtime]) : (struct json) {0};
}

static int system_field(const char *system, const char *key, char *out, const size_t out_size) {
    out[0] = '\0';

    const struct json node = system_node(system);
    if (!json_exists(node)) return 0;

    json_string_copy(json_object_get(node, key), out, out_size);
    return out[0] != '\0';
}

int coredb_system_namespace(const char *system, char *out, const size_t out_size) {
    const struct coredb_entry *entry = system_entry(system);
    if (!entry) return 0;

    snprintf(out, out_size, "%s", entry->name_space);
    return 1;
}

enum core_runtime coredb_system_runtime(const char *system) {
    char value[COREDB_NAME_MAX];
    if (system_field(system, "runtime", value, sizeof(value))) {
        for (int r = 0; r < core_runtime_count; r++)
            if (strcasecmp(value, runtime_name[r]) == 0) return (enum core_runtime) r;
    }

    return core_runtime_pickles;
}

static int runtime_in_order(const enum core_runtime preferred, const int step) {
    if (step == 0) return preferred;

    const int index = step - 1;
    return index < (int) preferred ? index : index + 1;
}

int coredb_system_default_core(const char *system, struct coredb_core *out) {
    const enum core_runtime preferred = coredb_system_runtime(system);

    char id[COREDB_NAME_MAX];
    if (system_field(system, "default", id, sizeof(id))) {
        for (int step = 0; step < core_runtime_count; step++) {
            if (coredb_core_find(system, (enum core_runtime) runtime_in_order(preferred, step), id, out)) return 1;
        }
    }

    for (int step = 0; step < core_runtime_count; step++) {
        if (coredb_core_at(system, (enum core_runtime) runtime_in_order(preferred, step), 0, out)) return 1;
    }

    return 0;
}

int coredb_system_default(const char *system, char *out, const size_t out_size) {
    struct coredb_core core;
    if (coredb_system_default_core(system, &core)) {
        snprintf(out, out_size, "%s", core.id);
        return 1;
    }

    return system_field(system, "default", out, out_size);
}

int coredb_system_catalogue(const char *system, char *out, const size_t out_size) {
    if (system_field(system, "catalogue", out, out_size)) return 1;

    snprintf(out, out_size, "%s", system);
    return 0;
}

int coredb_system_lookup(const char *system) {
    const struct json node = system_node(system);
    if (!json_exists(node)) return 0;

    const struct json value = json_object_get(node, "lookup");
    return json_exists(value) ? json_int(value) : 0;
}

int coredb_system_governor(const char *system, char *out, const size_t out_size) {
    return system_field(system, "governor", out, out_size);
}

int coredb_system_control(const char *system, char *out, const size_t out_size) {
    return system_field(system, "control", out, out_size);
}

static void
fill_core(const struct json key, const struct json entry, const enum core_runtime runtime, struct coredb_core *out) {
    memset(out, 0, sizeof(*out));
    out->runtime = runtime;

    json_string_copy(key, out->id, sizeof(out->id));
    json_string_copy(json_object_get(entry, "name"), out->name, sizeof(out->name));
    if (!out->name[0]) snprintf(out->name, sizeof(out->name), "%s", out->id);
    json_string_copy(json_object_get(entry, "core"), out->core, sizeof(out->core));
    json_string_copy(json_object_get(entry, "launcher"), out->launcher, sizeof(out->launcher));
    json_string_copy(json_object_get(entry, "governor"), out->governor, sizeof(out->governor));
    json_string_copy(json_object_get(entry, "control"), out->control, sizeof(out->control));

    if (!out->launcher[0] && runtime != core_runtime_external)
        snprintf(out->launcher, sizeof(out->launcher), "general.sh");

    if (out->launcher[0]) {
        char stem[COREDB_NAME_MAX];
        snprintf(stem, sizeof(stem), "%s", out->launcher);
        snprintf(out->launcher, sizeof(out->launcher), "%s%s", runtime_prefix[runtime], stem);
    }

    const struct json bios = json_object_get(entry, "bios_required");
    out->bios_required = json_exists(bios) ? json_int(bios) : 0;
}

static int walk_cores(
    const char *system, const enum core_runtime runtime, const int wanted, const char *wanted_id,
    struct coredb_core *out
) {
    const struct json cores = group_node(system, runtime);
    if (!json_exists(cores)) return 0;

    int index = 0;
    for (struct json key = json_first(cores); json_exists(key); key = json_next(json_next(key))) {
        const struct json entry = json_next(key);
        if (!requirements_met(entry)) continue;
        if (!wanted_id && !extra_shown() && marked_extra(entry)) continue;

        if (wanted_id) {
            char id[COREDB_NAME_MAX];
            json_string_copy(key, id, sizeof(id));
            if (strcmp(id, wanted_id) != 0) continue;

            if (out) fill_core(key, entry, runtime, out);
            return 1;
        }

        if (wanted >= 0 && index == wanted) {
            if (out) fill_core(key, entry, runtime, out);
            return 1;
        }

        index++;
    }

    return wanted < 0 && !wanted_id ? index : 0;
}

int coredb_core_file_hidden(const char *core_file) {
    if (extra_shown() || !core_file || !*core_file || !coredb_load()) return 0;

    int referenced = 0;
    for (int e = 0; e < entry_count; e++) {
        const struct json system = entries[e].node;
        const struct json cores = json_object_get(system, "libretro");
        if (!json_exists(cores)) continue;

        const int system_extra = marked_extra(system);
        for (struct json core = json_first(cores); json_exists(core); core = json_next(json_next(core))) {
            const struct json entry = json_next(core);

            char file[COREDB_NAME_MAX];
            json_string_copy(json_object_get(entry, "core"), file, sizeof(file));
            if (strcmp(file, core_file) != 0) continue;

            referenced = 1;
            if (!system_extra && !marked_extra(entry)) return 0;
        }
    }

    return referenced;
}

int coredb_core_count(const char *system, const enum core_runtime runtime) {
    return walk_cores(system, runtime, -1, NULL, NULL);
}

int coredb_core_at(const char *system, const enum core_runtime runtime, const int index, struct coredb_core *out) {
    return walk_cores(system, runtime, index, NULL, out);
}

int coredb_core_find(const char *system, const enum core_runtime runtime, const char *id, struct coredb_core *out) {
    return walk_cores(system, runtime, -1, id, out);
}

int coredb_core_find_file(const char *core_file, const enum core_runtime runtime, struct coredb_core *out) {
    if (!core_file || !*core_file || !coredb_load()) return 0;

    for (int n = 0; n < coredb_namespace_count(); n++) {
        const char *name_space = coredb_namespace_at(n);

        for (int s = 0; s < coredb_system_count(name_space); s++) {
            struct coredb_system system;
            if (!coredb_system_at(name_space, s, &system)) continue;

            for (int c = 0; c < coredb_core_count(system.id, runtime); c++) {
                struct coredb_core core;
                if (!coredb_core_at(system.id, runtime, c, &core)) continue;
                if (strcmp(core.core, core_file) != 0) continue;

                if (out) *out = core;
                return 1;
            }
        }
    }

    return 0;
}

int coredb_runtime_available(const char *system, const enum core_runtime runtime) {
    return coredb_core_count(system, runtime) > 0;
}

const char *coredb_board_governor(const struct coredb_core *core) {
    // The GKD Pixel 2 starts Pickles and Wasabi at full speed. Users can
    // still pick another governor per content afterwards.
    if (board_is(board_special_pixel_2)
        && (core->runtime == core_runtime_pickles || strcmp(core->core, "ext-video") == 0))
        return "performance";

    return NULL;
}
