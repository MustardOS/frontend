#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <json/json.h>
#include <common/base/options.h>
#include <common/base/strutil.h>
#include <common/content/core/coredb.h>
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

enum coredb_file { coredb_file_libretro = 0, coredb_file_external, coredb_file_count };

static const char *const db_file[coredb_file_count] = {"libretro", "external"};

static const enum coredb_file runtime_source[core_runtime_count] = {
    coredb_file_libretro, coredb_file_libretro, coredb_file_external
};

static const char *const runtime_prefix[core_runtime_count] = {
    COREDB_TAG_PICKLES, COREDB_TAG_RETROARCH, COREDB_TAG_EXTERNAL
};

static char *raw[coredb_file_count];
static struct json root[coredb_file_count];
static int loaded;

static char namespaces[COREDB_NAMESPACE_MAX][COREDB_NAME_MAX];
static int namespace_count;

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

static void collect_namespaces(void) {
    namespace_count = 0;

    for (int f = 0; f < coredb_file_count; f++) {
        if (!json_exists(root[f])) continue;

        for (struct json key = json_first(root[f]); json_exists(key); key = json_next(json_next(key))) {
            const struct json system = json_next(key);
            char name[COREDB_NAME_MAX];
            json_string_copy(json_object_get(system, "namespace"), name, sizeof(name));
            if (!name[0]) snprintf(name, sizeof(name), "%s", "Other");

            if (namespace_known(name) || namespace_count >= COREDB_NAMESPACE_MAX) continue;
            snprintf(namespaces[namespace_count++], COREDB_NAME_MAX, "%s", name);
        }
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

int coredb_load(void) {
    if (loaded) return 1;

    int found = 0;
    for (int f = 0; f < coredb_file_count; f++) {
        char path[COREDB_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s.json", INFO_CORE_PATH, db_file[f]);

        raw[f] = read_manifest(path);
        if (!raw[f]) {
            snprintf(path, sizeof(path), "%s/%s.json", STORE_LOC_CORE, db_file[f]);
            raw[f] = read_manifest(path);
            if (!raw[f]) {
                LOG_WARN(mux_module, "coredb: no usable definitions at %s", path);
                continue;
            }
        }

        root[f] = json_parse(raw[f]);
        found++;
    }

    if (!found) return 0;

    collect_namespaces();
    loaded = 1;
    return 1;
}

void coredb_free(void) {
    for (int f = 0; f < coredb_file_count; f++) {
        free(raw[f]);
        raw[f] = NULL;
        root[f] = (struct json) {0};
    }

    namespace_count = 0;
    loaded = 0;
}

int coredb_namespace_count(void) {
    return namespace_count;
}

const char *coredb_namespace_at(const int index) {
    return index >= 0 && index < namespace_count ? namespaces[index] : "";
}

static int system_in_namespace(const struct json system, const char *name_space) {
    char value[COREDB_NAME_MAX];
    json_string_copy(json_object_get(system, "namespace"), value, sizeof(value));
    if (!value[0]) snprintf(value, sizeof(value), "%s", "Other");

    return strcmp(value, name_space) == 0;
}

static int gather_systems(const char *name_space, struct coredb_system *out, const int limit) {
    int count = 0;

    for (int f = 0; f < coredb_file_count; f++) {
        if (!json_exists(root[f])) continue;

        for (struct json key = json_first(root[f]); json_exists(key); key = json_next(json_next(key))) {
            const struct json system = json_next(key);

            char id[COREDB_NAME_MAX];
            json_string_copy(key, id, sizeof(id));
            if (name_space && !system_in_namespace(system, name_space)) continue;

            int seen = 0;
            for (int i = 0; i < count; i++)
                if (strcmp(out[i].id, id) == 0) seen = 1;
            if (seen || count >= limit) continue;

            if (coredb_core_count(id, core_runtime_pickles) == 0 && coredb_core_count(id, core_runtime_external) == 0) {
                continue;
            }

            snprintf(out[count].id, COREDB_NAME_MAX, "%s", id);
            json_string_copy(json_object_get(system, "name"), out[count].name, COREDB_NAME_MAX);
            if (!out[count].name[0]) snprintf(out[count].name, COREDB_NAME_MAX, "%s", id);
            json_string_copy(json_object_get(system, "namespace"), out[count].name_space, COREDB_NAME_MAX);
            count++;
        }
    }

    return count;
}

static int system_compare(const void *a, const void *b) {
    return strcasecmp(((const struct coredb_system *) a)->name, ((const struct coredb_system *) b)->name);
}

static struct coredb_system system_cache[COREDB_SYSTEM_MAX];
static char system_cache_key[COREDB_NAME_MAX];
static int system_cache_count = -1;

static int systems_for(const char *name_space) {
    if (system_cache_count >= 0 && strcmp(system_cache_key, name_space ? name_space : "") == 0)
        return system_cache_count;

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

static struct json system_node(const char *system, const enum core_runtime runtime) {
    if (runtime >= core_runtime_count) return (struct json) {0};

    const enum coredb_file file = runtime_source[runtime];
    if (!json_exists(root[file])) return (struct json) {0};

    return json_object_get(root[file], system);
}

static struct json system_node_in(const enum coredb_file file, const char *system) {
    return json_exists(root[file]) ? json_object_get(root[file], system) : (struct json) {0};
}

static int system_field(const char *system, const char *key, char *out, const size_t out_size) {
    for (int f = 0; f < coredb_file_count; f++) {
        const struct json node = system_node_in((enum coredb_file) f, system);
        if (!json_exists(node)) continue;

        json_string_copy(json_object_get(node, key), out, out_size);
        if (out[0]) return 1;
    }

    out[0] = '\0';
    return 0;
}

int coredb_system_namespace(const char *system, char *out, const size_t out_size) {
    for (int f = 0; f < coredb_file_count; f++) {
        const struct json node = system_node_in((enum coredb_file) f, system);
        if (!json_exists(node)) continue;

        json_string_copy(json_object_get(node, "namespace"), out, out_size);
        if (!out[0]) snprintf(out, out_size, "%s", "Other");
        return 1;
    }

    return 0;
}

int coredb_system_default(const char *system, char *out, const size_t out_size) {
    return system_field(system, "default", out, out_size);
}

int coredb_system_catalogue(const char *system, char *out, const size_t out_size) {
    if (system_field(system, "catalogue", out, out_size)) return 1;

    snprintf(out, out_size, "%s", system);
    return 0;
}

int coredb_system_lookup(const char *system) {
    for (int f = 0; f < coredb_file_count; f++) {
        const struct json node = system_node_in((enum coredb_file) f, system);
        if (!json_exists(node)) continue;

        const struct json value = json_object_get(node, "lookup");
        if (json_exists(value)) return json_int(value);
    }

    return 0;
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
    const struct json node = system_node(system, runtime);
    if (!json_exists(node)) return wanted_id ? 0 : 0;

    const struct json cores = json_object_get(node, "cores");
    if (!json_exists(cores)) return 0;

    int index = 0;
    for (struct json key = json_first(cores); json_exists(key); key = json_next(json_next(key))) {
        const struct json entry = json_next(key);
        if (!requirements_met(entry)) continue;

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

int coredb_core_count(const char *system, const enum core_runtime runtime) {
    return walk_cores(system, runtime, -1, NULL, NULL);
}

int coredb_core_at(const char *system, const enum core_runtime runtime, const int index, struct coredb_core *out) {
    return walk_cores(system, runtime, index, NULL, out);
}

int coredb_core_find(const char *system, const enum core_runtime runtime, const char *id, struct coredb_core *out) {
    return walk_cores(system, runtime, -1, id, out);
}

int coredb_runtime_available(const char *system, const enum core_runtime runtime) {
    return coredb_core_count(system, runtime) > 0;
}
