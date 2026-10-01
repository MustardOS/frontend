#include "switcher.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <common/base/options.h>
#include <common/base/strutil.h>
#include <common/config/skip.h>
#include <common/content/content.h>
#include <common/content/core/common.h>
#include <common/storage/fileio.h>
#include <common/storage/union.h>

static int append_entry(
    content_switch_list *list, const char *path, const char *title, const content_switch_source source,
    const content_switch_native native, const long modified
) {
    for (size_t index = 0; index < list->count; index++)
        if (strcasecmp(list->entries[index].path, path) == 0) return 1;

    content_switch_entry *entries = realloc(list->entries, (list->count + 1) * sizeof(*entries));
    if (!entries) return 0;
    list->entries = entries;

    content_switch_entry *entry = &list->entries[list->count];
    memset(entry, 0, sizeof(*entry));
    entry->path = strdup(path);
    entry->title = strdup(title && *title ? title : get_file_name(path));
    if (!entry->path || !entry->title) {
        free(entry->path);
        free(entry->title);
        return 0;
    }
    entry->source = source;
    entry->native = native;
    entry->modified = modified;
    list->count++;
    return 1;
}

static content_switch_native native_type(const char *path) {
    char resolved[PATH_MAX];
    if (!union_resolve_to_real(path, resolved, sizeof(resolved)) || !file_exist(resolved))
        return content_switch_native_none;

    char *directory = get_content_path(resolved);
    char *name = strip_ext(get_file_name(resolved));
    if (!directory || !name) {
        free(directory);
        free(name);
        return content_switch_native_none;
    }

    char relative[PATH_MAX];
    union_get_relative_path(directory, relative, sizeof(relative));
    if (strncasecmp(relative, MAIN_ROM_DIR, strlen(MAIN_ROM_DIR)) == 0) {
        char *cursor = relative + strlen(MAIN_ROM_DIR);
        while (*cursor == '/') cursor++;
        memmove(relative, cursor, strlen(cursor) + 1);
    }

    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), INFO_CON_PATH "/%s/%s.cfg", relative, name);
    remove_double_slashes(config_path);

    int core_line = content_core;
    int launch_line = content_assign;
    if (!file_exist(config_path)) {
        snprintf(config_path, sizeof(config_path), INFO_CON_PATH "/%s/core.cfg", relative);
        remove_double_slashes(config_path);
        core_line = global_core;
        launch_line = global_assign;
    }

    free(directory);
    free(name);
    if (!file_exist(config_path)) return content_switch_native_none;

    char *core = read_line_char_from(config_path, (size_t) core_line);
    char *launch = read_line_char_from(config_path, (size_t) launch_line);
    content_switch_native native = content_switch_native_none;
    if (core_uses_muxretro(launch) && core && *core) {
        char core_path[PATH_MAX];
        const int written = core[0] == '/'
                                ? snprintf(core_path, sizeof(core_path), "%s", core)
                                : snprintf(core_path, sizeof(core_path), OPT_SHARE_PATH "core/%s", core);
        if (written > 0 && (size_t) written < sizeof(core_path) && file_exist(core_path))
            native = content_switch_native_pickles;
    } else if ((core && strcasecmp(core, "ext-video") == 0)
               || (launch && (strcasecmp(launch, "ext-video") == 0 || strcasecmp(launch, "ext-video.sh") == 0))) {
        native = content_switch_native_wasabi;
    }
    free(core);
    free(launch);
    return native;
}

static void scan_directory(
    content_switch_list *list, const char *directory, const content_switch_source source, const int recurse
) {
    DIR *stream = opendir(directory);
    if (!stream) return;

    struct dirent *item;
    while ((item = readdir(stream))) {
        if (item->d_name[0] == '.') continue;

        char entry_path[PATH_MAX];
        if (snprintf(entry_path, sizeof(entry_path), "%s/%s", directory, item->d_name) >= (int) sizeof(entry_path))
            continue;

        struct stat status;
        if (lstat(entry_path, &status) != 0) continue;
        if (S_ISDIR(status.st_mode) && recurse) {
            scan_directory(list, entry_path, source, recurse);
            continue;
        }
        if (!S_ISREG(status.st_mode) || !ends_with(item->d_name, ".cfg")) continue;

        char *path = read_line_char_from(entry_path, 1);
        char *title = read_line_char_from(entry_path, 3);
        if (!path || !*path) {
            free(path);
            free(title);
            continue;
        }

        const content_switch_native native = native_type(path);
        if (native != content_switch_native_none)
            append_entry(list, path, title, source, native, (long) status.st_mtime);
        free(path);
        free(title);
    }
    closedir(stream);
}

static int compare_entries(const void *left, const void *right) {
    const content_switch_entry *a = left;
    const content_switch_entry *b = right;
    if (a->source != b->source) return a->source < b->source ? -1 : 1;
    if (a->source == content_switch_source_history && a->modified != b->modified)
        return a->modified > b->modified ? -1 : 1;
    const int title = strcasecmp(a->title, b->title);
    return title ? title : strcasecmp(a->path, b->path);
}

int content_switch_load(content_switch_list *list, const char *current_path) {
    if (!list) return 0;
    content_switch_free(list);
    scan_directory(list, INFO_HIS_PATH, content_switch_source_history, 0);
    if (list->count > 1) qsort(list->entries, list->count, sizeof(*list->entries), compare_entries);

    list->selected = 0;
    if (current_path && *current_path) {
        char current[PATH_MAX];
        if (!union_resolve_to_real(current_path, current, sizeof(current)))
            snprintf(current, sizeof(current), "%s", current_path);
        for (size_t index = 0; index < list->count;) {
            char candidate[PATH_MAX];
            if (!union_resolve_to_real(list->entries[index].path, candidate, sizeof(candidate)))
                snprintf(candidate, sizeof(candidate), "%s", list->entries[index].path);
            if (strcasecmp(current, candidate) == 0) {
                free(list->entries[index].path);
                free(list->entries[index].title);
                if (index + 1 < list->count)
                    memmove(&list->entries[index], &list->entries[index + 1],
                            (list->count - index - 1) * sizeof(*list->entries));
                list->count--;
                continue;
            }
            index++;
        }
    }
    return list->count > 0;
}

void content_switch_free(content_switch_list *list) {
    if (!list) return;
    for (size_t index = 0; index < list->count; index++) {
        free(list->entries[index].path);
        free(list->entries[index].title);
    }
    free(list->entries);
    memset(list, 0, sizeof(*list));
}

int content_switch_write_request(const char *path, const content_switch_native native) {
    if (!path || !*path) return 0;
    if (!write_text_to_file_atomic(CONTENT_SWITCH_REQUEST, CHAR, path)) return 0;
    if (native == content_switch_native_wasabi)
        return write_text_to_file_atomic(WASABI_HISTORY_LAUNCH, CHAR, "1");
    remove(WASABI_HISTORY_LAUNCH);
    return 1;
}
