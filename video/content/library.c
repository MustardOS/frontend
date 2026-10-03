#include "library.h"

#include <common/content/content.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <json/json.h>
#include <common/content/manifest.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>

#define VIDEO_SCAN_DEPTH 12
#define PLAYLIST_TEXT_LIMIT (8U * 1024U * 1024U)

static char *trim(char *text);

static char playlist_lead(const char *title) {
    if (!title || !title[0]) return '#';
    const unsigned char lead = (unsigned char) title[0];
    return isalpha(lead) ? (char) toupper(lead) : '#';
}

size_t video_playlist_step(
    const video_library_entry *entries, const size_t count, size_t current, const int direction, size_t steps,
    const int wrap
) {
    if (!entries || !count) return 0;
    if (current >= count) current = count - 1;
    if (!steps || !direction) return current;

    if (!wrap) {
        if (direction < 0) return steps > current ? 0 : current - steps;
        const size_t remaining = count - current - 1;
        return current + (steps > remaining ? remaining : steps);
    }

    steps %= count;
    if (direction < 0) return current >= steps ? current - steps : count - (steps - current);
    return current + steps < count ? current + steps : current + steps - count;
}

size_t video_playlist_skip(
    const video_library_entry *entries, const size_t count, size_t current, const int direction,
    const size_t page_size, const int letter_skip
) {
    if (!entries || !count) return 0;
    if (current >= count) current = count - 1;

    if (letter_skip) {
        const char here = playlist_lead(entries[current].title);
        if (direction > 0) {
            for (size_t index = current + 1; index < count; index++)
                if (playlist_lead(entries[index].title) != here) return index;
        } else if (direction < 0 && current > 0) {
            size_t index = current;
            while (index > 0 && playlist_lead(entries[index - 1].title) == here)
                index--;
            if (index > 0) {
                const char previous = playlist_lead(entries[index - 1].title);
                index--;
                while (index > 0 && playlist_lead(entries[index - 1].title) == previous)
                    index--;
                return index;
            }
        }
    }

    return video_playlist_step(entries, count, current, direction, page_size ? page_size : 1, 0);
}

typedef struct {
    unsigned long number;
    char *uri;
    char *title;
} pls_entry;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} playlist_text;

static size_t playlist_write(const void *data, const size_t size, const size_t count, void *opaque) {
    playlist_text *text = opaque;
    if (!text || !size || count > SIZE_MAX / size) return 0;
    const size_t bytes = size * count;
    if (text->size > PLAYLIST_TEXT_LIMIT || bytes > PLAYLIST_TEXT_LIMIT - text->size) return 0;
    const size_t required = text->size + bytes + 1;
    if (required > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 64U * 1024U;
        while (capacity < required && capacity < PLAYLIST_TEXT_LIMIT + 1U) {
            const size_t next = capacity * 2U;
            capacity = next > capacity && next <= PLAYLIST_TEXT_LIMIT + 1U
                           ? next
                           : PLAYLIST_TEXT_LIMIT + 1U;
        }
        char *grown = realloc(text->data, capacity);
        if (!grown) return 0;
        text->data = grown;
        text->capacity = capacity;
    }
    memcpy(text->data + text->size, data, bytes);
    text->size += bytes;
    text->data[text->size] = '\0';
    return bytes;
}

static FILE *playlist_open(const char *path, char **owned) {
    static int curl_ready;
    *owned = NULL;
    if (!strstr(path, "://")) return fopen(path, "r");
    if (strncasecmp(path, "http://", 7) && strncasecmp(path, "https://", 8)) return NULL;
    if (!curl_ready) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return NULL;
        curl_ready = 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    playlist_text text = {0};
    curl_easy_setopt(curl, CURLOPT_URL, path);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, playlist_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &text);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MustardOS-Wasabi/1.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    const CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || !text.data || !text.size) {
        free(text.data);
        return NULL;
    }
    FILE *file = fmemopen(text.data, text.size, "r");
    if (!file) {
        free(text.data);
        return NULL;
    }
    *owned = text.data;
    return file;
}

static int entry_compare(const void *left, const void *right) {
    const video_library_entry *a = left;
    const video_library_entry *b = right;
    return strcasecmp(a->title, b->title);
}

static int has_extension(const char *name, const char *const extensions[]) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    for (size_t i = 0; extensions[i]; ++i)
        if (strcasecmp(dot, extensions[i]) == 0) return 1;
    return 0;
}

int video_path_extension_is(const char *path, const char *extension) {
    if (!path || !extension) return 0;
    const char *end = strpbrk(path, "?#");
    if (!end) end = path + strlen(path);
    const char *dot = NULL;
    for (const char *cursor = end; cursor > path;) {
        cursor--;
        if (*cursor == '/' || *cursor == '\\') break;
        if (*cursor == '.') {
            dot = cursor;
            break;
        }
    }
    const size_t length = strlen(extension);
    return dot && (size_t) (end - dot) == length && strncasecmp(dot, extension, length) == 0;
}

static int is_video(const char *name) {
    static const char *const extensions[] = {
        ".3g2", ".3gp", ".apng", ".asf", ".avi", ".bik", ".divx", ".flc", ".fli", ".flv",
        ".gif", ".h263", ".m2ts", ".m2v", ".m4v", ".mjpeg", ".mjpg", ".mkv", ".mov", ".mp4",
        ".mpe", ".mpeg", ".mpg", ".mts", ".mve", ".nut", ".ogm", ".ogv", ".qt",
        ".rm", ".rmvb", ".roq", ".smk", ".ts", ".vob", ".webm", ".wmv", NULL,
    };
    return has_extension(name, extensions);
}

int video_path_is_audio(const char *name) {
    return content_path_is_audio(name);
}

int video_path_is_sequenced(const char *name) {
    return content_path_is_sequenced(name);
}

const char *video_title_from_uri(const char *uri, char *buffer, const size_t size) {
    if (!buffer || size == 0) return "";
    const char *base = strrchr(uri ? uri : "", '/');
    base = base ? base + 1 : (uri ? uri : "");
    snprintf(buffer, size, "%s", base[0] ? base : uri);

    char *query = strchr(buffer, '?');
    if (query && strstr(uri, "://")) *query = '\0';
    char *dot = strrchr(buffer, '.');
    if (dot && !strchr(dot, '/')) *dot = '\0';
    return buffer;
}

static int append_entry(
    video_library_entry **entries, size_t *count, size_t *capacity, const char *uri, const char *title,
    const char *logo, const int live
) {
    if (*count >= *capacity) {
        const size_t next_capacity = *capacity ? *capacity * 2 : 32;
        if (next_capacity < *capacity || next_capacity > SIZE_MAX / sizeof(**entries)) return -1;
        video_library_entry *grown = realloc(*entries, next_capacity * sizeof(**entries));
        if (!grown) return -1;
        *entries = grown;
        *capacity = next_capacity;
    }

    video_library_entry *entry = &(*entries)[*count];
    entry->uri = strdup(uri);
    entry->title = strdup(title);
    entry->logo = logo && logo[0] ? strdup(logo) : NULL;
    entry->live = live;
    if (!entry->uri || !entry->title || (logo && logo[0] && !entry->logo)) {
        free(entry->uri);
        free(entry->title);
        free(entry->logo);
        return -1;
    }
    (*count)++;
    return 0;
}

void video_library_free(video_library_entry *entries, const size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(entries[i].uri);
        free(entries[i].title);
        free(entries[i].logo);
    }
    free(entries);
}

static int scan_media_dir(
    const char *path, const int depth, const int audio, video_library_entry **entries, size_t *count,
    size_t *capacity
) {
    if (depth > VIDEO_SCAN_DEPTH) return 0;

    DIR *directory = opendir(path);
    if (!directory) return errno == ENOENT ? 0 : -1;

    int result = 0;
    struct dirent *item;
    while ((item = readdir(directory))) {
        if (item->d_name[0] == '.') continue;

        char full[PATH_MAX];
        if (snprintf(full, sizeof(full), "%s/%s", path, item->d_name) >= (int) sizeof(full)) continue;

        int is_directory = item->d_type == DT_DIR;
        int is_regular = item->d_type == DT_REG;
        if (item->d_type == DT_LNK) continue;
        if (item->d_type == DT_UNKNOWN) {
            struct stat info;
            if (lstat(full, &info) != 0 || S_ISLNK(info.st_mode)) continue;
            is_directory = S_ISDIR(info.st_mode);
            is_regular = S_ISREG(info.st_mode);
        }

        if (is_directory) {
            if (scan_media_dir(full, depth + 1, audio, entries, count, capacity) < 0) {
                result = -1;
                break;
            }
        } else if (is_regular && (audio ? video_path_is_audio(item->d_name) : is_video(item->d_name))) {
            char title[PATH_MAX];
            video_title_from_uri(item->d_name, title, sizeof(title));
            if (append_entry(entries, count, capacity, full, title, NULL, 0) < 0) {
                result = -1;
                break;
            }
        }
    }

    closedir(directory);
    return result;
}

int video_library_scan(const char *root, const int audio, video_library_entry **entries, size_t *count) {
    *entries = NULL;
    *count = 0;
    if (!root || !root[0]) return 0;

    size_t capacity = 0;
    const int result = scan_media_dir(root, 0, audio, entries, count, &capacity);
    return result;
}

static int natural_compare(const char *left, const char *right) {
    while (*left && *right) {
        if (isdigit((unsigned char) *left) && isdigit((unsigned char) *right)) {
            while (*left == '0')
                left++;
            while (*right == '0')
                right++;

            size_t left_digits = 0;
            size_t right_digits = 0;
            while (isdigit((unsigned char) left[left_digits]))
                left_digits++;
            while (isdigit((unsigned char) right[right_digits]))
                right_digits++;

            if (left_digits != right_digits) return left_digits < right_digits ? -1 : 1;

            const int order = strncmp(left, right, left_digits);
            if (order) return order;

            left += left_digits;
            right += right_digits;
            continue;
        }

        const int a = tolower((unsigned char) *left);
        const int b = tolower((unsigned char) *right);
        if (a != b) return a < b ? -1 : 1;

        left++;
        right++;
    }

    return *left ? 1 : *right ? -1 : 0;
}

static int folder_entry_compare(const void *left, const void *right) {
    const video_library_entry *a = left;
    const video_library_entry *b = right;
    const int order = natural_compare(a->title, b->title);
    return order ? order : strcmp(a->uri, b->uri);
}

int video_folder_playlist(const char *path, video_library_entry **entries, size_t *count, size_t *index) {
    *entries = NULL;
    *count = 0;
    *index = 0;

    if (!path || !path[0] || strstr(path, "://") || strchr(path, '#')) return 0;

    const int audio = video_path_is_audio(path);
    if (!audio && !is_video(path)) return 0;

    const char *slash = strrchr(path, '/');
    if (!slash) return 0;

    char folder[PATH_MAX];
    if (snprintf(folder, sizeof(folder), "%.*s", (int) (slash - path), path) >= (int) sizeof(folder)) return 0;
    if (!folder[0]) snprintf(folder, sizeof(folder), "/");

    DIR *directory = opendir(folder);
    if (!directory) return 0;

    size_t capacity = 0;
    int result = 0;
    const struct dirent *item;
    while ((item = readdir(directory))) {
        if (item->d_name[0] == '.') continue;
        if (audio ? !video_path_is_audio(item->d_name) : !is_video(item->d_name)) continue;

        char full[PATH_MAX];
        if (snprintf(full, sizeof(full), "%s/%s", folder, item->d_name) >= (int) sizeof(full)) continue;

        struct stat info;
        if (stat(full, &info) != 0 || !S_ISREG(info.st_mode)) continue;

        char title[PATH_MAX];
        video_title_from_uri(item->d_name, title, sizeof(title));
        if (append_entry(entries, count, &capacity, full, title, NULL, 0) < 0) {
            result = -1;
            break;
        }
    }
    closedir(directory);

    if (result < 0 || *count < 2) {
        video_library_free(*entries, *count);
        *entries = NULL;
        *count = 0;
        return result;
    }

    qsort(*entries, *count, sizeof(**entries), folder_entry_compare);

    for (size_t position = 0; position < *count; position++) {
        if (strcmp(strrchr((*entries)[position].uri, '/') + 1, slash + 1) != 0) continue;
        *index = position;
        return 1;
    }

    video_library_free(*entries, *count);
    *entries = NULL;
    *count = 0;
    return 0;
}

video_playlist_type video_playlist_probe(const char *path) {
    char *owned = NULL;
    FILE *file = playlist_open(path, &owned);
    if (!file) return video_playlist_unavailable;
    char line[1024];
    video_playlist_type type = video_playlist_m3u;
    while (fgets(line, sizeof(line), file)) {
        char *value = trim(line);
        if (strlen(value) >= 3 && (unsigned char) value[0] == 0xef && (unsigned char) value[1] == 0xbb
            && (unsigned char) value[2] == 0xbf)
            value += 3;
        if (strncasecmp(value, "#EXT-X-", 7) == 0) {
            type = video_playlist_hls;
            break;
        }
    }
    fclose(file);
    free(owned);
    return type;
}

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') text++;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static void extinf_title(const char *line, char *title, const size_t size) {
    const char *comma = strrchr(line, ',');
    if (comma && comma[1]) {
        snprintf(title, size, "%s", trim((char *) comma + 1));
        return;
    }

    const char *name = strstr(line, "tvg-name=\"");
    if (name) {
        name += strlen("tvg-name=\"");
        const char *end = strchr(name, '"');
        if (end) {
            snprintf(title, size, "%.*s", (int) (end - name), name);
            return;
        }
    }
    title[0] = '\0';
}

static void extinf_attribute(const char *line, const char *attribute, char *value, const size_t size) {
    value[0] = '\0';
    char pattern[64];
    if (snprintf(pattern, sizeof(pattern), "%s=\"", attribute) >= (int) sizeof(pattern)) return;
    const char *start = strcasestr(line, pattern);
    if (!start) return;
    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end || end == start) return;
    snprintf(value, size, "%.*s", (int) (end - start), start);
}

static int resolve_playlist_path(const char *path, const char *value, char *resolved, const size_t size) {
    if (!value || !value[0]) {
        resolved[0] = '\0';
        return 1;
    }
    if (strstr(value, "://") || value[0] == '/')
        return snprintf(resolved, size, "%s", value) < (int) size;
    const char *slash = strrchr(path, '/');
    const int directory_length = slash ? (int) (slash - path) : 0;
    const int written = directory_length > 0
                            ? snprintf(resolved, size, "%.*s/%s", directory_length, path, value)
                            : snprintf(resolved, size, "%s", value);
    return written >= 0 && written < (int) size;
}

static int tvheadend_uri(const char *value, char *uri, const size_t size) {
    if (strncasecmp(value, "pipe://", 7) != 0) return 0;

    const char *input = strstr(value + 7, " -i ");
    if (!input) return 0;
    input += 4;
    while (isspace((unsigned char) *input)) input++;

    char quote = '\0';
    if (*input == '\'' || *input == '"') quote = *input++;
    const char *end = input;
    while (*end && (quote ? *end != quote : !isspace((unsigned char) *end))) end++;

    const size_t length = (size_t) (end - input);
    if (!length || length >= size) return 0;
    memcpy(uri, input, length);
    uri[length] = '\0';
    return strncasecmp(uri, "http://", 7) == 0 || strncasecmp(uri, "https://", 8) == 0;
}

static int append_playlist_entry(
    video_library_entry **entries, size_t *count, size_t *capacity, const char *path, const char *value,
    const char *pending_title, const char *pending_logo, const int live
) {
    char source[PATH_MAX];
    if (strncasecmp(value, "pipe://", 7) == 0) {
        if (!tvheadend_uri(value, source, sizeof(source))) return 0;
    } else if (snprintf(source, sizeof(source), "%s", value) >= (int) sizeof(source)) {
        return 0;
    }

    char title[PATH_MAX];
    if (pending_title && pending_title[0])
        snprintf(title, sizeof(title), "%s", pending_title);
    else
        video_title_from_uri(source, title, sizeof(title));

    char uri[PATH_MAX];
    char logo[PATH_MAX];
    if (!resolve_playlist_path(path, source, uri, sizeof(uri))
        || !resolve_playlist_path(path, pending_logo, logo, sizeof(logo)))
        return 0;

    return append_entry(entries, count, capacity, uri, title, logo, live);
}

static int load_m3u_playlist(
    const char *path, const int live, video_library_entry **entries, size_t *count
) {
    char *owned = NULL;
    FILE *file = playlist_open(path, &owned);
    if (!file) return -1;

    char pending_title[PATH_MAX] = "";
    char pending_logo[PATH_MAX] = "";
    char *line = NULL;
    size_t capacity = 0;
    size_t entry_capacity = 0;
    int result = 0;
    while (getline(&line, &capacity, file) >= 0) {
        char *value = trim(line);
        if (!value[0]) continue;
        if (strncmp(value, "#EXTINF:", 8) == 0) {
            extinf_title(value, pending_title, sizeof(pending_title));
            extinf_attribute(value, "tvg-logo", pending_logo, sizeof(pending_logo));
            continue;
        }
        if (value[0] == '#') continue;

        if (append_playlist_entry(
                entries, count, &entry_capacity, path, value, pending_title, pending_logo, live
            ) < 0) {
            result = -1;
            break;
        }
        pending_title[0] = '\0';
        pending_logo[0] = '\0';
    }

    free(line);
    fclose(file);
    free(owned);
    return result;
}

static int pls_number(const char *key, const char *prefix, unsigned long *number) {
    const size_t prefix_length = strlen(prefix);
    if (strncasecmp(key, prefix, prefix_length) != 0) return 0;
    const char *digits = key + prefix_length;
    if (!isdigit((unsigned char) *digits)) return 0;
    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(digits, &end, 10);
    if (errno || !end || *end) return 0;
    *number = value;
    return 1;
}

static pls_entry *pls_find_or_add(pls_entry **items, size_t *count, size_t *capacity, const unsigned long number) {
    for (size_t index = 0; index < *count; index++)
        if ((*items)[index].number == number) return &(*items)[index];

    if (*count >= *capacity) {
        const size_t next_capacity = *capacity ? *capacity * 2 : 32;
        if (next_capacity < *capacity || next_capacity > SIZE_MAX / sizeof(**items)) return NULL;
        pls_entry *grown = realloc(*items, next_capacity * sizeof(**items));
        if (!grown) return NULL;
        *items = grown;
        *capacity = next_capacity;
    }

    pls_entry *item = &(*items)[(*count)++];
    *item = (pls_entry) {.number = number};
    return item;
}

static int replace_text(char **target, const char *value) {
    char *replacement = strdup(value);
    if (!replacement) return -1;
    free(*target);
    *target = replacement;
    return 0;
}

static int load_pls_playlist(
    const char *path, const int live, video_library_entry **entries, size_t *count
) {
    FILE *file = fopen(path, "r");
    if (!file) return -1;

    pls_entry *items = NULL;
    size_t item_count = 0;
    size_t item_capacity = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    int result = 0;

    while (getline(&line, &line_capacity, file) >= 0) {
        char *key = trim(line);
        if (!key[0] || key[0] == ';' || key[0] == '#' || key[0] == '[') continue;
        char *separator = strchr(key, '=');
        if (!separator) continue;
        *separator = '\0';
        char *value = trim(separator + 1);
        key = trim(key);

        unsigned long number = 0;
        char **field = NULL;
        pls_entry *item = NULL;
        if (pls_number(key, "File", &number)) {
            item = pls_find_or_add(&items, &item_count, &item_capacity, number);
            if (item) field = &item->uri;
        } else if (pls_number(key, "Title", &number)) {
            item = pls_find_or_add(&items, &item_count, &item_capacity, number);
            if (item) field = &item->title;
        }
        if (item && field && replace_text(field, value) < 0) {
            result = -1;
            break;
        }
        if (!item && (strncasecmp(key, "File", 4) == 0 || strncasecmp(key, "Title", 5) == 0)) {
            result = -1;
            break;
        }
    }

    free(line);
    fclose(file);

    size_t entry_capacity = 0;
    if (result == 0) {
        for (size_t index = 0; index < item_count; index++) {
            if (!items[index].uri || !items[index].uri[0]) continue;
            if (append_playlist_entry(
                    entries, count, &entry_capacity, path, items[index].uri, items[index].title, NULL, live
                ) < 0) {
                result = -1;
                break;
            }
        }
    }

    for (size_t index = 0; index < item_count; index++) {
        free(items[index].uri);
        free(items[index].title);
    }
    free(items);
    return result;
}

static int append_json_entry(
    const char *path, const int live, const struct json node, const struct json key, video_library_entry **entries,
    size_t *count, size_t *capacity
) {
    if (json_type(node) != JSON_OBJECT) return 0;

    char uri[PATH_MAX];
    if (!manifest_json_string(node, "mjh_master", uri, sizeof(uri)) &&
        !manifest_json_string(node, "url", uri, sizeof(uri)) &&
        !manifest_json_string(node, "stream", uri, sizeof(uri)) &&
        !manifest_json_string(node, "stream_url", uri, sizeof(uri)))
        return 0;

    char title[PATH_MAX];
    if (!manifest_json_string(node, "name", title, sizeof(title))) {
        if (json_type(key) == JSON_STRING) {
            const size_t length = json_string_copy(key, title, sizeof(title));
            if (!length || length >= sizeof(title)) title[0] = '\0';
        } else {
            title[0] = '\0';
        }
    }

    char logo[PATH_MAX] = "";
    manifest_json_string(node, "logo", logo, sizeof(logo));

    return append_playlist_entry(entries, count, capacity, path, uri, title, logo, live);
}

static int load_json_playlist(
    const char *path, const int live, video_library_entry **entries, size_t *count
) {
    char *content = read_all_char_from(path);
    if (!content) return -1;
    if (!json_valid(content)) {
        free(content);
        return -1;
    }

    const struct json root = json_parse(content);
    size_t capacity = 0;
    int result = 0;
    if (json_type(root) == JSON_OBJECT) {
        for (struct json key = json_first(root); json_exists(key); key = json_next(json_next(key))) {
            const struct json node = json_next(key);
            if (append_json_entry(path, live, node, key, entries, count, &capacity) < 0) {
                result = -1;
                break;
            }
        }
    } else if (json_type(root) == JSON_ARRAY) {
        const struct json no_key = {0};
        for (struct json node = json_first(root); json_exists(node); node = json_next(node)) {
            if (append_json_entry(path, live, node, no_key, entries, count, &capacity) < 0) {
                result = -1;
                break;
            }
        }
    } else {
        result = -1;
    }

    free(content);
    return result;
}

int video_playlist_load(
    const char *path, const int live, video_library_entry **entries, size_t *count
) {
    if (!entries || !count) return -1;
    *entries = NULL;
    *count = 0;

    int result;
    if (video_path_extension_is(path, ".json"))
        result = load_json_playlist(path, live, entries, count);
    else if (video_path_extension_is(path, ".pls"))
        result = load_pls_playlist(path, live, entries, count);
    else
        result = load_m3u_playlist(path, live, entries, count);

    if (result < 0) {
        video_library_free(*entries, *count);
        *entries = NULL;
        *count = 0;
        return result;
    }
    if (*count > 1) qsort(*entries, *count, sizeof(**entries), entry_compare);
    return 0;
}

int video_live_scan(const char *root, video_library_entry **entries, size_t *count) {
    *entries = NULL;
    *count = 0;

    DIR *directory = opendir(root);
    if (!directory) return errno == ENOENT ? 0 : -1;

    int result = 0;
    size_t capacity = 0;
    struct dirent *item;
    static const char *const playlist_extensions[] = {".json", ".m3u", ".m3u8", ".pls", NULL};
    while ((item = readdir(directory))) {
        if (item->d_name[0] == '.' || !has_extension(item->d_name, playlist_extensions)) continue;

        char full[PATH_MAX];
        if (snprintf(full, sizeof(full), "%s/%s", root, item->d_name) >= (int) sizeof(full)) continue;
        video_library_entry *playlist = NULL;
        size_t playlist_count = 0;
        if (video_playlist_load(full, 1, &playlist, &playlist_count) < 0) {
            result = -1;
            break;
        }
        for (size_t index = 0; index < playlist_count; index++) {
            if (append_entry(
                    entries, count, &capacity, playlist[index].uri, playlist[index].title,
                    playlist[index].logo, 1
                ) < 0) {
                result = -1;
                break;
            }
        }
        video_library_free(playlist, playlist_count);
        if (result < 0) break;
    }

    closedir(directory);
    if (*count > 1) qsort(*entries, *count, sizeof(**entries), entry_compare);
    return result;
}
