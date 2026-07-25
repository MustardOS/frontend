#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/language.h"
#include "macro.h"

struct macro_entry macro_list[MACRO_MAX];
int macro_count = 0;

static char base_dir[MACRO_PATH_MAX] = "";

#define MACRO_GROUP "macro"

static void format_epoch(const long long epoch, char *buf, const size_t size) {
    const time_t t = epoch;
    const struct tm *tm_info = localtime(&t);
    if (!tm_info) {
        snprintf(buf, size, "%s", lang.generic.unknown);
        return;
    }
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

static void sanitize_filename(const char *name, char *out, const size_t out_len) {
    size_t j = 0;
    for (size_t i = 0; name[i] && j + 1 < out_len; i++) {
        const unsigned char c = (unsigned char) name[i];
        out[j++] = (char) (isalnum(c) || c == ' ' || c == '-' || c == '_' ? c : '_');
    }
    out[j] = '\0';

    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '.'))
        out[--j] = '\0';

    size_t start = 0;
    while (out[start] == ' ' || out[start] == '.')
        start++;
    if (start > 0) memmove(out, out + start, j - start + 1);

    if (out[0] == '\0') snprintf(out, out_len, "macro");
}

static void assign_unique_path(struct macro_entry *entry) {
    char safe[MACRO_NAME_MAX];
    sanitize_filename(entry->name, safe, sizeof(safe));

    snprintf(entry->path, sizeof(entry->path), "%s/%s.ini", base_dir, safe);
    if (!file_exist(entry->path)) return;

    for (int n = 2; n < 1000; n++) {
        snprintf(entry->path, sizeof(entry->path), "%s/%s (%d).ini", base_dir, safe, n);
        if (!file_exist(entry->path)) return;
    }
}

static void write_manifest_entry(const struct macro_entry *entry) {
    mini_t *ini = mini_create(entry->path);
    if (!ini) return;

    mini_set_string(ini, MACRO_GROUP, "name", entry->name);
    mini_set_int(ini, MACRO_GROUP, "created", entry->created);
    mini_set_int(ini, MACRO_GROUP, "index", entry->index);
    mini_set_int(ini, MACRO_GROUP, "step_count", entry->step_count);

    for (int i = 0; i < entry->step_count; i++) {
        char mask_key[24];
        char hz_key[24];
        snprintf(mask_key, sizeof(mask_key), "step%d_mask", i);
        snprintf(hz_key, sizeof(hz_key), "step%d_hz", i);

        mini_set_int(ini, MACRO_GROUP, mask_key, entry->steps[i].target_mask);
        mini_set_int(ini, MACRO_GROUP, hz_key, entry->steps[i].hz_rate);
    }

    mini_save(ini, 0);
    mini_free(ini);
}

static int next_free_index(void) {
    int max_index = -1;
    for (int i = 0; i < macro_count; i++) {
        if (macro_list[i].index > max_index) max_index = macro_list[i].index;
    }
    return max_index + 1;
}

static int macros_position_from_index(const int index) {
    for (int i = 0; i < macro_count; i++) {
        if (macro_list[i].index == index) return i;
    }
    return -1;
}

const struct macro_entry *macros_get_by_index(const int index) {
    const int position = macros_position_from_index(index);
    return position < 0 ? NULL : &macro_list[position];
}

const char *macros_get_name_by_index(const int index) {
    const struct macro_entry *entry = macros_get_by_index(index);
    return entry ? entry->name : lang.generic.unknown;
}

static int macro_cmp(const void *a, const void *b) {
    return strcasecmp(((const struct macro_entry *) a)->name, ((const struct macro_entry *) b)->name);
}

void macros_init(const char *macro_dir) {
    snprintf(base_dir, sizeof(base_dir), "%s", macro_dir);
    create_directories(base_dir, 0);

    macro_count = 0;

    DIR *dir = opendir(base_dir);
    if (!dir) return;

    struct dirent *ent;
    while (((ent = readdir(dir))) && macro_count < MACRO_MAX) {
        if (ent->d_type != DT_REG && ent->d_type != DT_UNKNOWN) continue;

        const char *ext = strrchr(ent->d_name, '.');
        if (!ext || strcasecmp(ext, ".ini") != 0) continue;

        char file_path[MACRO_PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", base_dir, ent->d_name);

        mini_t *ini = mini_try_load(file_path);
        if (!ini) continue;

        struct macro_entry *entry = &macro_list[macro_count];
        snprintf(entry->path, sizeof(entry->path), "%s", file_path);
        snprintf(
            entry->name, sizeof(entry->name), "%s", get_ini_string(ini, MACRO_GROUP, "name", lang.generic.unknown)
        );
        entry->created = mini_get_int(ini, MACRO_GROUP, "created", 0);
        entry->index = (int) mini_get_int(ini, MACRO_GROUP, "index", -1);

        const int step_count = (int) mini_get_int(ini, MACRO_GROUP, "step_count", 0);
        entry->step_count = step_count < 0 ? 0 : step_count > MACRO_STEP_MAX ? MACRO_STEP_MAX : step_count;

        for (int i = 0; i < entry->step_count; i++) {
            char mask_key[24];
            char hz_key[24];
            snprintf(mask_key, sizeof(mask_key), "step%d_mask", i);
            snprintf(hz_key, sizeof(hz_key), "step%d_hz", i);

            entry->steps[i].target_mask = (int) mini_get_int(ini, MACRO_GROUP, mask_key, 0);
            entry->steps[i].hz_rate = (int) mini_get_int(ini, MACRO_GROUP, hz_key, 0);
        }

        mini_free(ini);
        macro_count++;
    }

    closedir(dir);

    for (int i = 0; i < macro_count; i++) {
        if (macro_list[i].index < 0) {
            macro_list[i].index = next_free_index();
            write_manifest_entry(&macro_list[i]);
        }
    }

    qsort(macro_list, (size_t) macro_count, sizeof(struct macro_entry), macro_cmp);
}

int macros_create(const char *name) {
    if (!base_dir[0] || macro_count >= MACRO_MAX) return -1;

    const int position = macro_count;
    struct macro_entry *entry = &macro_list[position];

    entry->index = next_free_index();
    entry->created = (long long) time(NULL);
    entry->step_count = 0;

    if (name && *name) {
        snprintf(entry->name, sizeof(entry->name), "%s", name);
    } else {
        format_epoch(entry->created, entry->name, sizeof(entry->name));
    }

    assign_unique_path(entry);
    write_manifest_entry(entry);

    macro_count++;
    return position;
}

int macros_rename(const int position, const char *new_name) {
    if (position < 0 || position >= macro_count || !new_name || !*new_name) return -1;

    struct macro_entry *entry = &macro_list[position];
    char old_path[MACRO_PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s", entry->path);

    snprintf(entry->name, sizeof(entry->name), "%s", new_name);
    assign_unique_path(entry);

    if (strcmp(old_path, entry->path) != 0) remove(old_path);
    write_manifest_entry(entry);

    return 0;
}

int macros_delete(const int position) {
    if (position < 0 || position >= macro_count) return -1;

    remove(macro_list[position].path);

    for (int i = position; i < macro_count - 1; i++) {
        macro_list[i] = macro_list[i + 1];
    }
    macro_count--;

    return 0;
}

int macros_save(const int position) {
    if (position < 0 || position >= macro_count) return -1;
    write_manifest_entry(&macro_list[position]);
    return 0;
}

int macros_add_step(const int position, const int target_mask, const int hz_rate) {
    if (position < 0 || position >= macro_count) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (entry->step_count >= MACRO_STEP_MAX) return -1;

    entry->steps[entry->step_count].target_mask = target_mask;
    entry->steps[entry->step_count].hz_rate = hz_rate;
    entry->step_count++;

    return entry->step_count - 1;
}

int macros_remove_step(const int position, const int step_pos) {
    if (position < 0 || position >= macro_count) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    for (int i = step_pos; i < entry->step_count - 1; i++) {
        entry->steps[i] = entry->steps[i + 1];
    }
    entry->step_count--;

    return 0;
}

int macros_set_step_hz(const int position, const int step_pos, const int hz_rate) {
    if (position < 0 || position >= macro_count) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    entry->steps[step_pos].hz_rate = hz_rate;
    return 0;
}
