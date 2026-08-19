#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/language.h"
#include "../macro/macro.h"
#include "deck.h"

struct deck_entry deck_list[DECK_MAX];
int deck_count = 0;

static char base_dir[DECK_PATH_MAX] = "";
static int deck_lookup[DECK_MAX];
static int deck_lookup_count = 0;

#define DECK_GROUP "deck"

static void format_epoch(const long long epoch, char *buf, const size_t size) {
    const time_t stamp = epoch;
    const struct tm *broken = localtime(&stamp);
    if (!broken) {
        snprintf(buf, size, "%s", lang.generic.unknown);
        return;
    }
    strftime(buf, size, "%Y-%m-%d %H:%M", broken);
}

static void sanitize_filename(const char *name, char *out, const size_t out_len) {
    size_t written = 0;
    for (size_t i = 0; name[i] && written + 1 < out_len; i++) {
        const unsigned char letter = (unsigned char) name[i];
        out[written++] = (char) (isalnum(letter) || letter == ' ' || letter == '-' || letter == '_' ? letter : '_');
    }
    out[written] = '\0';

    while (written > 0 && (out[written - 1] == ' ' || out[written - 1] == '.'))
        out[--written] = '\0';

    size_t start = 0;
    while (out[start] == ' ' || out[start] == '.')
        start++;
    if (start > 0) memmove(out, out + start, written - start + 1);

    if (out[0] == '\0') snprintf(out, out_len, "deck");
}

static void assign_unique_path(struct deck_entry *entry) {
    char safe[DECK_NAME_MAX];
    sanitize_filename(entry->name, safe, sizeof(safe));

    snprintf(entry->path, sizeof(entry->path), "%s/%s.ini", base_dir, safe);
    if (!file_exist(entry->path)) return;

    for (int attempt = 2; attempt < 1000; attempt++) {
        snprintf(entry->path, sizeof(entry->path), "%s/%s (%d).ini", base_dir, safe, attempt);
        if (!file_exist(entry->path)) return;
    }
}

static void write_manifest_entry(const struct deck_entry *entry) {
    mini_t *ini = mini_create(entry->path);
    if (!ini) return;

    mini_set_string(ini, DECK_GROUP, "name", entry->name);
    mini_set_int(ini, DECK_GROUP, "created", entry->created);
    mini_set_int(ini, DECK_GROUP, "index", entry->index);
    mini_set_int(ini, DECK_GROUP, "route", entry->route);
    mini_set_int(ini, DECK_GROUP, "priority", entry->priority);

    for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
        char key[24];

        snprintf(key, sizeof(key), "src_%d", source);
        mini_set_int(ini, "map", key, entry->source_target[source]);

        snprintf(key, sizeof(key), "srcms_%d", source);
        mini_set_int(ini, "map", key, entry->source_turbo[source]);

        snprintf(key, sizeof(key), "macroname_%d", source);
        mini_set_string(ini, "map", key, entry->source_macro_name[source]);
    }

    mini_save(ini, 0);
    mini_free(ini);
}

static int next_free_index(void) {
    int highest = -1;
    for (int i = 0; i < deck_count; i++) {
        if (deck_list[i].index > highest) highest = deck_list[i].index;
    }
    return highest + 1;
}

static void decks_rebuild_lookup(void) {
    deck_lookup_count = deck_count;

    for (int i = 0; i < deck_lookup_count; i++) {
        int at = i;
        while (at > 0 && deck_list[deck_lookup[at - 1]].index > deck_list[i].index) {
            deck_lookup[at] = deck_lookup[at - 1];
            at--;
        }
        deck_lookup[at] = i;
    }
}

int decks_position_from_index(const int index) {
    int low = 0;
    int high = deck_lookup_count - 1;

    while (low <= high) {
        const int middle = low + (high - low) / 2;
        const int position = deck_lookup[middle];
        const int found = deck_list[position].index;

        if (found == index) return position;
        if (found < index) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }

    return -1;
}

static int deck_cmp(const void *left, const void *right) {
    return strcasecmp(((const struct deck_entry *) left)->name, ((const struct deck_entry *) right)->name);
}

static void reset_map(struct deck_entry *entry) {
    for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
        entry->source_target[source] = session_settings_default_source_target(source);
        entry->source_turbo[source] = 0;
        entry->source_macro[source] = -1;
        entry->source_macro_name[source][0] = '\0';
    }
}

void decks_resolve_macros(void) {
    for (int i = 0; i < deck_count; i++) {
        for (int source = 0; source < PORT_SOURCE_COUNT; source++)
            deck_list[i].source_macro[source] = macros_index_by_name(deck_list[i].source_macro_name[source]);
    }
}

void decks_set_macro_name(const int position, const int source, const int macro_index) {
    if (position < 0 || position >= deck_count) return;
    if (source < 0 || source >= PORT_SOURCE_COUNT) return;

    const struct macro_entry *macro = macro_index >= 0 ? macros_get_by_index(macro_index) : NULL;
    snprintf(deck_list[position].source_macro_name[source], MACRO_NAME_MAX, "%s", macro ? macro->name : "");
}

void decks_init(const char *deck_dir) {
    snprintf(base_dir, sizeof(base_dir), "%s", deck_dir);
    create_directories(base_dir, 0);

    deck_count = 0;
    deck_lookup_count = 0;

    DIR *dir = opendir(base_dir);
    if (!dir) return;

    struct dirent *ent;
    while (((ent = readdir(dir))) && deck_count < DECK_MAX) {
        if (ent->d_type != DT_REG && ent->d_type != DT_UNKNOWN) continue;

        const char *ext = strrchr(ent->d_name, '.');
        if (!ext || strcasecmp(ext, ".ini") != 0) continue;

        char file_path[DECK_PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", base_dir, ent->d_name);

        mini_t *ini = mini_try_load(file_path);
        if (!ini) continue;

        struct deck_entry *entry = &deck_list[deck_count];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->path, sizeof(entry->path), "%s", file_path);
        snprintf(entry->name, sizeof(entry->name), "%s", get_ini_string(ini, DECK_GROUP, "name", lang.generic.unknown));
        entry->created = mini_get_int(ini, DECK_GROUP, "created", 0);
        entry->index = (int) mini_get_int(ini, DECK_GROUP, "index", -1);

        const int route = (int) mini_get_int(ini, DECK_GROUP, "route", 0);
        entry->route = route < 0 || route >= MUX_INPUT_PORT_COUNT ? 0 : route;
        entry->priority = mini_get_int(ini, DECK_GROUP, "priority", 0) ? 1 : 0;

        for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
            char key[24];

            snprintf(key, sizeof(key), "src_%d", source);
            const int target = (int) mini_get_int(ini, "map", key, session_settings_default_source_target(source));
            entry->source_target[source] = target < -1 || target >= PORT_TARGET_COUNT ? -1 : target;

            snprintf(key, sizeof(key), "srcms_%d", source);
            const int turbo = (int) mini_get_int(ini, "map", key, 0);
            entry->source_turbo[source] = turbo < 0 || turbo > 65535 ? 0 : turbo;

            snprintf(key, sizeof(key), "macroname_%d", source);
            snprintf(entry->source_macro_name[source], MACRO_NAME_MAX, "%s", get_ini_string(ini, "map", key, ""));

            if (!entry->source_macro_name[source][0]) {
                snprintf(key, sizeof(key), "macro_%d", source);
                const int legacy = (int) mini_get_int(ini, "map", key, -1);
                const struct macro_entry *named = legacy >= 0 ? macros_get_by_index(legacy) : NULL;

                if (named) snprintf(entry->source_macro_name[source], MACRO_NAME_MAX, "%s", named->name);
            }

            entry->source_macro[source] = -1;
        }

        mini_free(ini);
        deck_count++;
    }

    closedir(dir);

    for (int i = 0; i < deck_count; i++) {
        if (deck_list[i].index < 0) {
            deck_list[i].index = next_free_index();
            write_manifest_entry(&deck_list[i]);
        }
    }

    qsort(deck_list, (size_t) deck_count, sizeof(struct deck_entry), deck_cmp);
    decks_rebuild_lookup();
    decks_resolve_macros();
}

int decks_create(const char *name) {
    if (!base_dir[0] || deck_count >= DECK_MAX) return -1;

    const int position = deck_count;
    struct deck_entry *entry = &deck_list[position];

    memset(entry, 0, sizeof(*entry));
    entry->index = next_free_index();
    entry->created = (long long) time(NULL);
    entry->route = 0;
    entry->priority = 0;
    reset_map(entry);

    if (name && *name) {
        snprintf(entry->name, sizeof(entry->name), "%s", name);
    } else {
        format_epoch(entry->created, entry->name, sizeof(entry->name));
    }

    assign_unique_path(entry);
    write_manifest_entry(entry);

    deck_count++;
    decks_rebuild_lookup();
    return position;
}

int decks_rename(const int position, const char *new_name) {
    if (position < 0 || position >= deck_count || !new_name || !*new_name) return -1;

    struct deck_entry *entry = &deck_list[position];
    char old_path[DECK_PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s", entry->path);

    snprintf(entry->name, sizeof(entry->name), "%s", new_name);
    assign_unique_path(entry);

    if (strcmp(old_path, entry->path) != 0) remove(old_path);
    write_manifest_entry(entry);

    return 0;
}

int decks_delete(const int position) {
    if (position < 0 || position >= deck_count) return -1;

    remove(deck_list[position].path);

    for (int i = position; i < deck_count - 1; i++) {
        deck_list[i] = deck_list[i + 1];
    }
    deck_count--;
    decks_rebuild_lookup();

    return 0;
}

int decks_save(const int position) {
    if (position < 0 || position >= deck_count) return -1;

    write_manifest_entry(&deck_list[position]);
    deck_list[position].dirty = 0;

    return 0;
}

void decks_mark_dirty(const int position) {
    if (position < 0 || position >= deck_count) return;
    deck_list[position].dirty = 1;
}

void decks_flush(void) {
    for (int i = 0; i < deck_count; i++) {
        if (deck_list[i].dirty) decks_save(i);
    }
}

void decks_clear_macro_references(const int macro_index) {
    for (int i = 0; i < deck_count; i++) {
        int touched = 0;

        for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
            if (deck_list[i].source_macro[source] != macro_index) continue;

            deck_list[i].source_macro[source] = -1;
            deck_list[i].source_macro_name[source][0] = '\0';
            touched = 1;
        }

        if (touched) write_manifest_entry(&deck_list[i]);
    }
}
