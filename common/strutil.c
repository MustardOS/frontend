#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "init.h"
#include "strutil.h"
#include "fileio.h"
#include "sysinfo.h"
#include "util.h"
#include "log.h"
#include "language.h"
#include "skip.h"

struct pattern skip_pattern_list = {NULL, 0, 0};
int skip_patterns_loaded = 0;

static void free_skip_patterns(void) {
    for (size_t i = 0; i < skip_pattern_list.count; i++)
        free(skip_pattern_list.patterns[i]);
    free(skip_pattern_list.patterns);
    skip_pattern_list.patterns = NULL;
    skip_pattern_list.count = 0;
    skip_pattern_list.capacity = 0;
}

void load_skip_patterns(void) {
    if (skip_patterns_loaded) return;
    skip_patterns_loaded = 1;

    const char *skip_ini = resolve_info_path("skip.ini");
    if (!skip_ini) {
        LOG_ERROR(mux_module, "skip.ini not found");
        return;
    }

    FILE *file = fopen(skip_ini, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, skip_ini);
        return;
    }

    free_skip_patterns();

    skip_pattern_list.patterns = NULL;
    skip_pattern_list.count = 0;
    skip_pattern_list.capacity = 0;

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;

        if (skip_pattern_list.count >= skip_pattern_list.capacity) {
            const size_t newcap = skip_pattern_list.capacity ? skip_pattern_list.capacity * 2 : 8;
            char **newptr = realloc(skip_pattern_list.patterns, newcap * sizeof(char *));
            if (!newptr) break;
            skip_pattern_list.patterns = newptr;
            skip_pattern_list.capacity = newcap;
        }

        char *stored = mux_strdup(line);
        for (char *p = stored; *p; p++)
            *p = (char) tolower((unsigned char) *p);

        skip_pattern_list.patterns[skip_pattern_list.count++] = stored;
    }

    fclose(file);
    LOG_INFO(mux_module, "Loaded skip patterns: %zu", skip_pattern_list.count);
}

int should_skip(const char *name, const int is_dir) {
    if (!name || !*name) return 0;

    char name_l[MAX_BUFFER_SIZE];
    snprintf(name_l, sizeof(name_l), "%s", name);
    for (char *p = name_l; *p; p++)
        *p = (char) tolower((unsigned char) *p);

    for (size_t i = 0; i < skip_pattern_list.count; i++) {
        const char *pat = skip_pattern_list.patterns[i];
        if (!pat || !*pat) continue;

        int dir_only = 0;
        if (pat[0] == '/') {
            dir_only = 1;
            pat++;
            if (!*pat) continue;
        }

        if (dir_only && !is_dir) continue;
        if (fnmatch(pat, name_l, 0) == 0) return 1;
    }

    return 0;
}

int str_compare(const void *a, const void *b) {
    const char *str1 = *(const char **) a;
    const char *str2 = *(const char **) b;

    while (*str1 && *str2) {
        const unsigned char c1 = tolower((unsigned char) *str1);
        const unsigned char c2 = tolower((unsigned char) *str2);

        if (isdigit(c1) && isdigit(c2)) {
            unsigned long long n1 = 0, n2 = 0;
            while (isdigit(*str1))
                n1 = n1 * 10 + (*str1++ - '0');
            while (isdigit(*str2))
                n2 = n2 * 10 + (*str2++ - '0');

            if (n1 != n2) return (n1 > n2) - (n1 < n2);
            continue;
        }

        if (c1 != c2) return (c1 > c2) - (c1 < c2);

        str1++;
        str2++;
    }

    return (unsigned char) *str1 - (unsigned char) *str2;
}

int str_startswith(const char *a, const char *b) {
    if (strncmp(a, b, strlen(b)) == 0) return 1;

    return 0;
}

char *str_nonew(char *text) {
    char *newline_pos = strchr(text, '\n');

    if (newline_pos != NULL) *newline_pos = '\0';

    return text;
}

char *str_tolower(const char *text) {
    char *result = mux_strdup(text);
    char *ptr = result;

    while (*ptr) {
        *ptr = tolower((unsigned char) *ptr);
        ptr++;
    }

    return result;
}

char *str_toupper(const char *text) {
    char *result = mux_strdup(text);
    char *ptr = result;

    while (*ptr) {
        *ptr = toupper((unsigned char) *ptr);
        ptr++;
    }

    return result;
}

char *str_remchar(char *text, const char c) {
    const char *r_ptr = text;
    char *w_ptr = text;

    while (*r_ptr != '\0') {
        if (*r_ptr == c) {
            r_ptr++;
            continue;
        }

        *w_ptr = *r_ptr;
        w_ptr++;
        r_ptr++;
    }

    *w_ptr = '\0';
    return text;
}

char *str_remchars(char *text, const char *c) {
    const char *r_ptr = text;
    char *w_ptr = text;

    while (*r_ptr != '\0') {
        const char *d_ptr = c;
        int remove = 0;

        while (*d_ptr != '\0') {
            if (*r_ptr == *d_ptr) {
                remove = 1;
                break;
            }
            d_ptr++;
        }

        if (!remove) {
            *w_ptr = *r_ptr;
            w_ptr++;
        }

        r_ptr++;
    }

    *w_ptr = '\0';
    return text;
}

char *str_trim(char *text) {
    if (!text || !*text) return text;

    while (isspace((unsigned char) (*text)))
        text++;

    if (*text == '\0') return text;

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char) (*end)))
        end--;

    *(end + 1) = '\0';
    return text;
}

char *str_replace(const char *orig, const char *rep, const char *with) {
    if (!orig || !rep) return NULL;

    const size_t len_rep = strlen(rep);
    if (len_rep == 0) return NULL;

    if (!with) with = "";

    const size_t len_with = strlen(with);

    int count = 0;
    const char *ins = orig;
    const char *tmp;
    while ((tmp = strstr(ins, rep))) {
        count++;
        ins = tmp + len_rep;
    }

    if (count == 0) return strdup(orig);

    char *result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    char *out = result;
    ins = orig;

    while (count--) {
        tmp = strstr(ins, rep);
        const size_t len_front = tmp - ins;

        memcpy(out, ins, len_front);
        out += len_front;

        memcpy(out, with, len_with);
        out += len_with;

        ins = tmp + len_rep;
    }

    const size_t tail = strlen(ins);
    memcpy(out, ins, tail + 1);
    return result;
}

int str_replace_segment(
    const char *orig, const char *prefix, const char *suffix, const char *with, char **replacement
) {
    if (!orig || !prefix || !suffix || !replacement) return 0;

    const char *start = strstr(orig, prefix);
    if (!start) return 0;

    start += strlen(prefix);
    const char *end = strstr(start, suffix);
    if (!end) return 0;

    const size_t len_front = start - orig;
    const size_t len_suffix = strlen(end);
    const size_t len_with = strlen(with);
    const size_t total_len = len_front + len_with + len_suffix + 1;

    *replacement = malloc(total_len);
    if (!*replacement) return 0;

    memcpy(*replacement, orig, len_front);
    memcpy(*replacement + len_front, with, len_with);
    memcpy(*replacement + len_front + len_with, end, len_suffix + 1);

    return 1;
}

int str_extract(const char *orig, const char *prefix, const char *suffix, char **extraction) {
    if (!orig || !prefix || !suffix) return 0;

    const char *start = strstr(orig, prefix);
    if (!start) return 0;

    start += strlen(prefix);
    const char *end = strstr(start, suffix);
    if (!end) return 0;

    const size_t len_dynamic = end - start;

    *extraction = malloc(len_dynamic + 1);
    if (!*extraction) return 0;

    memcpy(*extraction, start, len_dynamic);
    (*extraction)[len_dynamic] = '\0';

    return 1;
}

char *str_capital(char *text) {
    if (text && text[0] != '\0') text[0] = toupper((unsigned char) text[0]);
    return text;
}

char *str_capital_all(char *text) {
    int new_word = 1;

    for (char *p = text; p && *p != '\0'; ++p) {
        if (isspace((unsigned char) *p)) {
            new_word = 1;
        } else if (new_word) {
            *p = toupper((unsigned char) *p);
            new_word = 0;
        }
    }

    return text;
}

char *str_rem_first_char(char *text, const int count) {
    static char buffer[PATH_MAX];
    const size_t len = strlen(text);

    if (count <= 0) return text;
    if (count >= (int) len) return "";

    snprintf(buffer, sizeof(buffer), "%s", text + count);

    return buffer;
}

char *str_rem_last_char(char *text, int count) {
    static char buffer[PATH_MAX];
    size_t len = strlen(text);

    if (count >= (int) len) return "";

    snprintf(buffer, sizeof(buffer), "%s", text);

    while (count-- > 0 && len > 0) {
        len--;
        buffer[len] = '\0';
    }

    return buffer;
}

char *get_last_subdir(char *text, const char separator, const int n) {
    char *ptr = text;
    int count = 0;

    while (*ptr && count < n) {
        if (*ptr == separator) count++;
        ptr++;
    }

    if (count < n) {
        return "";
    }

    return ptr;
}

void remove_double_slashes(char *str) {
    const char *src = str;
    char *dst = str;

    while (*src) {
        *dst = *src;

        if (*src == '/' && *(src + 1) == '/') {
            while (*(src + 1) == '/') {
                src++;
            }
        }

        src++;
        dst++;
    }

    *dst = '\0';
}

char *get_last_dir(const char *text) {
    char *last_slash = strrchr(text, '/');

    if (last_slash != NULL) return last_slash + 1;

    return "";
}

char *get_file_name(const char *text) {
    char *last_slash = strrchr(text, '/');

    if (last_slash != NULL) return last_slash + 1;

    return (char *) text;
}

char *get_content_path(char *path) {
    char *directory_path = strip_dir(path);
    if (dir_exist(path)) return directory_path;

    const char *directory_name = get_last_dir(directory_path);

    if (strchr(directory_name, '.') != NULL && strcasecmp(directory_name, get_file_name(path)) == 0)
        return strip_dir(directory_path);
    if (!ends_with(path, ".scummvm") && !ends_with(path, ".m3u") && !ends_with(path, ".cue")
        && !ends_with(path, ".gdi"))
        return directory_path;

    const char *path_no_ext = strip_ext(get_file_name(path));
    return strcasecmp(directory_name, path_no_ext) == 0 ? strip_dir(directory_path) : directory_path;
}

char *strip_dir(const char *text) {
    char *result = mux_strdup(text);
    char *last_slash = strrchr(result, '/');

    if (last_slash != NULL) *last_slash = '\0';

    return result;
}

char *strip_ext(const char *text) {
    char *result = mux_strdup(text);
    char *ext = strrchr(result, '.');

    if (ext != NULL) *ext = '\0';

    return result;
}

char *grab_ext(const char *text) {
    const char *ext = strrchr(text, '.');

    if (ext != NULL && *(ext + 1) != '\0') return mux_strdup(ext + 1);

    return mux_strdup("");
}

void adjust_visual_label(char *text, const int method, const int rep_dash) {
    size_t len = strlen(text);
    size_t text_index = 0;

    int with_bracket = 0;

    char b_open_1 = 0, b_open_2 = 0, b_close_1 = 0, b_close_2 = 0;

    switch (method) {
        case 1:
            b_open_1 = '[';
            b_close_1 = ']';
            break;
        case 2:
            b_open_1 = '(';
            b_close_1 = ')';
            break;
        case 3:
            b_open_1 = '(';
            b_open_2 = '[';
            b_close_1 = ')';
            b_close_2 = ']';
            break;
        default:
            break;
    }

    for (size_t i = 0; i < len; i++) {
        if (text[i] == b_open_1 || (method == 3 && text[i] == b_open_2)) {
            with_bracket = 1;
        } else if (text[i] == b_close_1 || (method == 3 && text[i] == b_close_2)) {
            with_bracket = 0;
        } else if (!with_bracket) {
            text[text_index++] = text[i];
        }
    }

    text[text_index] = '\0';

    size_t start = 0;
    while (isspace((unsigned char) text[start]))
        start++;

    len = strlen(text);
    size_t end = len ? len - 1 : 0;
    while (end < len && isspace((unsigned char) text[end])) {
        if (end == 0) break;
        end--;
    }

    if (start > 0 || end < len - 1) {
        const size_t new_len = end - start + 1;
        memmove(text, text + start, new_len);
        text[new_len] = '\0';
    }

    if (rep_dash) {
        char *found = strstr(text, " - ");
        if (found) {
            found[0] = ':';
            memmove(found + 2, found + 3, strlen(found + 3) + 1);
            found[1] = ' ';
        }
    }
}

char *generate_number_string(
    const int min, const int max, const int increment, const char *prefix, const char *infix, const char *suffix,
    const int infix_position
) {
    size_t buffer_size = 0;

    const size_t prefix_len = prefix ? strlen(prefix) : 0;
    const size_t infix_len = infix ? strlen(infix) : 0;
    const size_t suffix_len = suffix ? strlen(suffix) : 0;

    if (prefix) buffer_size += prefix_len + 1;

    for (int i = min; increment > 0 ? i <= max : i >= max; i += increment) {
        buffer_size += (size_t) snprintf(NULL, 0, "%d", i);
        if (infix) buffer_size += infix_len;
        if (increment > 0 ? i + increment <= max : i + increment >= max) buffer_size += 1;
    }

    if (suffix) buffer_size += suffix_len;

    char *number_string = malloc(buffer_size + 1);
    if (!number_string) return NULL;

    char *ptr = number_string;
    const char *end = number_string + buffer_size + 1;

    if (prefix) ptr += snprintf(ptr, (size_t) (end - ptr), "%s\n", prefix);

    for (int i = min; increment > 0 ? i <= max : i >= max; i += increment) {
        if (infix && infix_position == 0) ptr += snprintf(ptr, (size_t) (end - ptr), "%s", infix);

        ptr += snprintf(ptr, (size_t) (end - ptr), "%d", i);

        if (infix && infix_position == 1) ptr += snprintf(ptr, (size_t) (end - ptr), "%s", infix);
        if ((increment > 0 ? i + increment <= max : i + increment >= max) && ptr < end - 1) *ptr++ = '\n';
    }

    if (suffix) ptr += snprintf(ptr, (size_t) (end - ptr), "%s", suffix);

    *ptr = '\0';

    return number_string;
}

char *generate_time_string(int minute_offset) {
    if (minute_offset <= 0) minute_offset = 15;

    const int slots = 1440 / minute_offset;
    char *buf = malloc((size_t) slots * 6 + 1);
    if (!buf) return NULL;

    char *ptr = buf;
    for (int i = 0; i < slots; i++) {
        const int total = i * minute_offset;
        ptr += snprintf(ptr, 6, "%02d:%02d", total / 60, total % 60);
        if (i < slots - 1) *ptr++ = '\n';
    }

    *ptr = '\0';
    return buf;
}

char **split_command(const char *cmd, size_t *argc_out) {
#define FREE_ARGV                                                                                                      \
    do {                                                                                                               \
        for (size_t i = 0; i < argc; i++)                                                                              \
            free(argv[i]);                                                                                             \
        free(argv);                                                                                                    \
        return NULL;                                                                                                   \
    } while (0)

    if (!cmd || !*cmd) return NULL;

    size_t argc = 0;
    size_t cap = 8;

    char **argv = malloc(cap * sizeof(char *));
    if (!argv) return NULL;

    const char *p = cmd;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p) break;

        char buf[MAX_BUFFER_SIZE];
        size_t len = 0;
        int in_quote = 0;
        char quote_char = 0;

        while (*p && (in_quote || (*p != ' ' && *p != '\t'))) {
            if (*p == '\\') {
                p++;
                if (*p) buf[len++] = *p++;
            } else if (!in_quote && (*p == '\'' || *p == '"')) {
                in_quote = 1;
                quote_char = *p++;
            } else if (in_quote && *p == quote_char) {
                in_quote = 0;
                p++;
            } else {
                buf[len++] = *p++;
            }
        }

        buf[len] = '\0';
        char *arg = malloc(len + 1);

        if (!arg) FREE_ARGV;
        memcpy(arg, buf, len + 1);

        if (argc + 1 >= cap) {
            const size_t new_cap = cap * 2;

            char **tmp = realloc(argv, new_cap * sizeof(char *));
            if (!tmp) FREE_ARGV;

            argv = tmp;
            cap = new_cap;
        }

        argv[argc++] = arg;
    }

    argv[argc] = NULL;
    if (argc_out) *argc_out = argc;

    return argv;
}

void free_array(char **array, const size_t count) {
    if (!array) return;
    for (size_t i = 0; i < count; ++i)
        free(array[i]);
    free(array);
}
