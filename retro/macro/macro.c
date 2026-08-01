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
#include "relish.h"

struct macro_entry macro_list[MACRO_MAX];
int macro_count = 0;

static char base_dir[MACRO_PATH_MAX] = "";
static int macro_lookup[MACRO_MAX];
static int macro_lookup_count = 0;

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
        char group[24];
        snprintf(group, sizeof(group), "step%d", i);

        mini_set_int(ini, group, "kind", entry->steps[i].kind);
        mini_set_int(ini, group, "mask", entry->steps[i].target_mask);
        mini_set_int(ini, group, "wait", entry->steps[i].wait_ms);
        mini_set_int(ini, group, "ms", entry->steps[i].hold_ms);
        mini_set_int(ini, group, "repeat", entry->steps[i].repeat);
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

static void macros_rebuild_lookup(void) {
    macro_lookup_count = macro_count;

    for (int i = 0; i < macro_lookup_count; i++) {
        int at = i;
        while (at > 0 && macro_list[macro_lookup[at - 1]].index > macro_list[i].index) {
            macro_lookup[at] = macro_lookup[at - 1];
            at--;
        }
        macro_lookup[at] = i;
    }
}

static int macros_position_from_index(const int index) {
    int low = 0;
    int high = macro_lookup_count - 1;

    while (low <= high) {
        const int middle = low + (high - low) / 2;
        const int position = macro_lookup[middle];
        const int found = macro_list[position].index;

        if (found == index) return position;
        if (found < index) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
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
    macro_lookup_count = 0;
    relish_registry_load(base_dir);

    DIR *dir = opendir(base_dir);
    if (!dir) return;

    struct dirent *ent;
    while (((ent = readdir(dir))) && macro_count < MACRO_MAX) {
        if (ent->d_type != DT_REG && ent->d_type != DT_UNKNOWN) continue;

        const char *ext = strrchr(ent->d_name, '.');
        if (!ext) continue;

        if (strcasecmp(ext, ".rls") == 0) {
            char file_path[MACRO_PATH_MAX];
            snprintf(file_path, sizeof(file_path), "%s/%s", base_dir, ent->d_name);

            relish_compile_file(file_path, &macro_list[macro_count]);
            macro_count++;
            continue;
        }

        if (strcasecmp(ext, ".ini") != 0) continue;

        char file_path[MACRO_PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", base_dir, ent->d_name);

        mini_t *ini = mini_try_load(file_path);
        if (!ini) continue;

        struct macro_entry *entry = &macro_list[macro_count];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->path, sizeof(entry->path), "%s", file_path);
        snprintf(
            entry->name, sizeof(entry->name), "%s", get_ini_string(ini, MACRO_GROUP, "name", lang.generic.unknown)
        );
        entry->created = mini_get_int(ini, MACRO_GROUP, "created", 0);
        entry->index = (int) mini_get_int(ini, MACRO_GROUP, "index", -1);

        const int step_count = (int) mini_get_int(ini, MACRO_GROUP, "step_count", 0);
        entry->step_count = step_count < 0 ? 0 : step_count > MACRO_STEP_MAX ? MACRO_STEP_MAX : step_count;

        for (int i = 0; i < entry->step_count; i++) {
            char group[24];
            snprintf(group, sizeof(group), "step%d", i);

            const int kind = (int) mini_get_int(ini, group, "kind", macro_step_button);
            const int wait_ms = (int) mini_get_int(ini, group, "wait", MACRO_WAIT_MS_DEFAULT);
            const int hold_ms = (int) mini_get_int(ini, group, "ms", MACRO_HOLD_MS_DEFAULT);
            const int repeat = (int) mini_get_int(ini, group, "repeat", MACRO_REPEAT_DEFAULT);
            const int jump_target = (int) mini_get_int(ini, group, "jump", 0);
            const int loop_count = (int) mini_get_int(ini, group, "loop", MACRO_LOOP_COUNT_DEFAULT);

            entry->steps[i].kind = kind < macro_step_button || kind > macro_step_loop ? macro_step_button : kind;
            entry->steps[i].target_mask = (int) mini_get_int(ini, group, "mask", 0);
            entry->steps[i].wait_ms = wait_ms < 0 ? 0 : wait_ms > 65535 ? 65535 : wait_ms;
            entry->steps[i].hold_ms = hold_ms < 0 ? 0 : hold_ms > 65535 ? 65535 : hold_ms;
            entry->steps[i].repeat = repeat < 1 ? 1 : repeat > 65535 ? 65535 : repeat;
            entry->steps[i].jump_target = jump_target < 0 || jump_target >= i ? (i > 0 ? i - 1 : 0) : jump_target;
            entry->steps[i].loop_count = loop_count < 1 ? 1 : loop_count > 65535 ? 65535 : loop_count;
        }

        mini_free(ini);
        macro_count++;
    }

    closedir(dir);

    for (int i = 0; i < macro_count; i++) {
        if (macro_list[i].index < 0) {
            macro_list[i].index = next_free_index();
            if (macro_list[i].is_relish)
                relish_registry_record(macro_list[i].path, macro_list[i].index);
            else
                write_manifest_entry(&macro_list[i]);
        }
    }
    relish_registry_finalise(macro_list, macro_count, base_dir);

    qsort(macro_list, (size_t) macro_count, sizeof(struct macro_entry), macro_cmp);
    macros_rebuild_lookup();
}

int macros_create(const char *name) {
    if (!base_dir[0] || macro_count >= MACRO_MAX) return -1;

    const int position = macro_count;
    struct macro_entry *entry = &macro_list[position];

    memset(entry, 0, sizeof(*entry));
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
    macros_rebuild_lookup();
    return position;
}

int macros_rename(const int position, const char *new_name) {
    if (position < 0 || position >= macro_count || !new_name || !*new_name) return -1;
    if (macro_list[position].is_relish) return -1;

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
    macros_rebuild_lookup();

    return 0;
}

int macros_save(const int position) {
    if (position < 0 || position >= macro_count) return -1;
    write_manifest_entry(&macro_list[position]);
    return 0;
}

int macros_add_step(const int position, const struct macro_step *new_step) {
    if (position < 0 || position >= macro_count || !new_step) return -1;
    if (macro_list[position].is_relish) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (entry->step_count >= MACRO_STEP_MAX) return -1;

    if ((new_step->kind == macro_step_goto || new_step->kind == macro_step_loop) && entry->step_count <= 0) return -1;

    struct macro_step *step = &entry->steps[entry->step_count];
    *step = *new_step;
    step->repeat = step->repeat < 1 ? 1 : step->repeat;
    step->loop_count = step->loop_count < 1 ? 1 : step->loop_count;
    if (step->jump_target < 0 || step->jump_target >= entry->step_count)
        step->jump_target = entry->step_count > 0 ? entry->step_count - 1 : 0;

    entry->step_count++;

    return entry->step_count - 1;
}

int macros_remove_step(const int position, const int step_pos) {
    if (position < 0 || position >= macro_count) return -1;
    if (macro_list[position].is_relish) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    if (step_pos == 0 && entry->step_count > 1) {
        const int next_kind = entry->steps[1].kind;
        if (next_kind == macro_step_goto || next_kind == macro_step_loop) return -1;
    }

    for (int i = step_pos; i < entry->step_count - 1; i++) {
        entry->steps[i] = entry->steps[i + 1];
    }
    entry->step_count--;

    for (int i = 0; i < entry->step_count; i++) {
        struct macro_step *step = &entry->steps[i];
        if (step->kind != macro_step_goto && step->kind != macro_step_loop) continue;

        if (step->jump_target > step_pos) {
            step->jump_target--;
        } else if (step->jump_target == step_pos) {
            step->jump_target = step_pos > 0 ? step_pos - 1 : 0;
        }

        if (step->jump_target >= i) step->jump_target = i > 0 ? i - 1 : 0;
    }

    return 0;
}

int macros_set_step_wait_ms(const int position, const int step_pos, const int wait_ms) {
    if (position < 0 || position >= macro_count) return -1;
    if (macro_list[position].is_relish) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    entry->steps[step_pos].wait_ms = wait_ms;
    return 0;
}

int macros_set_step_hold_ms(const int position, const int step_pos, const int hold_ms) {
    if (position < 0 || position >= macro_count) return -1;
    if (macro_list[position].is_relish) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    entry->steps[step_pos].hold_ms = hold_ms;
    return 0;
}

int macros_set_step_repeat(const int position, const int step_pos, const int repeat) {
    if (position < 0 || position >= macro_count) return -1;
    if (macro_list[position].is_relish) return -1;

    struct macro_entry *entry = &macro_list[position];
    if (step_pos < 0 || step_pos >= entry->step_count) return -1;

    entry->steps[step_pos].repeat = repeat < 1 ? 1 : repeat;
    return 0;
}

static const int time_ms_table[] = {48, 96, 192, 384, 768, 1536, 3072, 6144};
#define TIME_MS_TABLE_COUNT ((int) (sizeof(time_ms_table) / sizeof(time_ms_table[0])))

int macros_cycle_step_time_ms(const int current_ms, const int direction) {
    int idx = 0;
    int best_diff = abs(time_ms_table[0] - current_ms);

    for (int i = 1; i < TIME_MS_TABLE_COUNT; i++) {
        const int diff = abs(time_ms_table[i] - current_ms);
        if (diff < best_diff) {
            best_diff = diff;
            idx = i;
        }
    }

    idx = (idx + (direction > 0 ? 1 : -1) + TIME_MS_TABLE_COUNT) % TIME_MS_TABLE_COUNT;
    return time_ms_table[idx];
}

static const int repeat_table[] = {1, 2, 3, 4, 5, 10, 20, 50};
#define REPEAT_TABLE_COUNT ((int) (sizeof(repeat_table) / sizeof(repeat_table[0])))

int macros_cycle_step_repeat(const int current_repeat, const int direction) {
    int idx = 0;
    int best_diff = abs(repeat_table[0] - current_repeat);

    for (int i = 1; i < REPEAT_TABLE_COUNT; i++) {
        const int diff = abs(repeat_table[i] - current_repeat);
        if (diff < best_diff) {
            best_diff = diff;
            idx = i;
        }
    }

    idx = (idx + (direction > 0 ? 1 : -1) + REPEAT_TABLE_COUNT) % REPEAT_TABLE_COUNT;
    return repeat_table[idx];
}

const char *macros_time_ms_name(const int time_ms) {
    static char buf[16];

    if (time_ms >= 1000 && time_ms % 1000 == 0) {
        snprintf(buf, sizeof(buf), "%ds", time_ms / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%dms", time_ms);
    }

    return buf;
}

const char *macros_step_compact_label(const struct macro_step *step, char *buf, const size_t buf_len) {
    if (step->kind == macro_step_goto) {
        snprintf(buf, buf_len, "-> Row %d", step->jump_target + 1);
        return buf;
    }

    if (step->kind == macro_step_loop) {
        snprintf(buf, buf_len, "-> Row %d x%d", step->jump_target + 1, step->loop_count);
        return buf;
    }

    if (step->kind == macro_step_button && step->target_mask == 0) {
        snprintf(buf, buf_len, "%s", macros_time_ms_name(step->hold_ms));
        return buf;
    }

    char wait_buf[16];
    char hold_buf[16];

    snprintf(wait_buf, sizeof(wait_buf), "%s", macros_time_ms_name(step->wait_ms));
    snprintf(hold_buf, sizeof(hold_buf), "%s", macros_time_ms_name(step->hold_ms));

    snprintf(buf, buf_len, "W%s-H%s-R%d", wait_buf, hold_buf, step->repeat);
    return buf;
}
