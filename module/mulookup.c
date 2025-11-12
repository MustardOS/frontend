#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>

#include "../common/common.h"
#include "../lookup/lookup.h"
#include "../common/json/json.h"

#define FOLDER_JSON  INFO_NAM_PATH "/%s.json"
#define GENERAL_JSON INFO_NAM_PATH "/general.json"

static struct json cached_root;

static char *cached_json_data = NULL;
static int cache_loaded = 0;

static const char *lookup_internal(const char *name) {
    const char *res = lookup(name);
    return res && *res ? res : NULL;
}

static const char *r_lookup_internal(const char *value) {
    const char *res = r_lookup(value);
    return res && *res ? res : NULL;
}

static int load_json_cache(char *path) {
    if (cache_loaded) return 1;

    if (!file_exist(path)) return 0;

    cached_json_data = read_all_char_from(path);
    if (!cached_json_data) return 0;

    if (!json_valid(cached_json_data)) {
        free(cached_json_data);
        cached_json_data = NULL;
        return 0;
    }

    cached_root = json_parse(cached_json_data);
    if (!json_exists(cached_root) || json_type(cached_root) != JSON_OBJECT) {
        free(cached_json_data);
        cached_json_data = NULL;
        return 0;
    }

    cache_loaded = 1;
    return 1;
}

static const char *lookup_json(char *path, const char *rom, char *buf) {
    if (!file_exist(path)) return NULL;

    char *json_data = read_all_char_from(path);
    if (!json_data) return NULL;

    if (!json_valid(json_data)) {
        free(json_data);
        return NULL;
    }

    struct json root = json_parse(json_data);
    if (!json_exists(root) || json_type(root) != JSON_OBJECT) {
        free(json_data);
        return NULL;
    }

    struct json val = json_object_get(root, rom);
    if (!json_exists(val)) {
        free(json_data);
        return NULL;
    }

    json_string_copy(val, buf, MAX_BUFFER_SIZE);
    free(json_data);

    return buf;
}

static const char *lookup_json_cached(const char *rom, char *buf) {
    if (!cache_loaded || !json_exists(cached_root)) return NULL;
    struct json val = json_object_get(cached_root, rom);

    if (!json_exists(val)) return NULL;
    json_string_copy(val, buf, MAX_BUFFER_SIZE);

    return buf;
}

static void process_lookup(const char *term, char *json_path, int reverse, int use_cache) {
    char json_buf[MAX_BUFFER_SIZE];
    const char *result = NULL;

    if (reverse) {
        result = r_lookup_internal(term);
        if (!result) {
            if (use_cache) result = lookup_json_cached(term, json_buf);
            else result = lookup_json(json_path, term, json_buf);
        }
    } else {
        result = lookup_internal(term);
        if (!result) {
            if (use_cache) result = lookup_json_cached(term, json_buf);
            else result = lookup_json(json_path, term, json_buf);
        }
    }

    if (!result) result = term;

    puts(result);
}

int main(int argc, char **argv) {
    int reverse = 0;
    int batch = 0;

    char *term = NULL;
    char *folder = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [--reverse|-r] [--folder|-f <folder>] [--batch|-b] <term>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--reverse") || !strcmp(argv[i], "-r")) {
            reverse = 1;
        } else if (!strcmp(argv[i], "--folder") || !strcmp(argv[i], "-f")) {
            if (i + 1 < argc) folder = argv[++i];
            else {
                fprintf(stderr, "Error: missing argument for --folder\n");
                return 1;
            }
        } else if (!strcmp(argv[i], "--batch") || !strcmp(argv[i], "-b")) {
            batch = 1;
        } else {
            term = argv[i];
        }
    }

    char json_path[PATH_MAX];
    if (folder) {
        snprintf(json_path, sizeof(json_path), FOLDER_JSON, folder);
        if (!file_exist(json_path)) snprintf(json_path, sizeof(json_path), "%s", GENERAL_JSON);
    } else {
        snprintf(json_path, sizeof(json_path), "%s", GENERAL_JSON);
    }

    if (batch) {
        load_json_cache(json_path);

        char line[MAX_BUFFER_SIZE];
        while (fgets(line, sizeof(line), stdin)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '\0') continue;

            process_lookup(line, json_path, reverse, 1);
        }

        if (cached_json_data) free(cached_json_data);
        return 0;
    }

    if (!term) {
        fprintf(stderr, "Error: missing search term\n");
        return 1;
    }

    process_lookup(term, json_path, reverse, 0);

    return 0;
}
