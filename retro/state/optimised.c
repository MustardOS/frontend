#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>
#include <json/json.h>
#include "content_hash.h"
#include "optimised.h"

#define OPTIMISED_USER  INFO_MNF_PATH "/optimised.json"
#define OPTIMISED_SHARE STORE_LOC_MANIFEST "/optimised.json"

#define OPTIMISED_MAX       64
#define OPTIMISED_KEY_LEN   128
#define OPTIMISED_VALUE_LEN 128
#define OPTIMISED_HASH_LEN  40
#define OPTIMISED_CRC_LIMIT (64LL * 1024 * 1024)

struct optimised_option_entry {
    char key[OPTIMISED_KEY_LEN];
    char value[OPTIMISED_VALUE_LEN];
    int applied;
};

struct optimised_setting_entry {
    char key[OPTIMISED_KEY_LEN];
    int value;
    int applied;
};

static int active = 0;
static int malformed = 0;
static char profile_name[MAX_BUFFER_SIZE] = "";

static struct optimised_option_entry options[OPTIMISED_MAX];
static int option_count = 0;

static struct optimised_setting_entry settings[OPTIMISED_MAX];
static int setting_count = 0;

static void reset(void) {
    active = 0;
    malformed = 0;
    profile_name[0] = '\0';
    option_count = 0;
    setting_count = 0;
}

static int is_archive(const char *path) {
    static const char *archive_exts[] = {".zip", ".7z", ".gz", ".tar", ".rar"};

    const char *ext = strrchr(path, '.');
    if (!ext) return 0;

    for (size_t i = 0; i < sizeof(archive_exts) / sizeof(archive_exts[0]); i++)
        if (strcasecmp(ext, archive_exts[i]) == 0) return 1;

    return 0;
}

static void lower(char *text) {
    for (char *p = text; *p; p++)
        *p = (char) tolower((unsigned char) *p);
}

static char *load_bundle(const char *path, int64_t *generated) {
    *generated = -1;
    if (!file_exist(path)) return NULL;

    char *data = read_all_char_from(path);
    if (!data) return NULL;

    if (!json_valid(data)) {
        LOG_WARN(mux_module, "Ignoring invalid optimised profiles: %s", path);
        free(data);
        return NULL;
    }

    const struct json root = json_parse(data);
    if (json_type(root) != JSON_OBJECT || json_type(json_object_get(root, "content")) != JSON_OBJECT) {
        LOG_WARN(mux_module, "Ignoring optimised profiles without content: %s", path);
        free(data);
        return NULL;
    }

    const struct json stamp = json_object_get(root, "generated");
    *generated = json_type(stamp) == JSON_NUMBER ? json_int64(stamp) : 0;
    return data;
}

static char *newest_bundle(void) {
    int64_t user_generated;
    int64_t share_generated;

    char *user = load_bundle(OPTIMISED_USER, &user_generated);
    char *share = load_bundle(OPTIMISED_SHARE, &share_generated);

    if (user && share) {
        if (user_generated >= share_generated) {
            free(share);
            return user;
        }

        free(user);
        return share;
    }

    return user ? user : share;
}

static void copy_options(const struct json object) {
    if (!json_exists(object)) return;
    if (json_type(object) != JSON_OBJECT) {
        malformed = 1;
        return;
    }

    for (struct json key = json_first(object); json_exists(key); key = json_next(json_next(key))) {
        const struct json value = json_next(key);
        if (json_type(value) != JSON_STRING) {
            malformed = 1;
            continue;
        }

        char name[OPTIMISED_KEY_LEN];
        json_string_copy(key, name, sizeof(name));

        int slot = -1;
        for (int i = 0; i < option_count; i++)
            if (strcmp(options[i].key, name) == 0) slot = i;

        if (slot < 0) {
            if (option_count >= OPTIMISED_MAX) {
                malformed = 1;
                continue;
            }
            slot = option_count++;
            snprintf(options[slot].key, sizeof(options[slot].key), "%s", name);
        }

        json_string_copy(value, options[slot].value, sizeof(options[slot].value));
        options[slot].applied = 0;
    }
}

static void copy_settings(const struct json object) {
    if (!json_exists(object)) return;
    if (json_type(object) != JSON_OBJECT) {
        malformed = 1;
        return;
    }

    for (struct json key = json_first(object); json_exists(key); key = json_next(json_next(key))) {
        const struct json value = json_next(key);
        if (json_type(value) != JSON_NUMBER) {
            malformed = 1;
            continue;
        }

        char name[OPTIMISED_KEY_LEN];
        json_string_copy(key, name, sizeof(name));

        int slot = -1;
        for (int i = 0; i < setting_count; i++)
            if (strcmp(settings[i].key, name) == 0) slot = i;

        if (slot < 0) {
            if (setting_count >= OPTIMISED_MAX) {
                malformed = 1;
                continue;
            }
            slot = setting_count++;
            snprintf(settings[slot].key, sizeof(settings[slot].key), "%s", name);
        }

        settings[slot].value = json_int(value);
        settings[slot].applied = 0;
    }
}

static int content_hashes(const char *content_path, char cheevo[OPTIMISED_HASH_LEN], char crc[OPTIMISED_HASH_LEN]) {
    cheevo[0] = '\0';
    crc[0] = '\0';

    if (is_archive(content_path)) {
        content_hash_archive_cached(content_hash_cheevo, content_path, cheevo, OPTIMISED_HASH_LEN);
        content_hash_archive_cached(content_hash_content, content_path, crc, OPTIMISED_HASH_LEN);
    } else if (!content_hash_resolve(content_hash_cheevo, content_path, content_path, -1, cheevo, OPTIMISED_HASH_LEN)) {
        content_hash_resolve(
            content_hash_content, content_path, content_path, OPTIMISED_CRC_LIMIT, crc, OPTIMISED_HASH_LEN
        );
    }

    lower(cheevo);
    lower(crc);
    return cheevo[0] || crc[0];
}

int optimised_init(const char *core_name, const char *content_path) {
    reset();
    if (!config.settings.advanced.optimised || !core_name || !content_path || !content_path[0]) return 0;

    char *data = newest_bundle();
    if (!data) return 0;

    char cheevo[OPTIMISED_HASH_LEN];
    char crc[OPTIMISED_HASH_LEN];
    if (!content_hashes(content_path, cheevo, crc)) {
        LOG_INFO(mux_module, "Optimised profiles: no content hash available yet");
        free(data);
        return 0;
    }

    const struct json content = json_object_get(json_parse(data), "content");
    struct json entry = cheevo[0] ? json_object_get(content, cheevo) : (struct json) {0};
    if (json_type(entry) != JSON_OBJECT && crc[0]) entry = json_object_get(content, crc);

    if (!json_exists(entry)) {
        free(data);
        return 0;
    }

    const struct json name = json_object_get(entry, "name");
    if (json_type(name) == JSON_STRING) json_string_copy(name, profile_name, sizeof(profile_name));

    if (json_type(entry) != JSON_OBJECT) {
        malformed = 1;
    } else {
        const struct json cores = json_object_get(entry, "cores");
        const struct json core = json_object_get(cores, core_name);

        if (json_exists(cores) && json_type(cores) != JSON_OBJECT) malformed = 1;
        if (json_exists(core) && json_type(core) != JSON_OBJECT) malformed = 1;

        copy_settings(json_object_get(entry, "settings"));
        if (json_type(core) == JSON_OBJECT) {
            copy_settings(json_object_get(core, "settings"));
            copy_options(json_object_get(core, "options"));
        }
    }

    active = option_count > 0 || setting_count > 0;
    if (malformed) {
        LOG_WARN(mux_module, "Optimised profile for %s is malformed", cheevo[0] ? cheevo : crc);
    } else if (active) {
        LOG_INFO(
            mux_module, "Optimised profile '%s' for %s: %d core options, %d settings", profile_name,
            cheevo[0] ? cheevo : crc, option_count, setting_count
        );
    }

    free(data);
    return active;
}

int optimised_active(void) {
    return active;
}

enum optimised_state optimised_status(void) {
    if (malformed) return optimised_state_invalid;
    if (!active) return optimised_state_none;

    for (int i = 0; i < option_count; i++)
        if (!options[i].applied) return optimised_state_invalid;

    for (int i = 0; i < setting_count; i++)
        if (!settings[i].applied) return optimised_state_invalid;

    return optimised_state_applied;
}

void optimised_option_result(const char *key, const int applied) {
    if (!active || !key) return;

    for (int i = 0; i < option_count; i++) {
        if (strcmp(options[i].key, key) != 0) continue;

        options[i].applied = applied;
        if (!applied) LOG_WARN(mux_module, "Optimised core option '%s' has a value the core does not offer", key);
        return;
    }
}

void optimised_setting_result(const char *key, const int applied) {
    if (!active || !key) return;

    for (int i = 0; i < setting_count; i++) {
        if (strcmp(settings[i].key, key) != 0) continue;

        settings[i].applied = applied;
        return;
    }
}

const char *optimised_name(void) {
    return profile_name;
}

const char *optimised_option(const char *key) {
    if (!active || !key) return NULL;

    for (int i = 0; i < option_count; i++)
        if (strcmp(options[i].key, key) == 0) return options[i].value;

    return NULL;
}

int optimised_setting(const char *key, int *value) {
    if (!active || !key || !value) return 0;

    for (int i = 0; i < setting_count; i++) {
        if (strcmp(settings[i].key, key) != 0) continue;

        *value = settings[i].value;
        return 1;
    }

    return 0;
}
