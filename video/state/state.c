#include "state.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common/base/options.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>
#include "paths.h"

#define HISTORY_FILE     WASABI_HISTORY_FILE
#define BOOKMARK_FILE    WASABI_BOOKMARK_FILE
#define COLLECTION_FILE  INFO_VID_PATH "/collection.tsv"
#define HISTORY_LIMIT   256
#define QUICK_BOOKMARK_NAME "\x1fwasabi-quick"

static int entry_compare_updated(const void *left, const void *right) {
    const video_state_entry *a = left;
    const video_state_entry *b = right;
    if (a->updated == b->updated) return strcasecmp(a->title, b->title);
    return a->updated < b->updated ? 1 : -1;
}

static int entry_compare_title(const void *left, const void *right) {
    const video_state_entry *a = left;
    const video_state_entry *b = right;
    return strcasecmp(a->title, b->title);
}

static int safe_char(const unsigned char c) {
    return isalnum(c) || c == ' ' || c == '/' || c == '.' || c == '_' || c == '-' || c == ':' || c == '?' || c == '&'
           || c == '=' || c == '+' || c == ',' || c == '(' || c == ')' || c == '[' || c == ']';
}

static char *encode_field(const char *text) {
    if (!text) return strdup("");

    const size_t length = strlen(text);
    if (length > (SIZE_MAX - 1) / 3) return NULL;

    char *encoded = malloc(length * 3 + 1);
    if (!encoded) return NULL;

    static const char hex[] = "0123456789ABCDEF";
    char *out = encoded;
    for (const unsigned char *p = (const unsigned char *) text; *p; ++p) {
        if (safe_char(*p) && *p != '%') {
            *out++ = (char) *p;
        } else {
            *out++ = '%';
            *out++ = hex[*p >> 4];
            *out++ = hex[*p & 15];
        }
    }
    *out = '\0';
    return encoded;
}

static int hex_value(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *decode_field(const char *text) {
    if (!text) return strdup("");
    char *decoded = malloc(strlen(text) + 1);
    if (!decoded) return NULL;

    char *out = decoded;
    for (size_t i = 0; text[i]; ++i) {
        if (text[i] == '%' && text[i + 1] && text[i + 2]) {
            const int hi = hex_value(text[i + 1]);
            const int lo = hex_value(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                *out++ = (char) ((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        *out++ = text[i];
    }
    *out = '\0';
    return decoded;
}

static char *next_field(char **cursor) {
    if (!cursor || !*cursor) return NULL;
    char *field = *cursor;
    char *separator = strchr(field, '\t');
    if (separator) {
        *separator = '\0';
        *cursor = separator + 1;
    } else {
        *cursor = NULL;
    }
    return field;
}

void video_state_free(video_state_entry *entries, const size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(entries[i].uri);
        free(entries[i].title);
        free(entries[i].thumbnail);
        free(entries[i].name);
    }
    free(entries);
}

int video_state_init(void) {
    create_directories(INFO_VID_PATH "/", 0);
    create_directories(INFO_VID_PATH "/live/", 0);
    create_directories(WASABI_SHARE_PATH, 0);
    create_directories(WASABI_STATE_PATH "/", 0);
    create_directories(WASABI_FILTER_PATH, 0);
    create_directories(WASABI_SHADER_PATH, 0);
    create_directories(WASABI_OVERLAY_PATH, 0);
    return dir_exist(INFO_VID_PATH) && dir_exist(WASABI_STATE_PATH) ? 0 : -1;
}

static uint64_t uri_key(const char *uri) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) uri; cursor && *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int video_state_thumbnail_path(
    const char *uri, const double position, const int bookmark, char *path, const size_t path_size
) {
    if (!uri || !*uri || !path || !path_size) return -1;

    char directory[PATH_MAX];
    if (snprintf(
            directory, sizeof(directory), "%s/%016llx", WASABI_STATE_PATH,
            (unsigned long long) uri_key(uri)
        ) >= (int) sizeof(directory))
        return -1;
    create_directories(directory, 0);
    if (!dir_exist(directory)) return -1;

    if (!bookmark) return snprintf(path, path_size, "%s/exit.png", directory) < (int) path_size ? 0 : -1;

    const long long milliseconds = position > 0.0 ? (long long) (position * 1000.0 + 0.5) : 0;
    return snprintf(path, path_size, "%s/bookmark_%012lld.png", directory, milliseconds) < (int) path_size ? 0 : -1;
}

static int append_entry(video_state_entry **entries, size_t *count, const video_state_entry *entry) {
    video_state_entry *grown = realloc(*entries, (*count + 1) * sizeof(**entries));
    if (!grown) return -1;
    *entries = grown;
    (*entries)[*count] = *entry;
    (*count)++;
    return 0;
}

static int load_file(const char *path, video_state_entry **entries, size_t *count, const int history) {
    *entries = NULL;
    *count = 0;

    FILE *file = fopen(path, "r");
    if (!file) return errno == ENOENT ? 0 : -1;

    char *line = NULL;
    size_t capacity = 0;
    while (getline(&line, &capacity, file) >= 0) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;

        char *cursor = line;
        char *uri = next_field(&cursor);
        char *title = next_field(&cursor);
        char *position = history ? next_field(&cursor) : NULL;
        char *duration = history ? next_field(&cursor) : NULL;
        char *updated = history ? next_field(&cursor) : NULL;
        char *thumbnail = history ? next_field(&cursor) : NULL;
        char *name = history ? next_field(&cursor) : NULL;
        char *live = next_field(&cursor);
        if (!uri || !title || (history && (!position || !duration || !updated))) continue;

        video_state_entry entry = {
            .uri = decode_field(uri),
            .title = decode_field(title),
            .thumbnail = thumbnail ? decode_field(thumbnail) : strdup(""),
            .name = name ? decode_field(name) : strdup(""),
            .position = position ? strtod(position, NULL) : 0.0,
            .duration = duration ? strtod(duration, NULL) : 0.0,
            .updated = updated ? (time_t) strtoll(updated, NULL, 10) : 0,
            .live = live ? atoi(live) != 0 : 0,
        };
        if (!entry.uri || !entry.title || !entry.thumbnail || !entry.name || append_entry(entries, count, &entry) < 0) {
            free(entry.uri);
            free(entry.title);
            free(entry.thumbnail);
            free(entry.name);
            video_state_free(*entries, *count);
            *entries = NULL;
            *count = 0;
            free(line);
            fclose(file);
            return -1;
        }
    }

    free(line);
    fclose(file);
    if (*count > 1) qsort(*entries, *count, sizeof(**entries), history ? entry_compare_updated : entry_compare_title);
    return 0;
}

int video_history_load(video_state_entry **entries, size_t *count) {
    return load_file(HISTORY_FILE, entries, count, 1);
}

int video_bookmark_load(video_state_entry **entries, size_t *count) {
    return load_file(BOOKMARK_FILE, entries, count, 1);
}

int video_collection_load(video_state_entry **entries, size_t *count) {
    return load_file(COLLECTION_FILE, entries, count, 0);
}

int video_history_find(const char *uri, video_state_entry *entry) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_history_load(&entries, &count) < 0) return 0;

    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].uri, uri) != 0) continue;
        if (entry) {
            *entry = entries[i];
            entries[i].uri = NULL;
            entries[i].title = NULL;
            entries[i].thumbnail = NULL;
            entries[i].name = NULL;
        }
        found = 1;
        break;
    }
    video_state_free(entries, count);
    return found;
}

static int write_entries(const char *path, const video_state_entry *entries, const size_t count, const int history) {
    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int) sizeof(temporary)) return -1;

    FILE *file = fopen(temporary, "w");
    if (!file) return -1;

    int result = 0;
    for (size_t i = 0; i < count; ++i) {
        char *uri = encode_field(entries[i].uri);
        char *title = encode_field(entries[i].title);
        char *thumbnail = history ? encode_field(entries[i].thumbnail) : NULL;
        char *name = history ? encode_field(entries[i].name) : NULL;
        if (!uri || !title || (history && (!thumbnail || !name))) {
            free(uri);
            free(title);
            free(thumbnail);
            free(name);
            result = -1;
            break;
        }

        if (history) {
            if (fprintf(file, "%s\t%s\t%.3f\t%.3f\t%lld\t%s\t%s\t%d\n", uri, title,
                        entries[i].position, entries[i].duration, (long long) entries[i].updated,
                        thumbnail, name, entries[i].live) < 0)
                result = -1;
        } else if (fprintf(file, "%s\t%s\t%d\n", uri, title, entries[i].live) < 0) {
            result = -1;
        }
        free(uri);
        free(title);
        free(thumbnail);
        free(name);
        if (result < 0) break;
    }

    if (fflush(file) != 0) result = -1;
    if (fclose(file) != 0) result = -1;

    if (result == 0 && rename(temporary, path) != 0) result = -1;
    if (result < 0) unlink(temporary);
    return result;
}

int video_history_update(
    const char *uri, const char *title, const double position, const double duration, const int complete,
    const char *thumbnail, const char *target_uri, const int live
) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_history_load(&entries, &count) < 0) return -1;

    size_t index = count;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].uri, uri) == 0) {
            index = i;
            break;
        }
    }

    if (!live && (complete || position < 1.0)) {
        if (index < count) {
            free(entries[index].uri);
            free(entries[index].title);
            free(entries[index].thumbnail);
            free(entries[index].name);
            memmove(entries + index, entries + index + 1, (count - index - 1) * sizeof(*entries));
            count--;
        }
    } else if (index < count) {
        char *replacement = strdup(title);
        if (!replacement) {
            video_state_free(entries, count);
            return -1;
        }
        free(entries[index].title);
        entries[index].title = replacement;
        entries[index].position = position;
        entries[index].duration = duration;
        entries[index].updated = time(NULL);
        entries[index].live = live;
        char *replacement_target = strdup(target_uri ? target_uri : "");
        if (!replacement_target) {
            video_state_free(entries, count);
            return -1;
        }
        free(entries[index].name);
        entries[index].name = replacement_target;
        if (thumbnail) {
            char *replacement_thumbnail = strdup(thumbnail);
            if (!replacement_thumbnail) {
                video_state_free(entries, count);
                return -1;
            }
            if (entries[index].thumbnail && entries[index].thumbnail[0]
                && strcmp(entries[index].thumbnail, replacement_thumbnail) != 0)
                remove(entries[index].thumbnail);
            free(entries[index].thumbnail);
            entries[index].thumbnail = replacement_thumbnail;
        }
    } else {
        video_state_entry entry = {
            .uri = strdup(uri),
            .title = strdup(title),
            .thumbnail = strdup(thumbnail ? thumbnail : ""),
            .name = strdup(target_uri ? target_uri : ""),
            .position = position,
            .duration = duration,
            .updated = time(NULL),
            .live = live,
        };
        if (!entry.uri || !entry.title || !entry.thumbnail || !entry.name
            || append_entry(&entries, &count, &entry) < 0) {
            free(entry.uri);
            free(entry.title);
            free(entry.thumbnail);
            free(entry.name);
            video_state_free(entries, count);
            return -1;
        }
    }

    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare_updated);
    if (count > HISTORY_LIMIT) {
        for (size_t i = HISTORY_LIMIT; i < count; ++i) {
            free(entries[i].uri);
            free(entries[i].title);
            free(entries[i].thumbnail);
            free(entries[i].name);
        }
        count = HISTORY_LIMIT;
    }

    const int result = write_entries(HISTORY_FILE, entries, count, 1);
    video_state_free(entries, count);
    return result;
}

static int bookmark_upsert(
    const char *uri, const char *title, const char *name, const double position, const double duration,
    const char *thumbnail, const int quick
) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_bookmark_load(&entries, &count) < 0) return -1;

    size_t index = count;
    for (size_t i = 0; i < count; ++i) {
        double difference = entries[i].position - position;
        if (difference < 0.0) difference = -difference;
        if (strcmp(entries[i].uri, uri) == 0
            && ((quick && strcmp(entries[i].name, QUICK_BOOKMARK_NAME) == 0)
                || (!quick && strcmp(entries[i].name, QUICK_BOOKMARK_NAME) != 0 && difference < 2.0))) {
            index = i;
            break;
        }
    }

    if (index < count) {
        char *replacement = strdup(title);
        if (!replacement) {
            video_state_free(entries, count);
            return -1;
        }
        free(entries[index].title);
        entries[index].title = replacement;
        entries[index].position = position;
        entries[index].duration = duration;
        entries[index].updated = time(NULL);
        char *replacement_name = strdup(name ? name : "");
        if (!replacement_name) {
            video_state_free(entries, count);
            return -1;
        }
        free(entries[index].name);
        entries[index].name = replacement_name;
        if (thumbnail) {
            char *replacement_thumbnail = strdup(thumbnail);
            if (!replacement_thumbnail) {
                video_state_free(entries, count);
                return -1;
            }
            if (entries[index].thumbnail && entries[index].thumbnail[0]
                && strcmp(entries[index].thumbnail, replacement_thumbnail) != 0)
                remove(entries[index].thumbnail);
            free(entries[index].thumbnail);
            entries[index].thumbnail = replacement_thumbnail;
        }
    } else {
        video_state_entry entry = {
            .uri = strdup(uri),
            .title = strdup(title),
            .thumbnail = strdup(thumbnail ? thumbnail : ""),
            .name = strdup(name ? name : ""),
            .position = position,
            .duration = duration,
            .updated = time(NULL),
        };
        if (!entry.uri || !entry.title || !entry.thumbnail || !entry.name
            || append_entry(&entries, &count, &entry) < 0) {
            free(entry.uri);
            free(entry.title);
            free(entry.thumbnail);
            free(entry.name);
            video_state_free(entries, count);
            return -1;
        }
    }

    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare_updated);
    if (count > HISTORY_LIMIT) {
        for (size_t i = HISTORY_LIMIT; i < count; ++i) {
            if (entries[i].thumbnail && entries[i].thumbnail[0]) remove(entries[i].thumbnail);
            free(entries[i].uri);
            free(entries[i].title);
            free(entries[i].thumbnail);
            free(entries[i].name);
        }
        count = HISTORY_LIMIT;
    }

    const int result = write_entries(BOOKMARK_FILE, entries, count, 1);
    video_state_free(entries, count);
    return result;
}

int video_bookmark_add(
    const char *uri, const char *title, const char *name, const double position, const double duration,
    const char *thumbnail
) {
    return bookmark_upsert(uri, title, name, position, duration, thumbnail, 0);
}

int video_bookmark_set_quick(
    const char *uri, const char *title, const double position, const double duration, const char *thumbnail
) {
    return bookmark_upsert(uri, title, QUICK_BOOKMARK_NAME, position, duration, thumbnail, 1);
}

int video_bookmark_find_quick(const char *uri, double *position) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_bookmark_load(&entries, &count) < 0) return 0;
    int found = 0;
    for (size_t index = 0; index < count; index++) {
        if (strcmp(entries[index].uri, uri) != 0
            || strcmp(entries[index].name, QUICK_BOOKMARK_NAME) != 0)
            continue;
        if (position) *position = entries[index].position;
        found = 1;
        break;
    }
    video_state_free(entries, count);
    return found;
}

int video_bookmark_is_quick(const char *name) {
    return name && strcmp(name, QUICK_BOOKMARK_NAME) == 0;
}

int video_bookmark_remove(const char *uri, const double position) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_bookmark_load(&entries, &count) < 0) return -1;

    for (size_t i = 0; i < count; ++i) {
        double difference = entries[i].position - position;
        if (difference < 0.0) difference = -difference;
        if (strcmp(entries[i].uri, uri) != 0 || difference >= 0.5) continue;
        if (entries[i].thumbnail && entries[i].thumbnail[0]) remove(entries[i].thumbnail);
        free(entries[i].uri);
        free(entries[i].title);
        free(entries[i].thumbnail);
        free(entries[i].name);
        memmove(entries + i, entries + i + 1, (count - i - 1) * sizeof(*entries));
        count--;
        break;
    }

    const int result = write_entries(BOOKMARK_FILE, entries, count, 1);
    video_state_free(entries, count);
    return result;
}

int video_collection_contains(const char *uri) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_collection_load(&entries, &count) < 0) return 0;

    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].uri, uri) == 0) {
            found = 1;
            break;
        }
    }
    video_state_free(entries, count);
    return found;
}

int video_collection_toggle(const char *uri, const char *title, const int live, int *collected) {
    video_state_entry *entries = NULL;
    size_t count = 0;
    if (video_collection_load(&entries, &count) < 0) return -1;

    size_t index = count;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].uri, uri) == 0) {
            index = i;
            break;
        }
    }

    if (index < count) {
        free(entries[index].uri);
        free(entries[index].title);
        free(entries[index].thumbnail);
        free(entries[index].name);
        memmove(entries + index, entries + index + 1, (count - index - 1) * sizeof(*entries));
        count--;
        if (collected) *collected = 0;
    } else {
        video_state_entry entry = {
            .uri = strdup(uri), .title = strdup(title), .thumbnail = strdup(""), .name = strdup(""), .live = live
        };
        if (!entry.uri || !entry.title || !entry.thumbnail || !entry.name
            || append_entry(&entries, &count, &entry) < 0) {
            free(entry.uri);
            free(entry.title);
            free(entry.thumbnail);
            free(entry.name);
            video_state_free(entries, count);
            return -1;
        }
        if (collected) *collected = 1;
    }

    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare_title);
    const int result = write_entries(COLLECTION_FILE, entries, count, 0);
    video_state_free(entries, count);
    return result;
}
