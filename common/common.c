#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../lvgl/lvgl.h"
#include "../font/notosans_medium.h"
#include "../font/notosans_ar_medium.h"
#include "../font/notosans_jp_medium.h"
#include "../font/notosans_kr_medium.h"
#include "../font/notosans_sc_medium.h"
#include "../font/notosans_tc_medium.h"
#include "miniz/miniz.h"
#include "img/nothing.h"
#include "json/json.h"
#include "init.h"
#include "common.h"
#include "ui_common.h"
#include "language.h"
#include "log.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "kiosk.h"
#include "input/list_nav.h"
#include "theme.h"
#include "mini/mini.h"
#include "../module/muxshare.h"

char mux_module[MAX_BUFFER_SIZE];
char mux_dimension[15];
int msgbox_active;
int block_input;
int fe_snd;
int fe_bgm;
struct json translation_generic;
struct json translation_specific;
struct pattern skip_pattern_list = {NULL, 0, 0};
int battery_capacity = 100;
lv_anim_t animation;
lv_obj_t *img_obj;
char **img_paths = NULL;
int img_paths_count = 0;
char **history_items = NULL;
int history_item_count = 0;
char **collection_items = NULL;
int collection_item_count = 0;
char current_wall[MAX_BUFFER_SIZE];
lv_obj_t *wall_img = NULL;
struct grid_info grid_info;
CachedSound sound_cache[SOUND_TOTAL];
int is_silence_playing = 0;
Mix_Music *current_bgm = NULL;
char **bgm_files = NULL;
size_t bgm_file_count = 0;
int bgm_volume = 90;
int current_brightness = 0;
int current_volume = 0;
int is_blank = 0;

char *theme_back_compat[] = {
        config.SYSTEM.VERSION,
        "2508.3_GOLDEN_GOOSE",
        "2508.2_SILLY_GOOSE",
        "2508.1_CANADA_GOOSE",
        "2508.0_GOOSE",
        "2502.0_PIXIE",
        NULL
};

char *disabled_enabled[] = {
        lang.GENERIC.DISABLED,
        lang.GENERIC.ENABLED
};

char *excluded_included[] = {
        lang.GENERIC.EXCLUDED,
        lang.GENERIC.INCLUDED
};

char *allowed_restricted[] = {
        lang.GENERIC.ALLOWED,
        lang.GENERIC.RESTRICTED
};

char *hidden_visible[] = {
        lang.GENERIC.HIDDEN,
        lang.GENERIC.VISIBLE
};

char *show_noicon_hide[] = {
        lang.GENERIC.VISIBLE,
        lang.GENERIC.NOGLYPH,
        lang.GENERIC.HIDDEN,
};

const char *snd_names[SOUND_TOTAL] = {
        "confirm", "back", "keypress", "navigate",
        "error", "muos", "reboot", "shutdown",
        "startup", "info_open", "info_close", "option"
};

int file_exist(char *filename) {
    return access(filename, F_OK) == 0;
}

int directory_exist(char *dirname) {
    struct stat stats;
    return stat(dirname, &stats) == 0 && S_ISDIR(stats.st_mode);
}

const char **build_term_exec(const char **term_cmd, size_t *term_cnt) {
    size_t arg_count = 0;
    for (const char **p = term_cmd; p && *p; p++) arg_count++;

    size_t total_args = 16 + arg_count + 1;
    const char **exec = malloc(sizeof(char *) * total_args);
    if (!exec) return NULL;

    size_t i = 0;
    exec[i++] = (OPT_PATH "frontend/muterm");
    exec[i++] = "-s";
    exec[i++] = (char *) theme.TERMINAL.FONT_SIZE;

    static char font_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("font", "ttf", font_path, sizeof(font_path))) {
        exec[i++] = "-f";
        exec[i++] = font_path;
    }

    static char image_path[MAX_BUFFER_SIZE];
    if (load_terminal_resource("image", "png", image_path, sizeof(image_path))) {
        exec[i++] = "-i";
        exec[i++] = image_path;
    }

    exec[i++] = "-bg";
    exec[i++] = (char *) theme.TERMINAL.BACKGROUND;
    exec[i++] = "-fg";
    exec[i++] = (char *) theme.TERMINAL.FOREGROUND;

    for (const char **p = term_cmd; p && *p; p++) exec[i++] = *p;

    exec[i] = NULL;
    if (term_cnt) *term_cnt = i;
    return exec;
}

void extract_archive(char *filename, char *screen) {
    size_t exec_count;
    const char *args[] = {(OPT_PATH "script/mux/extract.sh"), filename, screen, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);
}

void update_bootlogo(char *next_screen) {
    size_t exec_count;
    const char *args[] = {(OPT_PATH "script/package/theme.sh"), "bootlogo", next_screen, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0, 1, NULL, NULL);
    }
    free(exec);
}

int str_compare(const void *a, const void *b) {
    const char *str1 = *(const char **) a;
    const char *str2 = *(const char **) b;

    while (*str1 && *str2) {
        unsigned char c1 = tolower((unsigned char) *str1);
        unsigned char c2 = tolower((unsigned char) *str2);

        if (isdigit(c1) && isdigit(c2)) {
            unsigned long long n1 = 0, n2 = 0;
            while (isdigit(*str1)) n1 = n1 * 10 + (*str1++ - '0');
            while (isdigit(*str2)) n2 = n2 * 10 + (*str2++ - '0');

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

char *str_tolower(char *text) {
    char *result = strdup(text);
    char *ptr = result;

    while (*ptr) {
        *ptr = tolower((unsigned char) *ptr);
        ptr++;
    }

    return result;
}

char *str_toupper(char *text) {
    char *result = strdup(text);
    char *ptr = result;

    while (*ptr) {
        *ptr = toupper((unsigned char) *ptr);
        ptr++;
    }

    return result;
}

char *str_remchar(char *text, char c) {
    char *r_ptr = text;
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

char *str_remchars(char *text, char *c) {
    char *r_ptr = text;
    char *w_ptr = text;

    while (*r_ptr != '\0') {
        char *d_ptr = c;
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

void str_split(char *text, char sep, char *p1, char *p2) {
    const char *pos = strchr(text, sep);

    if (pos) {
        size_t len = pos - text;
        strncpy(p1, text, len);
        p1[len] = '\0';
        strcpy(p2, pos + 1);
    } else {
        strcpy(p1, text);
        p2[0] = '\0';
    }
}

char *str_trim(char *text) {
    if (!text || !*text) return text;

    while (isspace((unsigned char) (*text))) text++;

    if (*text == '\0') return text;

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char) (*end))) end--;

    *(end + 1) = '\0';
    return text;
}

char *str_replace(const char *orig, const char *rep, const char *with) {
    char *result;
    const char *ins;
    char *tmp;
    size_t len_rep;
    size_t len_with;
    size_t len_front;
    int count;

    if (!orig || !rep) return NULL;

    len_rep = strlen(rep);

    if (len_rep == 0) return NULL;

    if (!with) with = "";

    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) ins = tmp + len_rep;
    tmp = result = (char *) malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }

    strcpy(tmp, orig);
    return result;
}

int str_replace_segment(const char *orig, const char *prefix, const char *suffix,
                        const char *with, char **replacement) {
    const char *start, *end;
    size_t len_front, len_with, len_suffix, total_len;

    if (!orig || !prefix || !suffix || !replacement) return 0;

    start = strstr(orig, prefix);
    if (!start) return 0;

    start += strlen(prefix);
    end = strstr(start, suffix);
    if (!end) return 0;

    len_front = start - orig;
    len_suffix = strlen(end);
    len_with = strlen(with);
    total_len = len_front + len_with + len_suffix + 1;

    *replacement = (char *) malloc(total_len);
    if (!*replacement) return 0;

    strncpy(*replacement, orig, len_front);
    strcpy(*replacement + len_front, with);
    strcpy(*replacement + len_front + len_with, end);

    return 1;
}

int str_extract(const char *orig, const char *prefix, const char *suffix, char **extraction) {
    const char *start, *end;
    size_t len_dynamic;

    if (!orig || !prefix || !suffix) return 0;

    start = strstr(orig, prefix);
    if (!start) return 0;

    start += strlen(prefix);
    end = strstr(start, suffix);
    if (!end) return 0;

    len_dynamic = end - start;

    *extraction = (char *) malloc(len_dynamic + 1);
    if (!*extraction) return 0;

    strncpy(*extraction, start, len_dynamic);
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

char *str_rem_last_char(char *text, int count) {
    static char buffer[PATH_MAX];
    size_t len = strlen(text);

    if (count >= (int) len) return "";

    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    while (count-- > 0 && len > 0) {
        len--;
        buffer[len] = '\0';
    }

    return buffer;
}

char *get_last_subdir(char *text, char separator, int n) {
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
    char *src = str;
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

char *get_last_dir(char *text) {
    char *last_slash = strrchr(text, '/');

    if (last_slash != NULL) return last_slash + 1;

    return "";
}

char *strip_dir(char *text) {
    char *result = strdup(text);
    char *last_slash = strrchr(result, '/');

    if (last_slash != NULL) *last_slash = '\0';

    return result;
}

char *strip_ext(char *text) {
    char *result = strdup(text);
    char *ext = strrchr(result, '.');

    if (ext != NULL) *ext = '\0';

    return result;
}

char *grab_ext(char *text) {
    char *ext = strrchr(text, '.');

    if (ext != NULL && *(ext + 1) != '\0') return strdup(ext + 1);

    return strdup("");
}

char *get_execute_result(const char *command) {
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run: %s\n", command);
        return NULL;
    }

    static char result[MAX_BUFFER_SIZE];
    fgets(result, MAX_BUFFER_SIZE, fp);
    pclose(fp);

    char *newline = strchr(result, '\n');
    if (newline != NULL) *newline = '\0';

    return result;
}

int read_battery_capacity(void) {
    FILE *file = fopen(device.BATTERY.CAPACITY, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, device.BATTERY.CAPACITY)
        return 0;
    }

    char buf[32];
    if (!fgets(buf, sizeof(buf), file)) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_READ, device.BATTERY.CAPACITY)
        fclose(file);
        return 0;
    }

    fclose(file);

    char *end_ptr;
    long capacity = strtol(buf, &end_ptr, 10);

    int invalid_input = (end_ptr == buf);
    int trailing_garbage = (*end_ptr != '\0' && *end_ptr != '\n');

    if (invalid_input || trailing_garbage) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_READ, device.BATTERY.CAPACITY)
        return 0;
    }

    capacity += (config.SETTINGS.ADVANCED.OFFSET - 50);
    if (capacity > 100) capacity = 100;
    if (capacity < 0) capacity = 0;
    return (int) capacity;
}

char *read_battery_voltage(void) {
    FILE *file = fopen(device.BATTERY.VOLTAGE, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, device.BATTERY.VOLTAGE)
        return "0.00 V";
    }

    char buf[32];
    if (!fgets(buf, sizeof(buf), file)) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_READ, device.BATTERY.VOLTAGE)
        fclose(file);
        return "0.00 V";
    }

    fclose(file);

    char *end_ptr;
    long raw_voltage = strtol(buf, &end_ptr, 10);
    if (end_ptr == buf) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_READ, device.BATTERY.VOLTAGE)
        return "0.00 V";
    }

    char *form_voltage = malloc(10);
    if (!form_voltage) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        return "0.00 V";
    }

    snprintf(form_voltage, 10, "%.2f V", (double) raw_voltage / 1000000.0);
    return form_voltage;
}

char *read_all_char_from(const char *filename) {
    char *text = NULL;
    FILE *file = fopen(filename, "r");

    if (file == NULL) return "";

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    text = (char *) malloc(fileSize + 1);

    if (text != NULL) {
        size_t bytesRead = fread(text, 1, fileSize, file);

        if (bytesRead > 0 && text[bytesRead - 1] == '\n') {
            text[bytesRead - 1] = '\0';
        } else {
            text[bytesRead] = '\0';
        }
    } else {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
    }

    fclose(file);
    return text;
}

char *read_line_char_from(const char *filename, size_t line_number) {
    if (!filename || line_number == 0) {
        fprintf(stderr, "Invalid filename or line number.\n");
        return "";
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, filename)
        return "";
    }

    char *line = (char *) malloc(MAX_BUFFER_SIZE);
    if (!line) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        fclose(file);
        return "";
    }

    size_t current_line = 0;
    while (fgets(line, MAX_BUFFER_SIZE, file) != NULL) {
        current_line++;
        if (current_line == line_number) {
            size_t length = strlen(line);

            if (length > 0 && line[length - 1] == '\n') line[length - 1] = '\0';

            fclose(file);
            return line;
        }
    }

    free(line);
    fclose(file);

    return "";
}

int read_all_int_from(const char *filename, size_t buffer) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char line[buffer];
    size_t buf_size = sizeof(line);
    if (buf_size > INT_MAX) buf_size = INT_MAX;

    if (!fgets(line, (int) buf_size, file)) {
        fclose(file);
        return 0;
    }

    fclose(file);
    long value = strtol(line, NULL, 10);
    return (value > INT_MAX || value < INT_MIN) ? 0 : (int) value;
}

int read_line_int_from(const char *filename, size_t line_number) {
    char line[MAX_BUFFER_SIZE];
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    for (size_t i = 1; i <= line_number && fgets(line, sizeof(line), file); i++) {
        if (i == line_number) {
            line[strcspn(line, "\n")] = '\0';
            errno = 0;
            long value = strtol(line, NULL, 10);
            fclose(file);
            return (errno == ERANGE) ? 0 : (int) value;
        }
    }

    fclose(file);
    return 0;
}

unsigned long long read_all_long_from(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    char buf[64];
    if (!fgets(buf, sizeof(buf), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    char *end_ptr;
    errno = 0;

    unsigned long long value = strtoull(buf, &end_ptr, 10);
    if (errno != 0 || end_ptr == buf || (*end_ptr && *end_ptr != '\n')) return 0;

    return value;
}

const char *get_random_hex(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    unsigned char r = random() % 256;
    unsigned char g = random() % 256;
    unsigned char b = random() % 256;

    char *hex = malloc(8);
    if (!hex) return NULL;
    snprintf(hex, 8, "%02X%02X%02X", r, g, b);

    return hex;
}

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key, uint32_t default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    uint32_t result;
    if (strcmp(meta, "NOT FOUND") == 0) {
        result = default_value;
    } else {
        result = (uint32_t)
                strtoul(meta, NULL, 16);
    }

    return result;
}

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, int16_t default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    int16_t result;
    if (strcmp(meta, "NOT FOUND") == 0) {
        result = default_value;
    } else {
        result = (int16_t)
                strtol(meta, NULL, 10);
    }

    return result;
}

char *get_ini_string(mini_t *ini_config, const char *section, const char *key, char *default_value) {
    static char meta[MAX_BUFFER_SIZE];
    const char *result = mini_get_string(ini_config, section, key, default_value);

    strncpy(meta, result, MAX_BUFFER_SIZE - 1);
    meta[MAX_BUFFER_SIZE - 1] = '\0';

    return meta;
}

void write_text_to_file(const char *filename, const char *mode, int type, ...) {
    FILE *file = fopen(filename, mode);

    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_WRITE, filename)
        return;
    }

    va_list args;
    va_start(args, type);

    if (type == CHAR) { // type is general text!
        fprintf(file, "%s", va_arg(args,
                                   const char *));
    } else if (type == INT) { // type is a number!
        fprintf(file, "%d", va_arg(args,
                                   int));
    }

    va_end(args);
    fclose(file);
}

void create_directories(const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) return;

    char *path_copy = strdup(path);
    char *slash = strrchr(path_copy, '/');
    if (slash != NULL) {
        *slash = '\0';
        create_directories(path_copy);
        *slash = '/';
        // recursive bullshit
    }

    if (mkdir(path_copy, 0777) == -1) free(path_copy);
}

void show_info_box(char *title, char *content, int is_content) {
    if (msgbox_active == 0) {
        lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);

        if (is_content) {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }

        msgbox_active = 1;
        msgbox_element = ui_pnlHelp;

        lv_label_set_text(ui_lblHelpHeader, title);
        lv_label_set_text(ui_lblHelpContent, content);

        if (is_content) lv_label_set_text(ui_lblHelpPreviewHeader, title);

        lv_obj_t *ui_pnlItem = lv_obj_get_parent(ui_lblHelpContent);
        lv_obj_scroll_to_y(ui_pnlItem, 0, LV_ANIM_OFF);
    }
}

void nav_move(lv_group_t *group, int direction) {
    (direction < 0 ? nav_prev : nav_next)(group, 1);
}

void nav_prev(lv_group_t *group, int count) {
    for (int i = 0; i < count; i++) lv_group_focus_prev(group);
}

void nav_next(lv_group_t *group, int count) {
    for (int i = 0; i < count; i++) lv_group_focus_next(group);
}

char *get_datetime(void) {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    static char datetime_str[MAX_BUFFER_SIZE];

    strftime(datetime_str, sizeof(datetime_str), config.CLOCK.NOTATION ? TIME_STRING_24 : TIME_STRING_12, time_info);
    return datetime_str;
}

void datetime_task(lv_timer_t *timer) {
    struct dt_task_param *dt_par = timer->user_data;
    lv_label_set_text(dt_par->lblDatetime, get_datetime());
}

char *get_capacity(void) {
    static char capacity[MAX_BUFFER_SIZE];
    const char *prefix = read_line_int_from(device.BATTERY.CHARGER, 1)
                         ? "capacity_charging_"
                         : "capacity_";

    int level = battery_capacity;
    if (level < 0) level = 0;
    if (level > 100) level = 100;

    // Round down to nearest multiple of 10
    int rounded = (level / 10) * 10;

    snprintf(capacity, sizeof(capacity), "%s%d", prefix, rounded);
    return capacity;
}

void capacity_task(void) {
    battery_capacity = read_battery_capacity();
    update_battery_capacity(ui_staCapacity, &theme);
}

void increase_option_value(lv_obj_t *element) {
    uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;
    uint16_t current = lv_dropdown_get_selected(element);

    play_sound(SND_OPTION);

    if (current < (total - 1)) {
        current++;
        lv_dropdown_set_selected(element, current);
    } else {
        current = 0;
        lv_dropdown_set_selected(element, current);
    }
}

void decrease_option_value(lv_obj_t *element) {
    uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;
    uint16_t current = lv_dropdown_get_selected(element);

    play_sound(SND_OPTION);

    if (current > 0) {
        current--;
        lv_dropdown_set_selected(element, current);
    } else {
        current = (total - 1);
        lv_dropdown_set_selected(element, current);
    }
}

void load_assign(const char *loader, const char *rom, const char *dir, const char *sys, int forced, int app) {
    FILE *file = fopen(loader, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, loader)
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d\n%d", rom, dir, sys, forced, app);
    fclose(file);
}

void load_mux(const char *value) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, MUOS_ACT_LOAD)
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void play_sound(int sound) {
    if (!fe_snd || sound < 0 || sound >= SOUND_TOTAL) return;

    CachedSound *cs = &sound_cache[sound];
    if (cs->chunk) {
        Mix_PlayChannel(-1, cs->chunk, 0);
    } else {
        LOG_ERROR("sound", "Sound not found or cached: %s.wav", snd_names[sound])
    }
}

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], int recursive) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                size_t len = strlen(extension);
                size_t name_len = strlen(entry->d_name);

                if (name_len > len &&
                    strcasecmp(entry->d_name + name_len - len, extension) == 0) {

                    char file_path[PATH_MAX];
                    snprintf(file_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

                    int is_exception = 0;
                    if (exception != NULL) {
                        for (int i = 0; exception[i] != NULL; ++i) {
                            if (strcmp(entry->d_name, exception[i]) == 0) {
                                is_exception = 1;
                                break;
                            }
                        }
                    }

                    if (!is_exception) {
                        if (remove(file_path) != 0) {
                            perror(lang.SYSTEM.FAIL_DELETE_FILE);
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
                       strcmp(entry->d_name, "..") != 0) {
                if (recursive) {
                    char sub_dir_path[PATH_MAX];
                    snprintf(sub_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
                    delete_files_of_type(sub_dir_path, extension, exception, recursive);
                }
            }
        }

        closedir(dir);
    } else {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
    }
}

void delete_files_of_name(const char *dir_path, const char *filename) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char full_path[PATH_MAX];
            snprintf(full_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            if (entry->d_type == DT_DIR) {
                delete_files_of_name(full_path, filename);
            } else if (entry->d_type == DT_REG && strcmp(entry->d_name, filename) == 0) {
                if (remove(full_path) != 0) {
                    perror(lang.SYSTEM.FAIL_DELETE_FILE);
                }
            }
        }
        closedir(dir);
    } else {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
    }
}

int load_element_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                                 const char *image_type, const char *element, const char *element_fallback,
                                 const char *image_extension, char *image_path, size_t path_size) {
    const char *theme = theme_compat() ? theme_base : INTERNAL_THEME;

    const char *paths[] = {
            "%s/%simage/%s/%s/%s/%s.%s",
            "%s/%simage/%s/%s/%s.%s"
    };
    const char *dimensions[] = {mux_dimension, ""};
    const char *elements[] = {element, element_fallback};

    for (size_t i = 0; i < A_SIZE(dimensions); ++i) {
        for (size_t j = 0; j < A_SIZE(paths); ++j) {
            for (size_t k = 0; k < A_SIZE(elements); ++k) {
                int written;

                switch (j) {
                    case 0:
                        written = snprintf(image_path, path_size, paths[j], theme, dimensions[i],
                                           config.SETTINGS.GENERAL.LANGUAGE, image_type, program, elements[k],
                                           image_extension);
                        break;
                    case 1:
                    default:
                        written = snprintf(image_path, path_size, paths[j], theme, dimensions[i],
                                           image_type, program, elements[k], image_extension);
                        break;
                }

                if (written >= 0 && file_exist(image_path)) return 1;
            }
        }
    }

    return 0;
}

int load_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                         const char *image_type, const char *image_extension, char *image_path, size_t path_size) {
    const char *theme = theme_compat() ? theme_base : INTERNAL_THEME;

    const char *paths[] = {
            "%s/%simage/%s.%s",
            "%s/%simage/%s/%s/%s.%s",
            "%s/%simage/%s/%s.%s",
            "%s/%simage/%s/%s/default.%s",
            "%s/%simage/%s/default.%s"
    };

    for (size_t i = 0; i < A_SIZE(paths); ++i) {
        int written;

        switch (i) {
            case 0:
                written = snprintf(image_path, path_size, paths[i], theme, mux_dimension, image_type, image_extension);
                break;
            case 1:
                written = snprintf(image_path, path_size, paths[i], theme, mux_dimension,
                                   config.SETTINGS.GENERAL.LANGUAGE, image_type, program, image_extension);
                break;
            case 2:
                written = snprintf(image_path, path_size, paths[i], theme, mux_dimension, image_type, program,
                                   image_extension);
                break;
            case 3:
                written = snprintf(image_path, path_size, paths[i], theme, mux_dimension,
                                   config.SETTINGS.GENERAL.LANGUAGE, image_type, image_extension);
                break;
            case 4:
            default:
                written = snprintf(image_path, path_size, paths[i], theme, mux_dimension, image_type, image_extension);
                break;
        }

        if (written >= 0 && file_exist(image_path)) return 1;
    }

    return 0;
}

void load_splash_image_fallback(const char *mux_dimension, char *image, size_t image_size) {
    if (snprintf(image, image_size, "%s/splash.png", INFO_CAT_PATH) >= 0 && file_exist(image)) return;

    const char *theme = theme_compat() ? STORAGE_THEME : INTERNAL_THEME;
    if (snprintf(image, image_size, "%s/%simage/splash.png", theme, mux_dimension) >= 0 && file_exist(image)) return;

    snprintf(image, image_size, "%s/image/splash.png", theme);
}

bool is_supported_theme_catalogue(const char *catalogue_name, const char *image_type) {
    return (strcmp(catalogue_name, "Application") == 0 && strcmp(image_type, "box") == 0) ||
           (strcmp(catalogue_name, "Application") == 0 && strcmp(image_type, "grid") == 0) ||
           (strcmp(catalogue_name, "Collection") == 0 && strcmp(image_type, "box") == 0) ||
           (strcmp(catalogue_name, "Collection") == 0 && strcmp(image_type, "grid") == 0) ||
           (strcmp(catalogue_name, "Folder") == 0 && strcmp(image_type, "box") == 0) ||
           (strcmp(catalogue_name, "Folder") == 0 && strcmp(image_type, "grid") == 0);
}

int load_image_catalogue(const char *catalogue_name, const char *program, const char *program_alt,
                         const char *program_default, const char *mux_dimension, const char *image_type,
                         char *image_path, size_t path_size) {
    enum catalogue_kind {
        CAT_THEME, CAT_INFO
    };

    const char *path_format = "%s/%s/%s/%s%s.png";
    const bool skip_theme_catalogue =
            !directory_exist(THEME_CAT_PATH) || !is_supported_theme_catalogue(catalogue_name, image_type);

    struct {
        enum catalogue_kind kind;
        const char *catalogue_path;
        const char *dimension;
        const char *program;
    } args[] = {
            {CAT_THEME, THEME_CAT_PATH, mux_dimension, program},
            {CAT_THEME, THEME_CAT_PATH, mux_dimension, program_alt},
            {CAT_THEME, THEME_CAT_PATH, "",            program},
            {CAT_THEME, THEME_CAT_PATH, "",            program_alt},
            {CAT_INFO,  INFO_CAT_PATH,  mux_dimension, program},
            {CAT_INFO,  INFO_CAT_PATH,  mux_dimension, program_alt},
            {CAT_INFO,  INFO_CAT_PATH,  "",            program},
            {CAT_INFO,  INFO_CAT_PATH,  "",            program_alt},
            {CAT_THEME, THEME_CAT_PATH, mux_dimension, program_default},
            {CAT_THEME, THEME_CAT_PATH, "",            program_default},
            {CAT_INFO,  INFO_CAT_PATH,  mux_dimension, program_default},
            {CAT_INFO,  INFO_CAT_PATH,  "",            program_default},
    };

    for (size_t i = 0; i < A_SIZE(args); i++) {
        if ((args[i].kind == CAT_THEME && skip_theme_catalogue) ||
            args[i].program[0] == '\0') {
            continue;
        }

        int written;
        written = snprintf(image_path, path_size, path_format, args[i].catalogue_path, catalogue_name,
                           image_type, args[i].dimension, args[i].program);
        if (written >= 0 && file_exist(image_path)) return 1;
    }

    return 0;
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated, int random, int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    const char *wall_extension = random ? "0.png" : (animated == 1 ? "gif" : (animated == 2 ? "0.png" : "png"));

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
        const char *element = lv_obj_get_user_data(element_focused);
        switch (wall_type) {
            case APPLICATION:
                if (load_image_catalogue("Application", element, "", "default", mux_dimension, "wall",
                                         wall_image_path, sizeof(wall_image_path))) {
                    int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                    if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
                    return wall_image_embed;
                }
                break;
            case ARCHIVE:
                if (load_image_catalogue("Archive", element, "", "default", mux_dimension, "wall",
                                         wall_image_path, sizeof(wall_image_path))) {
                    int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                    if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
                    return wall_image_embed;
                }
                break;
            case TASK:
                if (load_image_catalogue("Task", element, "", "default", mux_dimension, "wall",
                                         wall_image_path, sizeof(wall_image_path))) {
                    int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                    if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
                    return wall_image_embed;
                }
                break;
            case GENERAL:
            default:
                break;
        }
        if (load_element_image_specifics(STORAGE_THEME, mux_dimension, program, "wall",
                                         strcmp(program, "muxlaunch") == 0 ? element : "default",
                                         "default", wall_extension, wall_image_path, sizeof(wall_image_path))) {
            int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
            if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
            return wall_image_embed;
        }
    }

    if (load_image_specifics(STORAGE_THEME, mux_dimension, program, "wall",
                             wall_extension, wall_image_path, sizeof(wall_image_path)) ||
        load_image_specifics(STORAGE_THEME, "", program, "wall",
                             wall_extension, wall_image_path, sizeof(wall_image_path))) {
        int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
        if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
        return wall_image_embed;
    }

    return "";
}

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall,
                    lv_obj_t *ui_imgWall, int wall_type) {
    static char new_wall[MAX_BUFFER_SIZE];
    snprintf(new_wall, sizeof(new_wall), "%s", get_wallpaper_path(
            ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND, wall_type));

    if (strcasecmp(new_wall, current_wall) != 0) {
        snprintf(current_wall, sizeof(current_wall), "%s", new_wall);
        if (strlen(new_wall) > 3) {
            if (theme.MISC.RANDOM_BACKGROUND) {
                load_image_random(ui_imgWall, new_wall);
            } else {
                switch (theme.MISC.ANIMATED_BACKGROUND) {
                    case 1:
                        wall_img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(wall_img, new_wall);
                        break;
                    case 2:
                        load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY,
                                             theme.ANIMATION.ANIMATION_REPEAT > 0 ? theme.ANIMATION.ANIMATION_REPEAT
                                                                                  : LV_ANIM_REPEAT_INFINITE,
                                             new_wall);
                        break;
                    default:
                        lv_img_set_src(ui_imgWall, new_wall);
                        break;
                }
            }
        } else {
            unload_image_animation();
            lv_img_set_src(ui_imgWall, &ui_image_Nothing);
        }
    }
}

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        const char *element = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        switch (wall_type) {
            case APPLICATION:
                if (grid_mode_enabled && config.VISUAL.BOX_ART_HIDE) {
                    return "";
                }
                if (load_image_catalogue("Application", element, "", "default", mux_dimension, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case ARCHIVE:
                if (load_image_catalogue("Archive", element, "", "default", mux_dimension, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case TASK:
                if (load_image_catalogue("Task", element, "", "default", mux_dimension, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case GENERAL:
            default:
                if (load_element_image_specifics(STORAGE_THEME, mux_dimension, program, "static",
                                                 strcmp(program, "muxlaunch") == 0 ? element : "default",
                                                 "default", "png", static_image_path,
                                                 sizeof(static_image_path))) {

                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
        }
    }

    return "";
}

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    switch (config.VISUAL.OVERLAY_IMAGE) {
        case 0:
            return;
        case 1:
            if (load_image_specifics(STORAGE_THEME, mux_dimension, program, "overlay", "png",
                                     static_image_path, sizeof(static_image_path)) ||
                load_image_specifics(STORAGE_THEME, "", program, "overlay", "png",
                                     static_image_path, sizeof(static_image_path))) {

                int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            }
            break;
        default:
            snprintf(static_image_path, sizeof(static_image_path), "%s/%s%d.png",
                     INTERNAL_OVERLAY, mux_dimension, config.VISUAL.OVERLAY_IMAGE);
            if (!file_exist(static_image_path)) {
                snprintf(static_image_path, sizeof(static_image_path), "%s/standard/%d.png",
                         INTERNAL_OVERLAY, config.VISUAL.OVERLAY_IMAGE);
            }
            int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
            if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            break;
    }

    if (file_exist(static_image_path)) {
        lv_img_set_src(overlay_image, static_image_embed);
        lv_obj_set_style_img_opa(overlay_image, config.VISUAL.OVERLAY_TRANSPARENCY, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_foreground(overlay_image);
    }
}

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (load_image_specifics(STORAGE_THEME, mux_dimension, program, "kiosk", "png",
                             static_image_path, sizeof(static_image_path)) ||
        load_image_specifics(STORAGE_THEME, "", program, "kiosk", "png",
                             static_image_path, sizeof(static_image_path))) {

        int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
        if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;

        lv_img_set_src(kiosk_image, static_image_embed);
        lv_obj_move_foreground(kiosk_image);
    }
}

int load_terminal_resource(const char *resource, const char *extension, char *buffer, size_t size) {
    const char *dimensions[] = {mux_dimension, ""};
    const char *theme = theme_compat() ? STORAGE_THEME : INTERNAL_THEME;

    for (size_t i = 0; i < 2; i++) {
        snprintf(buffer, size, "%s/%s%s/muterm.%s", theme, dimensions[i], resource, extension);
        if (file_exist(buffer)) return 1;
    }

    return 0;
}

static void image_anim_cb(void *var, int32_t img_idx) {
    lv_img_set_src(img_obj, img_paths[img_idx]);
}

void build_image_array(char *base_image_path) {
    char base_path[PATH_MAX];
    char path[PATH_MAX];
    size_t base_len = strlen(base_image_path) - 6;

    if (base_len >= PATH_MAX) {
        LOG_ERROR("image", "Base path exceeds maximum allowed length: %s", base_image_path)
        return;
    }

    strncpy(base_path, base_image_path, base_len);
    base_path[base_len] = '\0';

    int index = 0;
    int file_exists = 1;

    while (file_exists) {
        snprintf(path, sizeof(path), "%s.%d.png", base_path + 2, index);
        file_exists = file_exist(path);

        if (file_exists) {
            size_t needed_size = snprintf(NULL, 0, "%s.%d.png", base_path, index) + 1;
            char *path_embed = malloc(needed_size);
            if (!path_embed) {
                LOG_ERROR("image", "Failed to allocate memory for image: %s.%d.png", base_path, index)
                break;
            }

            snprintf(path_embed, needed_size, "%s.%d.png", base_path, index);

            char **img_temp = realloc(img_paths, (img_paths_count + 1) * sizeof(char *));
            if (!img_temp) {
                LOG_ERROR("image", "Failed to reallocate image path array")
                free(path_embed);
                break;
            }

            img_paths = img_temp;
            img_paths[img_paths_count] = path_embed;
            img_paths_count++;
        }

        index++;
    }
}

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path) {
    printf("Load Image Random: %s\n", base_image_path);
    img_paths_count = 0;
    build_image_array(base_image_path);

    img_obj = ui_imgWall;

    if (img_paths_count > 0) {
        lv_img_set_src(ui_imgWall, img_paths[random() % img_paths_count]);
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }
}

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, int repeat_count, char *base_image_path) {
    printf("Load Image Animation: %s\n", base_image_path);
    img_paths_count = 0;
    build_image_array(base_image_path);

    img_obj = ui_imgWall;

    if (img_paths_count > 1 && config.VISUAL.BACKGROUNDANIMATION) {
        lv_obj_center(img_obj);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, img_obj);
        lv_anim_set_values(&animation, 0, img_paths_count - 1);
        lv_anim_set_exec_cb(&animation, (lv_anim_exec_xcb_t) image_anim_cb);
        lv_anim_set_time(&animation, animation_time * img_paths_count);
        lv_anim_set_repeat_count(&animation, repeat_count);

        lv_anim_start(&animation);
    } else {
        image_anim_cb(NULL, 0);
    }
}

void unload_image_animation(void) {
    if (lv_obj_is_valid(wall_img)) lv_obj_del(wall_img);
    if (lv_obj_is_valid(img_obj)) lv_anim_del(img_obj, NULL);
}

int get_font_size(void) {
    if (device.MUX.WIDTH == 1280) {
        return 30;
    } else if (device.MUX.WIDTH == 1024) {
        return 32;
    } else {
        return 20;
    }
}

lv_font_t *get_language_font(void) {
    int font_size = get_font_size();
    size_t cache_size = 1024 * 10;
    lv_font_t * font;
    if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Chinese (Simplified)") == 0) {
        font = lv_tiny_ttf_create_data_ex(&notosans_sc_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Chinese (Traditional)") == 0) {
        font = lv_tiny_ttf_create_data_ex(&notosans_tc_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Japanese") == 0) {
        font = lv_tiny_ttf_create_data_ex(&notosans_jp_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Arabic") == 0) {
        font = lv_tiny_ttf_create_data_ex(&notosans_ar_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Korean") == 0) {
        font = lv_tiny_ttf_create_data_ex(&notosans_kr_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    } else {
        font = lv_tiny_ttf_create_data_ex(&notosans_medium_ttf, notosans_medium_ttf_len, font_size, cache_size);
    }
    return font;
}

void load_font_text_from_file(const char *filepath, lv_obj_t *element) {
    char theme_font_text_fs[MAX_BUFFER_SIZE];
    snprintf(theme_font_text_fs, sizeof(theme_font_text_fs), "M:%s", filepath);
    lv_font_t * font = lv_font_load(theme_font_text_fs);
    font->fallback = get_language_font();
    lv_obj_set_style_text_font(element, font, MU_OBJ_MAIN_DEFAULT);
}

void load_font_text(lv_obj_t *screen) {
    lv_font_t * language_font = get_language_font();

    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_text_default[MAX_BUFFER_SIZE];
        char theme_font_text[MAX_BUFFER_SIZE];

        char *dimensions[15] = {mux_dimension, ""};
        char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;

        if (theme_compat()) {
            for (int i = 0; i < 2; i++) {
                if ((snprintf(theme_font_text, sizeof(theme_font_text),
                              "%s/%sfont/%s/%s.bin", theme_location, dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, mux_module) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text_default),
                              "%s/%sfont/%s/default.bin", theme_location, dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text),
                              "%s/%sfont/%s.bin", theme_location, dimensions[i], mux_module) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text_default),
                              "%s/%sfont/default.bin", theme_location, dimensions[i]) >= 0 &&
                     file_exist(theme_font_text))) {

                    LOG_INFO(mux_module, "Loading Main Theme Font: %s", theme_font_text)
                    load_font_text_from_file(theme_font_text, screen);
                    return;
                }
            }
        }
    }

    LOG_INFO(mux_module, "Loading Default Language Font")
    lv_obj_set_style_text_font(screen, language_font, MU_OBJ_MAIN_DEFAULT);
}

void load_font_section(const char *section, lv_obj_t *element) {
    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_section[MAX_BUFFER_SIZE];

        char *dimensions[15] = {mux_dimension, ""};
        char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;

        if (theme_compat()) {
            for (int i = 0; i < 2; i++) {
                if ((snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s/%s.bin", theme_location, dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, section, mux_module) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s/default.bin", theme_location, dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, section) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s.bin", theme_location, dimensions[i], section, mux_module) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/default.bin", theme_location, dimensions[i], section) >= 0 &&
                     file_exist(theme_font_section))) {

                    LOG_INFO(mux_module, "Loading Section '%s' Font: %s", section, theme_font_section)
                    load_font_text_from_file(theme_font_section, element);
                    return;
                }
            }
        }
    }
}

int is_network_connected(void) {
    if (file_exist(device.NETWORK.STATE)) {
        if (strcasecmp("up", read_all_char_from(device.NETWORK.STATE)) == 0) return 1;
    }

    return 0;
}

void process_visual_element(enum visual_type visual, lv_obj_t *element) {
    switch (visual) {
        case CLOCK:
            if (!config.VISUAL.CLOCK) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case BLUETOOTH:
            if (!config.VISUAL.BLUETOOTH) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case NETWORK:
            if (!config.VISUAL.NETWORK) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case BATTERY:
            if (!config.VISUAL.BATTERY) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

static void free_skip_patterns(void) {
    for (size_t i = 0; i < skip_pattern_list.count; i++) free(skip_pattern_list.patterns[i]);
    free(skip_pattern_list.patterns);
    skip_pattern_list.patterns = NULL;
    skip_pattern_list.count = 0;
    skip_pattern_list.capacity = 0;
}

void load_skip_patterns(void) {
    char skip_ini[MAX_BUFFER_SIZE];
    int written = snprintf(skip_ini, sizeof(skip_ini), "%s/%s/skip.ini",
                           device.STORAGE.SDCARD.MOUNT, MUOS_INFO_PATH);
    if (written < 0 || (size_t) written >= sizeof(skip_ini)) return;

    if (!file_exist(skip_ini)) {
        written = snprintf(skip_ini, sizeof(skip_ini), "%s/%s/skip.ini",
                           device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH);
        if (written < 0 || (size_t) written >= sizeof(skip_ini)) return;
    }

    FILE *file = fopen(skip_ini, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, skip_ini)
        return;
    }

    free_skip_patterns();

    skip_pattern_list.capacity = 4;
    skip_pattern_list.patterns = malloc(skip_pattern_list.capacity * sizeof *skip_pattern_list.patterns);
    if (!skip_pattern_list.patterns) {
        perror("malloc failed");
        fclose(file);
        return;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof line, file)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

        if (len == 0 || line[0] == '#') continue;

        if (skip_pattern_list.count >= skip_pattern_list.capacity) {
            size_t newcap = skip_pattern_list.capacity * 2;
            char **newptr = realloc(skip_pattern_list.patterns, newcap * sizeof *newptr);

            if (!newptr) {
                perror("realloc failed");
                free_skip_patterns();
                fclose(file);
                return;
            }

            skip_pattern_list.patterns = newptr;
            skip_pattern_list.capacity = newcap;
        }

        skip_pattern_list.patterns[skip_pattern_list.count] = strdup(line);
        if (!skip_pattern_list.patterns[skip_pattern_list.count]) {
            perror("strdup failed");
            free_skip_patterns();
            fclose(file);
            return;
        }
        skip_pattern_list.count++;
    }

    fclose(file);
}

int should_skip(const char *name, int is_dir) {
    if (config.SETTINGS.GENERAL.HIDDEN) return 0;

    for (size_t i = 0; i < skip_pattern_list.count; i++) {
        const char *pat = skip_pattern_list.patterns[i];

        // Directory only pattern if it starts with a '/'
        if (pat[0] == '/') {
            if (!is_dir) continue;
            pat++;
        }

        if (fnmatch(pat, name, 0) == 0) return 1;
    }

    return 0;
}

void display_testing_message(lv_obj_t *screen) {
    struct screen_dimension dims = get_device_dimensions();
    int spec = 48;

    char *test_message = "This is a test image! This is a test image! This is a test image! This is a test image!\n"
                         "is a test image! This is a test image! This is a test image! This is a test image! This\n"
                         "a test image! This is a test image! This is a test image! This is a test image! This is\n"
                         "test image! This is a test image! This is a test image! This is a test image! This is a\n"
                         "image! This is a test image! This is a test image! This is a test image! This is a test\n";

    lv_obj_t *ui_conTest = lv_obj_create(screen);
    lv_obj_remove_style_all(ui_conTest);
    lv_obj_set_width(ui_conTest, dims.WIDTH);
    lv_obj_set_height(ui_conTest, dims.HEIGHT);

    lv_obj_set_align(ui_conTest, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conTest, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_conTest, lv_color_hex(0x808080), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_conTest, spec, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_conTest, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_conTest, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *ui_lblTestBottom = lv_label_create(ui_conTest);
    lv_obj_set_width(ui_lblTestBottom, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblTestBottom, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblTestBottom, 0);
    lv_obj_set_y(ui_lblTestBottom, spec * -1);
    lv_obj_set_align(ui_lblTestBottom, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_lblTestBottom, test_message);
    lv_obj_add_flag(ui_lblTestBottom, LV_OBJ_FLAG_FLOATING);

    lv_obj_t *ui_lblTestTop = lv_label_create(ui_conTest);
    lv_obj_set_width(ui_lblTestTop, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblTestTop, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblTestTop, 0);
    lv_obj_set_y(ui_lblTestTop, spec);
    lv_obj_set_align(ui_lblTestTop, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblTestTop, test_message);
    lv_obj_add_flag(ui_lblTestTop, LV_OBJ_FLAG_FLOATING);

    lv_obj_move_foreground(ui_conTest);
}

void adjust_visual_label(char *text, int method, int rep_dash) {
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
    while (isspace((unsigned char) text[start])) start++;

    len = strlen(text);
    size_t end = len ? len - 1 : 0;
    while (end < len && isspace((unsigned char) text[end])) {
        if (end == 0) break;
        end--;
    }

    if (start > 0 || end < len - 1) {
        size_t new_len = end - start + 1;
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

void update_image(lv_obj_t *ui_imgobj, struct ImageSettings image_settings) {
    if (file_exist(image_settings.image_path)) {
        char image_path[MAX_BUFFER_SIZE];
        snprintf(image_path, sizeof(image_path), "M:%s", image_settings.image_path);

        if (image_settings.max_height > 0 && image_settings.max_width > 0) {
            lv_img_header_t img_header;
            lv_img_decoder_get_info(image_path, &img_header);

            float width_ratio = (float) image_settings.max_width / (float) img_header.w;
            float height_ratio = (float) image_settings.max_height / (float) img_header.h;
            float zoom_ratio = (width_ratio < height_ratio) ? width_ratio : height_ratio;

            int zoom_factor = (int) (zoom_ratio * 256);

            lv_img_set_size_mode(ui_imgobj, LV_IMG_SIZE_MODE_REAL);
            lv_img_set_zoom(ui_imgobj, zoom_factor);
        }

        lv_obj_set_align(ui_imgobj, image_settings.align);
        lv_obj_set_style_pad_left(ui_imgobj, image_settings.pad_left, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(ui_imgobj, image_settings.pad_right, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(ui_imgobj, image_settings.pad_top, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_imgobj, image_settings.pad_bottom, MU_OBJ_MAIN_DEFAULT);
        lv_img_set_src(ui_imgobj, image_path);
        lv_obj_move_foreground(ui_imgobj);
    } else {
        lv_img_set_src(ui_imgobj, &ui_image_Nothing);
    }
}

void update_grid_scroll_position(int col_count, int row_count, int row_height,
                                 int current_item_index, lv_obj_t *ui_pnlGrid) {
    uint8_t cell_row_index = get_grid_row_index(current_item_index);
    lv_coord_t scroll_y = lv_obj_get_scroll_y(ui_pnlGrid);
    int first_visible_row = scroll_y / row_height;
    int last_visible_row = first_visible_row + row_count - 1;

    // Check if the current cell is within the visible range
    if (cell_row_index >= first_visible_row && cell_row_index <= last_visible_row) {
        // The cell is already within the visible range; no scroll needed
        return;
    }

    int y_offset = 0;

    // If the cell is below the visible range, scroll to bring it into view at the bottom
    if (cell_row_index > last_visible_row) {
        y_offset = (cell_row_index - row_count + 1) * row_height;
    }

    // If the cell is above the visible range, scroll to bring it into view at the top
    if (cell_row_index < first_visible_row) {
        y_offset = cell_row_index * row_height;
    }

    lv_obj_scroll_to_y(ui_pnlGrid, y_offset, LV_ANIM_OFF);
}

void scroll_object_to_middle(lv_obj_t *container, lv_obj_t *obj) {
    lv_coord_t scroll_y = lv_obj_get_y(obj) - (lv_obj_get_height(container) / 2) + (lv_obj_get_height(obj) / 2);
    lv_obj_scroll_to(container, lv_obj_get_scroll_x(container), scroll_y, LV_ANIM_OFF);
}

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count,
                            int current_item_index, lv_obj_t *ui_pnlContent) {
    // how many items should be above the currently selected item when scrolling
    double item_distribution = (mux_item_count - 1) / (double) 2;
    // how many items are off screen
    double scroll_multiplier = (current_item_index > item_distribution) ? (current_item_index - item_distribution)
                                                                        : 0;
    // max scroll value
    bool isAtBottom = (current_item_index >= ui_count - item_distribution - 1);
    if (isAtBottom) scroll_multiplier = ui_count - mux_item_count;

    if (mux_item_count % 2 == 0 && current_item_index > item_distribution && !isAtBottom) {
        lv_obj_set_scroll_snap_y(ui_pnlContent, LV_SCROLL_SNAP_CENTER);
    } else {
        lv_obj_set_scroll_snap_y(ui_pnlContent, LV_SCROLL_SNAP_START);
    }

    int content_panel_y = (int) round(scroll_multiplier * mux_item_panel);
    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
    lv_obj_update_snap(ui_pnlContent, LV_ANIM_OFF);
}

void load_language_file(const char *module) {
    char language_file[MAX_BUFFER_SIZE];
    snprintf(language_file, sizeof(language_file), STORAGE_LANG "/%s.json",
             config.SETTINGS.GENERAL.LANGUAGE);

    if (json_valid(read_all_char_from(language_file))) {
        translation_specific = json_object_get(json_parse(read_all_char_from(language_file)), module);
        translation_generic = json_object_get(json_parse(read_all_char_from(language_file)), "generic");
    }
}

char *translate_generic(char *key) {
    struct json translation_generic_json = json_object_get(translation_generic, key);
    if (json_exists(translation_generic_json)) {
        char translation[MAX_BUFFER_SIZE];
        json_string_copy(translation_generic_json, translation, sizeof(translation));
        return strdup(translation);
    }

    return key;
}

char *translate_specific(char *key) {
    struct json translation_specific_json = json_object_get(translation_specific, key);
    if (json_exists(translation_specific_json)) {
        char translation[MAX_BUFFER_SIZE];
        json_string_copy(translation_specific_json, translation, sizeof(translation));
        return strdup(translation);
    }

    return key;
}

void add_drop_down_options(lv_obj_t *ui_lblItemDropDown, char *options[], int count) {
    lv_dropdown_clear_options(ui_lblItemDropDown);
    for (unsigned int i = 0; i < count; i++) {
        lv_dropdown_add_option(ui_lblItemDropDown, options[i], LV_DROPDOWN_POS_LAST);
    }
}


char *generate_number_string(int min, int max, int increment, const char *prefix, const char *infix,
                             const char *suffix, int infix_position) {
    size_t buffer_size = 0;

    size_t prefix_len = prefix ? strlen(prefix) : 0;
    size_t infix_len = infix ? strlen(infix) : 0;
    size_t suffix_len = suffix ? strlen(suffix) : 0;

    if (prefix) buffer_size += prefix_len + 1;

    for (int i = min; (increment > 0 ? i <= max : i >= max); i += increment) {
        buffer_size += (size_t) snprintf(NULL, 0, "%d", i);
        if (infix) buffer_size += infix_len;
        if ((increment > 0 ? i + increment <= max : i + increment >= max)) buffer_size += 1;
    }

    if (suffix) buffer_size += suffix_len;

    char *number_string = malloc(buffer_size + 1);
    if (!number_string) return NULL;

    char *ptr = number_string;
    if (prefix) ptr += sprintf(ptr, "%s\n", prefix);

    for (int i = min; (increment > 0 ? i <= max : i >= max); i += increment) {
        if (infix && infix_position == 0) ptr += sprintf(ptr, "%s", infix);

        ptr += sprintf(ptr, "%d", i);

        if (infix && infix_position == 1) ptr += sprintf(ptr, "%s", infix);
        if ((increment > 0 ? i + increment <= max : i + increment >= max)) *ptr++ = '\n';
    }

    if (suffix) ptr += sprintf(ptr, "%s", suffix);

    *ptr = '\0';

    return number_string;
}

char *get_script_value(const char *filename, const char *key, const char *not_found) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file!");
        return strdup("");
    }

    char line[MAX_BUFFER_SIZE];
    char search_key[MAX_BUFFER_SIZE];
    snprintf(search_key, sizeof(search_key), "# %s: ", key);

    char *value = NULL;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, search_key, strlen(search_key)) == 0) {
            value = strdup(line + strlen(search_key));
            if (value) value[strcspn(value, "\n")] = 0;
            break;
        }
    }

    fclose(file);

    if (value == NULL || value[0] == '\0') value = strdup(not_found);
    return value;
}

int resolution_check(const char *filename) {
    printf("Inspecting theme for supported resolutions: %s\n", filename);
    const char *resolutions[] = {"640x480", "720x480", "720x576", "720x720", "1024x768", "1280x720"};

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, filename, 0)) {
        printf("Failed to open ZIP archive!\n");
        return 0;
    }

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;

        const char *filename = file_stat.m_filename;
        char *slash_pos = strchr(filename, '/');

        if (slash_pos && slash_pos == strrchr(filename, '/')) {
            size_t folder_length = slash_pos - filename;

            // Extract folder name
            char folder_name[256];
            strncpy(folder_name, filename, folder_length);
            folder_name[folder_length] = '\0';

            // Check if the folder name matches any target resolutions
            for (size_t j = 0; j < A_SIZE(resolutions); j++) {
                if (strcmp(folder_name, resolutions[j]) == 0) {
                    mz_zip_reader_end(&zip);
                    printf("Found supported resolution\n");
                    return 1;
                }
            }
        }
    }

    mz_zip_reader_end(&zip);
    printf("No supported resolutions found\n");

    return 0;
}

int extract_file_from_zip(const char *zip_path, const char *filename, const char *output) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        LOG_ERROR(mux_module, "Could not open archive '%s' - Corrupt?", zip_path)
        return 0;
    }

    int file_index = mz_zip_reader_locate_file(&zip, filename, NULL, 0);
    if (file_index == -1) {
        LOG_ERROR(mux_module, "File '%s' not found in archive", filename)
        mz_zip_reader_end(&zip);
        return 0;
    }

    if (!mz_zip_reader_extract_to_file(&zip, file_index, output, 0)) {
        LOG_ERROR(mux_module, "File '%s' could not be extracted", filename)
        mz_zip_reader_end(&zip);
        return 0;
    }

    mz_zip_reader_end(&zip);
    return 1;
}

void add_directory_to_list(char ***list, size_t *size, size_t *count, const char *dir) {
    if (*count >= *size) {
        size_t new_size = *size + 10;

        char **new_list = realloc(*list, new_size * sizeof(char *));
        if (!new_list) return;

        *list = new_list;
        *size = new_size;
    }

    (*list)[*count] = strdup(dir);
    if (!(*list)[*count]) return;

    (*count)++;
}

void collect_subdirectories(const char *base_dir, char ***list, size_t *size, size_t *count, size_t trim_start_count) {
    char subdir_path[PATH_MAX];
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
        return;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            const char *name = entry->d_name;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            if (should_skip(name, 1)) continue;

            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", base_dir, name);
            const char *trimmed_path = subdir_path + trim_start_count;

            add_directory_to_list(list, size, count, trimmed_path);
            collect_subdirectories(subdir_path, list, size, count, trim_start_count);
        }
    }

    closedir(dir);
}

char **get_subdirectories(const char *base_dir) {
    size_t trim_start_count = strlen(base_dir) + 1;
    size_t list_size = 10;
    size_t count = 0;

    char **subdir_list = malloc(list_size * sizeof(char *));
    if (!subdir_list) return NULL;

    collect_subdirectories(base_dir, &subdir_list, &list_size, &count, trim_start_count);
    subdir_list[count] = NULL;

    return subdir_list;
}

void free_subdirectories(char **dir_names) {
    if (dir_names == NULL) return;

    for (int i = 0; dir_names[i] != NULL; i++) free(dir_names[i]);

    free(dir_names);
}

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index) {
    for (int i = 0; i < num_options; i++) {
        if (value == options[i]) {
            lv_dropdown_set_selected(dropdown, i);
            return;
        }
    }

    lv_dropdown_set_selected(dropdown, def_index);
}

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value) {
    if (selected_index >= 0 && selected_index < num_options) {
        return options[selected_index];
    }

    return def_value;
}

void free_sound_cache(void) {
    for (int i = 0; i < SOUND_TOTAL; ++i) {
        if (sound_cache[i].chunk) {
            Mix_FreeChunk(sound_cache[i].chunk);
            sound_cache[i].chunk = NULL;
        }
    }
}

void free_bgm(void) {
    if (current_bgm) {
        Mix_HaltMusic();
        Mix_FreeMusic(current_bgm);
        current_bgm = NULL;
    }
}

void set_bgm_volume(int volume) {
    if (current_bgm) {
        if (volume < 0) volume = 0;
        if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;

        bgm_volume = volume;
        Mix_VolumeMusic(bgm_volume);
    }
}

void play_random_bgm(void) {
    if (bgm_file_count == 0) return;

    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    static size_t last_index = SIZE_MAX;

    size_t index;

    if (bgm_file_count == 1) {
        index = 0;
    } else {
        do {
            index = random() % bgm_file_count;
        } while (index == last_index);
    }

    const char *path = bgm_files[index];
    last_index = index;

    free_bgm();

    current_bgm = Mix_LoadMUS(path);
    if (current_bgm) {
        is_silence_playing = 0;
        Mix_VolumeMusic((config.SETTINGS.GENERAL.BGMVOL * MIX_MAX_VOLUME) / 100);
        Mix_PlayMusic(current_bgm, 1);
    }
}

void play_silence_bgm(void) {
    free_bgm();
    is_silence_playing = 0;

    char silence_path[MAX_BUFFER_SIZE];
    snprintf(silence_path, sizeof(silence_path), "%s", BGM_SILENCE);

    if (!file_exist(silence_path)) {
        LOG_INFO("audio", "No 'silence.ogg' file found")
        return;
    }

    current_bgm = Mix_LoadMUS(silence_path);
    if (current_bgm) {
        Mix_PlayMusic(current_bgm, -1);
        is_silence_playing = 1;
        LOG_SUCCESS("audio", "Silence BGM playback started")
    } else {
        LOG_ERROR("audio", "Failed to load 'silence.ogg': %s", Mix_GetError())
    }
}

int init_audio_backend(void) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        LOG_ERROR("audio", "SDL Init Failed")
        return 0;
    }

    int flags = MIX_INIT_OGG;
    int inited = Mix_Init(flags);
    if ((inited & flags) != flags) {
        LOG_ERROR("audio", "Missing SDL_mixer support for OGG")
    }

    if (Mix_OpenAudio(44100, AUDIO_F32LSB, 2, 2048) < 0) {
        LOG_ERROR("audio", "SDL_mixer open failed: %s", Mix_GetError())
        return 0;
    }

    LOG_SUCCESS("audio", "SDL Init Success")

/*
    printf("Audio Decode Support: ");
    for (int i = 0; i < Mix_GetNumMusicDecoders(); i++) {
        printf("%s ", Mix_GetMusicDecoder(i));
    }
    printf("\n");
*/

    return 1;
}

void init_fe_snd(int *fe_snd, int snd_type, int re_init) {
    *fe_snd = 0;
    free_sound_cache();

    if (!snd_type && !re_init) return;

    char base_path[MAX_BUFFER_SIZE];
    snprintf(base_path, sizeof(base_path), "%s", STORAGE_SOUND);
    if (snd_type == 2) {
        const char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;
        snprintf(base_path, sizeof(base_path), "%s/sound", theme_location);
    }

    DIR *dir = opendir(base_path);
    if (!dir) {
        LOG_INFO("audio", "Sound directory not found: %s", base_path)
        return;
    }

    for (int i = 0; i < SOUND_TOTAL; ++i) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s/%s.wav", base_path, snd_names[i]);

        if (file_exist(path)) {
            sound_cache[i].chunk = Mix_LoadWAV(path);
        } else {
            sound_cache[i].chunk = NULL;
        }
    }

    *fe_snd = 1;
    LOG_SUCCESS("audio", "FE Sound Started")
}

void init_fe_bgm(int *fe_bgm, int bgm_type, int re_init) {
    if (!bgm_type && !re_init) {
        play_silence_bgm();
        return;
    }

    free_bgm();
    *fe_bgm = 0;

    char base_path[MAX_BUFFER_SIZE];
    snprintf(base_path, sizeof(base_path), "%s", STORAGE_MUSIC);
    if (bgm_type == 2) {
        const char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;
        snprintf(base_path, sizeof(base_path), "%s/music", theme_location);
    }

    DIR *dir = opendir(base_path);
    if (!dir) {
        LOG_INFO("audio", "Music directory not found: %s", base_path)
        return;
    }

    size_t capacity = 8;
    bgm_files = malloc(capacity * sizeof(char *));
    if (!bgm_files) {
        LOG_ERROR("audio", "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        closedir(dir);
        return;
    }

    bgm_file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".ogg") == 0) {
            if (bgm_file_count >= capacity) {
                capacity *= 2;
                char **bgm_temp = realloc(bgm_files, capacity * sizeof(char *));

                if (!bgm_temp) {
                    LOG_ERROR("audio", "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
                    closedir(dir);
                    return;
                }

                bgm_files = bgm_temp;
            }

            char full_path[MAX_BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

            bgm_files[bgm_file_count] = strdup(full_path);
            if (!bgm_files[bgm_file_count]) {
                LOG_ERROR("audio", "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
                continue;
            }

            bgm_file_count++;
        }
    }

    closedir(dir);

    if (bgm_file_count > 0) {
        Mix_HookMusicFinished(play_random_bgm);
        play_random_bgm();
        *fe_bgm = 1;
        LOG_SUCCESS("audio", "FE Music playback started")
    } else {
        LOG_INFO("audio", "No OGG music files found")
        play_silence_bgm();
    }
}

int safe_atoi(const char *str) {
    if (str == NULL) return 0;

    errno = 0;
    char *str_ptr;
    long val = strtol(str, &str_ptr, 10);

    if (str_ptr == str) return 0;
    if (*str_ptr != '\0') return 0;
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (val > INT_MAX || val < INT_MIN)) return 0;

    return (int) val;
}

void init_grid_info(int item_count, int column_count) {
    grid_info.item_count = item_count;
    grid_info.column_count = column_count;

    // Calculate items in the last row
    int last_row_item_count = item_count % column_count;
    if (last_row_item_count == 0 && item_count != 0) {
        last_row_item_count = column_count;  // Full last row if remainder is 0
    }
    grid_info.last_row_item_count = last_row_item_count;

    // Calculate the last row index
    grid_info.last_row_index = (item_count - 1) / column_count;
}

int get_grid_row_index(int current_item_index) {
    return current_item_index / grid_info.column_count;
}

int get_grid_column_index(int current_item_index) {
    return current_item_index % grid_info.column_count;
}

int get_grid_row_item_count(int current_item_index) {
    uint8_t row_index = current_item_index / grid_info.column_count;

    if (row_index == grid_info.last_row_index) {
        return grid_info.last_row_item_count;
    } else {
        return grid_info.column_count;
    }
}

static void set_grid_catalogue_and_program_name(int index, char *catalogue_name, size_t catalogue_name_size,
                                                char *program, size_t program_size) {
    if (strcmp(mux_module, "muxapp") == 0) {
        snprintf(catalogue_name, catalogue_name_size, "Application");
        snprintf(program, program_size, "%s", items[index].glyph_icon);
        return;
    }

    if (items[index].content_type == ITEM) {
        if (strcmp(mux_module, "muxcollect") == 0 || strcmp(mux_module, "muxhistory") == 0) {
            char *item_dir = strip_dir(items[index].extra_data);
            char *file_path = strdup(items[index].extra_data);
            char *item_file = get_last_dir(file_path);
            if (item_dir && item_file) {
                get_catalogue_name(item_dir, item_file, catalogue_name, catalogue_name_size);
                snprintf(program, program_size, "%s", strip_ext(item_file));
            }
            free(item_dir);
            free(file_path);
        } else {
            get_catalogue_name(strdup(sys_dir), items[index].name, catalogue_name, catalogue_name_size);
            snprintf(program, program_size, "%s", strip_ext(items[index].name));
        }
        return;
    }

    const char *cat_label = strcmp(mux_module, "muxplore") == 0 ? "Folder" : "Collection";
    snprintf(catalogue_name, catalogue_name_size, "%s", cat_label);
    snprintf(program, program_size, "%s", strip_ext(items[index].name));
}

void update_grid_image_paths(int index) {
    char catalogue_name[MAX_BUFFER_SIZE];

    char program[MAX_BUFFER_SIZE];
    set_grid_catalogue_and_program_name(index, catalogue_name, sizeof(catalogue_name), program, sizeof(program));

    char alt_name[MAX_BUFFER_SIZE];
    snprintf(alt_name, sizeof(alt_name), "%s",
             strcmp(mux_module, "muxplore") == 0 ? get_catalogue_name_from_rom_path(sys_dir, items[index].name) : "");

    char grid_image[MAX_BUFFER_SIZE];
    load_image_catalogue(catalogue_name, program, alt_name, "default",
                         mux_dimension, "grid", grid_image, sizeof(grid_image));

    char glyph_name_focused[MAX_BUFFER_SIZE];
    snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", program);

    char alt_name_focused[MAX_BUFFER_SIZE];
    snprintf(alt_name_focused, sizeof(alt_name_focused), "%s_focused", alt_name);

    char grid_image_focused[MAX_BUFFER_SIZE];
    load_image_catalogue(catalogue_name, glyph_name_focused, alt_name_focused, "default_focused",
                         mux_dimension, "grid", grid_image_focused, sizeof(grid_image_focused));

    if (strcmp(mux_module, "muxapp") == 0) {
        get_app_grid_glyph(items[index].extra_data, program, "default", grid_image, sizeof(grid_image));
        get_app_grid_glyph(items[index].extra_data, glyph_name_focused, "default_focused", grid_image_focused,
                           sizeof(grid_image_focused));
    }

    items[index].grid_image = strdup(grid_image);
    items[index].grid_image_focused = strdup(grid_image_focused);
}

static void update_grid_image(lv_obj_t *cell, char *image_path) {
    if (file_exist(image_path)) {
        char grid_image[MAX_BUFFER_SIZE];
        snprintf(grid_image, sizeof(grid_image), "M:%s", image_path);
        lv_img_set_src(cell, grid_image);
    } else {
        lv_img_set_src(cell, &ui_image_Nothing);
    }
}

static void update_grid_item(lv_obj_t *ui_pnlItem, int index) {
    lv_obj_set_user_data(ui_pnlItem, UFI(index));
    lv_obj_t *ui_lblItem = lv_obj_get_child(ui_pnlItem, 1);
    lv_obj_t *cell_image = lv_obj_get_child(ui_pnlItem, 0);
    lv_obj_t *cell_image_focused = lv_obj_get_child(ui_pnlItem, 2);

    if (index >= item_count) {
        lv_obj_add_flag(ui_pnlItem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblItem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cell_image, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(ui_lblItem, items[index].display_name);
        if (items[index].glyph_icon != NULL) lv_obj_set_user_data(ui_lblItem, items[index].glyph_icon);

        if (items[index].grid_image == NULL) {
            update_grid_image_paths(index);
        }
        update_grid_image(cell_image, items[index].grid_image);
        update_grid_image(cell_image_focused, items[index].grid_image_focused);

        lv_obj_clear_flag(ui_pnlItem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblItem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cell_image, LV_OBJ_FLAG_HIDDEN);

        if (current_item_index == index && !is_carousel_grid_mode()) {
            lv_group_focus_obj(ui_pnlItem);
            lv_group_focus_obj(ui_lblItem);
            lv_group_focus_obj(cell_image);
        }
    }
}

void update_grid_items(int direction) {
    if (is_carousel_grid_mode()) {
        int carousel_item_count = (theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT)
                                  ? theme.GRID.COLUMN_COUNT : theme.GRID.ROW_COUNT;

        for (int i = 0; i < carousel_item_count; i++) {
            int offset = i - (carousel_item_count / 2);

            size_t raw_index = ((size_t) current_item_index + offset + (size_t) item_count) % (size_t) item_count;
            int index = (int) raw_index;

            lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, i);
            update_grid_item(panel_item, index);
        }
    } else {
        int row_index = get_grid_row_index(current_item_index);
        int start_row_index = (direction < 0) ? row_index : row_index - theme.GRID.ROW_COUNT + 1;

        if (start_row_index < 0) start_row_index = 0;

        int start_index = start_row_index * theme.GRID.COLUMN_COUNT;
        int total_items = theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT;

        for (int index = 0; index < total_items; ++index) {
            lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, index);
            update_grid_item(panel_item, start_index + index);
        }
    }
}

static int get_grid_item_index(int index) {
    lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, index);
    int item_index = IFU(lv_obj_get_user_data(panel_item));
    return item_index;
}

void update_grid(int direction) {
    if (is_carousel_grid_mode()) {
        update_grid_items(1);
        return;
    }

    if (item_count <= theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) return;
    int grid_start_index = get_grid_item_index(0);
    int grid_end_index = get_grid_item_index(theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT - 1);

    if (direction < 0) {
        if (current_item_index == item_count - 1) {
            update_grid_items(1);
        } else {
            if (current_item_index < grid_start_index) {
                update_grid_items(direction);
            }
        }
    } else {
        if (current_item_index == 0) {
            update_grid_items(-1);
        } else {
            if (current_item_index > grid_end_index) {
                update_grid_items(direction);
            }
        }
    }
}

static void gen_grid_item_common(int item_index, int panel_index, int focus_index) {
    if (items[item_index].grid_image == NULL) update_grid_image_paths(item_index);

    uint8_t col, row;
    if (is_carousel_grid_mode()) {
        col = (theme.GRID.COLUMN_COUNT == 1) ? 0 : panel_index;
        row = (theme.GRID.ROW_COUNT == 1) ? 0 : panel_index;
    } else {
        col = item_index % theme.GRID.COLUMN_COUNT;
        row = item_index / theme.GRID.COLUMN_COUNT;
    }

    lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
    lv_obj_set_user_data(cell_panel, UFI(item_index));

    lv_obj_t *cell_image = lv_img_create(cell_panel);
    lv_obj_t *cell_label = lv_label_create(cell_panel);
    if (items[item_index].glyph_icon != NULL) lv_obj_set_user_data(cell_label, items[item_index].glyph_icon);

    create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                     items[item_index].grid_image, items[item_index].grid_image_focused,
                     items[item_index].display_name);

    lv_group_add_obj(ui_group, cell_label);
    lv_group_add_obj(ui_group_glyph, cell_image);
    lv_group_add_obj(ui_group_panel, cell_panel);

    if (is_carousel_grid_mode() && focus_index == panel_index) {
        lv_group_focus_obj(cell_panel);
        lv_group_focus_obj(cell_label);
        lv_group_focus_obj(cell_image);
    }
}

void gen_grid_item(int item_index) {
    gen_grid_item_common(item_index, 0, -1);
}

static void gen_carousel_grid_item(int item_index, int panel_index, int focus_index) {
    gen_grid_item_common(item_index, panel_index, focus_index);
}

int disable_grid_file_exists(char *item_curr_dir) {
    char no_grid_path[PATH_MAX];
    snprintf(no_grid_path, sizeof(no_grid_path), "%s/.nogrid", item_curr_dir);
    return file_exist(no_grid_path);
}

void create_carousel_grid(void) {
    int carousel_item_count = (theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT)
                              ? theme.GRID.COLUMN_COUNT : theme.GRID.ROW_COUNT;

    int middle_index = carousel_item_count / 2;

    for (int i = 0; i < carousel_item_count; i++) {
        int offset = i - (carousel_item_count / 2);

        size_t raw_index = ((size_t) current_item_index + offset + (size_t) item_count) % (size_t) item_count;
        int item_index = (int) raw_index;

        gen_carousel_grid_item(item_index, i, middle_index);
    }
}

int is_carousel_grid_mode() {
    int carousel_item_count = (theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT) ? theme.GRID.COLUMN_COUNT
                                                                               : theme.GRID.ROW_COUNT;
    return (grid_mode_enabled && (theme.GRID.ROW_COUNT == 1 || theme.GRID.COLUMN_COUNT == 1) &&
            carousel_item_count > 2 && carousel_item_count % 2 == 1) ? 1 : 0;
}

void kiosk_denied(void) {
    if (is_ksk(kiosk.MESSAGE)) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.KIOSK_DISABLE, MEDIUM);
        refresh_screen(ui_screen);
    }
}

static pid_t pending_exec_pid = -1;
static exec_callback pending_exec_cb = NULL;

void run_exec(const char *args[], size_t size, int background, int turbo, const char *log_file, exec_callback cb) {
    const char *san[size];

    if (turbo) turbo_time(1, 0);

    size_t j = 0;
    for (size_t i = 0; i < size; ++i) {
        if (args[i]) san[j++] = args[i];
    }
    san[j] = NULL;

/*
 * Debugging message to print arguments to check if nulls
 * are being sanitised or not.  They should but you never
 * know with C these days...
 *
 *  for (size_t k = 0; k < j; ++k) {
 *      printf("arg[%zu]: %s\n", k, san[k]);
 *  }
*/

    pid_t pid = fork();
    if (pid == 0) {
        if (background && cb == NULL) {
            // If we run in the background lets disconnect from the parent...
            setsid();

            // Perform a second fork to ensure the background process is reaped by init,
            // preventing zombie processes from hanging around after completion...
            pid_t pid2 = fork();
            if (pid2 < 0) _exit(EXIT_FAILURE);
            if (pid2 > 0) _exit(EXIT_SUCCESS);

            // Secondary child continues here, now fully detached from the parent
            int fd;
            if (log_file && *log_file) {
                fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) fd = open("/dev/null", O_RDWR);
            } else {
                fd = open("/dev/null", O_RDWR);
            }

            if (fd != -1) {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);

                if (fd > 2) close(fd);
            }
        }

        execvp(san[0], (char *const *) san);
        _exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Foreground process, block until child finishes execution
        // Parent process does not wait for background process
        // Destroy the intermediate child immediately to avoid zombies... oh no
        if (!background) {
            waitpid(pid, NULL, 0);
        } else {
            pending_exec_pid = pid;
            pending_exec_cb = cb;
        }
    }

    if (turbo) turbo_time(0, 0);
}

void exec_watch_task() {
    if (pending_exec_pid <= 0) return;

    int status;
    pid_t r = waitpid(pending_exec_pid, &status, WNOHANG);
    if (r == 0) return;

    if (r < 0) {
        pending_exec_pid = -1;
        pending_exec_cb = NULL;
        return;
    }

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (pending_exec_cb) pending_exec_cb(exit_code);

    pending_exec_pid = -1;
    pending_exec_cb = NULL;
}

char *get_content_line(char *dir, char *name, char *ext, size_t line) {
    static char path[MAX_BUFFER_SIZE];
    char *subdir = get_last_subdir(dir, '/', 4);

    if (name == NULL) {
        snprintf(path, sizeof(path), INFO_COR_PATH "/%s/core.%s",
                 subdir, ext);
    } else {
        snprintf(path, sizeof(path), INFO_COR_PATH "/%s/%s.%s",
                 subdir, strip_ext(name), ext);
    }

    if (file_exist(path)) return read_line_char_from(path, line);

    return "";
}

char *get_application_line(char *dir, char *ext, size_t line) {
    static char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/mux_option.%s",
             dir, ext);

    if (file_exist(path)) return read_line_char_from(path, line);

    return "";
}

struct screen_dimension get_device_dimensions(void) {
    struct screen_dimension dims;
    if (read_line_int_from(device.SCREEN.HDMI, 1)) {
        dims.WIDTH = device.SCREEN.EXTERNAL.WIDTH;
        dims.HEIGHT = device.SCREEN.EXTERNAL.HEIGHT;
    } else {
        dims.WIDTH = device.SCREEN.INTERNAL.WIDTH;
        dims.HEIGHT = device.SCREEN.INTERNAL.HEIGHT;
    }

    LOG_INFO(mux_module, "Screen Output Dimensions: %dx%d", dims.WIDTH, dims.HEIGHT)
    return dims;
}

void set_nav_flags(struct nav_flag *nav_flags, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (nav_flags[i].visible) {
            lv_obj_clear_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

int16_t validate_int16(int value, const char *field) {
    if (value < INT16_MIN || value > INT16_MAX) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_INT16_LENGTH)
        return (value < INT16_MIN) ? INT16_MIN : INT16_MAX;
    }
    return (int16_t)
            value;
}

int at_base(char *sys_dir, char *base_name) {
    char *base_dir = strrchr(sys_dir, '/');
    return (base_dir && strcasecmp(base_dir + 1, base_name) == 0) ? 1 : 0;
}

int search_for_config(const char *base_path, const char *file_name, const char *system_name) {
    struct dirent *entry;
    char full_path[PATH_MAX];
    DIR *dir = opendir(base_path);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        if (entry->d_type == DT_REG) {
            if (strstr(entry->d_name, file_name)) {
                char *line = read_line_char_from(full_path, 2);
                if (line && strcmp(line, system_name) == 0) {
                    closedir(dir);
                    return 1;
                }
            }
        } else if (entry->d_type == DT_DIR) {
            if (search_for_config(full_path, file_name, system_name)) {
                closedir(dir);
                return 1;
            }
        }
    }

    closedir(dir);
    return 0;
}

void populate_items(const char *base_path, char ***items, int *item_count) {
    struct dirent *entry;
    char full_path[PATH_MAX];
    DIR *dir = opendir(base_path);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN)
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_STAT)
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (strstr(entry->d_name, ".cfg")) {
                *items = realloc(*items, ((*item_count) + 1) * sizeof(char *));
                (*items)[*item_count] = strdup(read_line_char_from(full_path, 1));
                (*item_count)++;
            }
        } else if (S_ISDIR(st.st_mode)) {
            populate_items(full_path, items, item_count);
        }
    }

    closedir(dir);
}

void populate_history_items(void) {
    populate_items(INFO_HIS_PATH, &history_items, &history_item_count);
}

void populate_collection_items(void) {
    populate_items(INFO_COL_PATH, &collection_items, &collection_item_count);
}

char *get_content_explorer_glyph_name(char *file_path) {
    if (config.VISUAL.CONTENTCOLLECT == 0) {
        for (int i = 0; i < collection_item_count; i++) {
            if (strcmp(collection_items[i], file_path) == 0) return "collection";
        }
    }

    if (config.VISUAL.CONTENTHISTORY == 0) {
        for (int i = 0; i < history_item_count; i++) {
            if (strcmp(history_items[i], file_path) == 0) return "history";
        }
    }

    return "rom";
}

uint32_t fnv1a_hash_str(const char *str) {
    uint32_t hash = 2166136261U; // FNV offset basis

    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t) (*p);
        hash *= 16777619; // FNV prime
    }

    return hash;
}

uint32_t fnv1a_hash_file(FILE *file) {
    uint32_t hash = 2166136261U; // FNV offset basis
    unsigned char buf[65535];
    size_t n;

    while ((n = fread(buf, 1, sizeof buf, file)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= buf[i];
            hash *= 16777619; // FNV prime
        }
    }

    if (ferror(file)) return 0;
    return hash;
}

bool get_glyph_path(const char *mux_module, const char *glyph_name,
                    char *glyph_image_embed, size_t glyph_image_embed_size) {
    char glyph_image_path[MAX_BUFFER_SIZE];
    if ((snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/%sglyph/%s/%s.png",
                  STORAGE_THEME, mux_dimension, mux_module, glyph_name) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s/%s.png",
                  STORAGE_THEME, mux_module, glyph_name) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/%sglyph/%s/%s.png",
                  INTERNAL_THEME, mux_dimension, mux_module, glyph_name) >= 0 && file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s/%s.png",
                  INTERNAL_THEME, mux_module, glyph_name) >= 0 &&
         file_exist(glyph_image_path))) {
        snprintf(glyph_image_embed, glyph_image_embed_size, "M:%s", glyph_image_path);
        return true;
    }

    return false;
}

void apply_app_glyph(const char *app_folder, const char *glyph_name, lv_obj_t *ui_lblItemGlyph) {
    char glyph_image_path[MAX_BUFFER_SIZE];
    if ((snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s%s.png", app_folder, mux_dimension,
                  glyph_name) >= 0 &&
         file_exist(glyph_image_path)) ||
        (snprintf(glyph_image_path, sizeof(glyph_image_path), "%s/glyph/%s.png", app_folder, glyph_name) >= 0 &&
         file_exist(glyph_image_path))
            ) {
        char glyph_image_embed[MAX_BUFFER_SIZE];
        snprintf(glyph_image_embed, sizeof(glyph_image_embed), "M:%s", glyph_image_path);
        lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);
    }
}

void get_app_grid_glyph(const char *app_folder, const char *glyph_name, const char *fallback_name,
                        char *glyph_image_path, size_t glyph_image_path_size) {
    if (file_exist(glyph_image_path) && strstr(glyph_image_path, fallback_name) == 0) return;

    char image_path[MAX_BUFFER_SIZE];
    if ((snprintf(image_path, sizeof(image_path), "%s/grid/%s%s.png", app_folder, mux_dimension, glyph_name) >= 0 &&
         file_exist(image_path)) ||
        (snprintf(image_path, sizeof(image_path), "%s/grid/%s.png", app_folder, glyph_name) >= 0 &&
         file_exist(image_path))
            ) {
        snprintf(glyph_image_path, glyph_image_path_size, "%s", image_path);
    }
}

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count, int *nav_moved) {
    if (!file_exist(MUOS_PDI_LOAD)) return 0;

    char *prev = read_all_char_from(MUOS_PDI_LOAD);
    if (!prev) return 0;

    int text_hit = 0;
    for (size_t i = 0; i < ui_count; i++) {
        const bool item_hidden = lv_obj_has_flag(ui_objects[i], LV_OBJ_FLAG_HIDDEN);
        if (!item_hidden && strcasecmp(lv_obj_get_user_data(ui_objects[i]), prev) == 0) break;
        if (!item_hidden) text_hit++;
    }

    int nav_next_return = 0;
    if (text_hit > 0) {
        *nav_moved = 1;
        if (strcmp(mux_module, "muxtweakgen") == 0) {
            nav_next_return = text_hit - !device.BOARD.HAS_HDMI;
        } else if (strcmp(mux_module, "muxtweakadv") == 0) {
            nav_next_return = text_hit - !device.BOARD.HAS_NETWORK;
        } else if (strcmp(mux_module, "muxconfig") == 0 && !config.NETWORK.TYPE && strcasecmp(prev, "connect") == 0) {
            nav_next_return = 4;
        } else {
            nav_next_return = text_hit;
        }
    }

    free(prev);
    remove(MUOS_PDI_LOAD);

    return nav_next_return;
}

int theme_compat(void) {
    char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;
    char theme_version_file[MAX_BUFFER_SIZE];
    snprintf(theme_version_file, sizeof(theme_version_file), "%s/version.txt", theme_location);

    if (file_exist(theme_version_file)) {
        char *theme_version = read_line_char_from(theme_version_file, 1);
        for (int i = 0; theme_back_compat[i] != NULL; i++) {
            if (str_startswith(theme_back_compat[i], theme_version)) return 1;
        }
        LOG_WARN(mux_module, "Incompatible Theme Detected: %s", theme_version)
    } else {
        LOG_WARN(mux_module, "Missing Theme Version File or Version Content")
    }

    return 0;
}

int brightness_to_percent(int val) {
    return (val * 100) / device.SCREEN.BRIGHT;
}

int volume_to_percent(int val) {
    int max = config.SETTINGS.ADVANCED.OVERDRIVE ? 200 : 100;
    return (val * 100) / max;
}

char **str_parse_file(const char *filename, int *count, enum parse_mode mode) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.SYSTEM.FAIL_FILE_OPEN, filename)
        return NULL;
    }

    char **list = malloc(MAX_BUFFER_SIZE * sizeof(char *));
    if (!list) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        fclose(file);
        return NULL;
    }

    *count = 0;
    char line[MAX_BUFFER_SIZE];
    int failed = 0;

    if (mode == TOKENS) {
        if (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, " \t\r\n");
            while (token && *count < MAX_BUFFER_SIZE) {
                list[*count] = strdup(token);

                if (!list[*count]) {
                    failed = 1;
                    break;
                }

                (*count)++;
                token = strtok(NULL, " \t\r\n");
            }
        }
    } else {
        while (fgets(line, sizeof(line), file) && *count < MAX_BUFFER_SIZE) {
            char *end = strpbrk(line, "\r\n");
            if (end) *end = '\0';
            if (*line == '\0') continue;

            list[*count] = strdup(line);
            if (!list[*count]) {
                failed = 1;
                break;
            }

            (*count)++;
        }
    }

    fclose(file);

    if (failed) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        for (int i = 0; i < *count; i++) free(list[i]);
        free(list);

        return NULL;
    }

    return list;
}

int is_partition_mounted(const char *partition) {
    if (strcmp(partition, "/") == 0) return 1; // this is rootfs so I mean it should always be mounted

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        perror("fopen /proc/mounts");
        return 0;
    }

    char line[MAX_BUFFER_SIZE];
    int mounted = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, partition)) {
            mounted = 1;
            break;
        }
    }

    fclose(fp);
    return mounted;
}

void get_storage_info(const char *partition, double *total, double *free, double *used) {
    struct statvfs stat;

    if (!is_partition_mounted(partition)) {
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    if (statvfs(partition, &stat) != 0) {
        perror("statvfs");
        *total = 0.0;
        *free = 0.0;
        *used = 0.0;
        return;
    }

    *total = (double) (stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);
    *free = (double) (stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);
    *used = *total - *free;
}

char *get_version(void) {
    static char version[64];
    snprintf(version, sizeof(version), "%s", str_replace(config.SYSTEM.VERSION, "_", " "));
    return version;
}

char *get_build(void) {
    static char build[16];
    snprintf(build, sizeof(build), "%s", config.SYSTEM.BUILD);
    return build;
}

int copy_file(const char *from, const char *to) {
    int fd_to = -1;
    int fd_from = -1;
    int saved_errno = 0;

    struct stat st;

    fd_from = open(from, O_RDONLY | O_CLOEXEC);
    if (fd_from < 0) return -1;

    if (fstat(fd_from, &st) < 0) goto out_error;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, st.st_mode & 0777);
    if (fd_to < 0) goto out_error;

    char buf[65536];
    ssize_t f_read;
    while (1) {
        f_read = read(fd_from, buf, sizeof buf);

        if (f_read == 0) break;

        if (f_read < 0) {
            if (errno == EINTR) continue;
            goto out_error;
        }

        size_t off = 0;
        while (off < (size_t) f_read) {
            ssize_t nw = write(fd_to, buf + off, (size_t) f_read - off);

            if (nw < 0) {
                if (errno == EINTR) continue;
                goto out_error;
            }

            off += (size_t) nw;
        }
    }

    if (fsync(fd_to) < 0) goto out_error;

    if (fchmod(fd_to, st.st_mode & 0777) < 0) goto out_error;

    if (close(fd_to) < 0) {
        fd_to = -1;
        goto out_error;
    }

    close(fd_from);
    return 0;

    out_error:
    saved_errno = errno;

    if (fd_from >= 0) close(fd_from);

    if (fd_to >= 0) {
        close(fd_to);
        unlink(to);
    }

    errno = saved_errno;
    return -1;
}

int load_content(int add_collection, char *sys_dir, char *file_name) {
    char *assigned_core = load_content_core(0, !add_collection, sys_dir, file_name);
    if (assigned_core == NULL || strcasestr(assigned_core, "(null)")) return 0;

    const char *content_name = strip_ext(file_name);
    const char *system_sub = get_last_subdir(sys_dir, '/', 4);

    char content_loader_file[MAX_BUFFER_SIZE];
    snprintf(content_loader_file, sizeof(content_loader_file), INFO_COR_PATH "/%s/%s.cfg",
             system_sub,
             content_name);
    LOG_INFO(mux_module, "Configuration File: %s", content_loader_file)

    if (!file_exist(content_loader_file)) {
        char content_loader_data[MAX_BUFFER_SIZE];
        snprintf(content_loader_data, sizeof(content_loader_data), "%s|%s|%s/|%s|%s",
                 content_name,
                 str_replace(assigned_core, "\n", "|"),
                 STORAGE_PATH,
                 system_sub,
                 file_name);

        write_text_to_file(content_loader_file, "w", CHAR, str_replace(content_loader_data, "|", "\n"));
        LOG_INFO(mux_module, "Configuration Data: %s", content_loader_data)
    }

    if (file_exist(content_loader_file)) {
        char pointer[MAX_BUFFER_SIZE];
        char content[MAX_BUFFER_SIZE];

        char cache_file[MAX_BUFFER_SIZE];
        snprintf(cache_file, sizeof(cache_file), INFO_COR_PATH "/%s/%s.cfg",
                 system_sub, content_name);

        LOG_INFO(mux_module, "Using Configuration: %s", cache_file)

        if (add_collection) {
            add_to_collection(file_name, cache_file, sys_dir);
        } else {
            LOG_INFO(mux_module, "Assigned Core: %s", assigned_core)

            char *assigned_gov = specify_asset(load_content_governor(sys_dir, NULL, 0, 1, 0),
                                               device.CPU.DEFAULT, "Governor");

            char *assigned_con = specify_asset(load_content_control_scheme(sys_dir, NULL, 0, 1, 0),
                                               "system", "Control Scheme");

            char full_file_path[MAX_BUFFER_SIZE];
            snprintf(full_file_path, sizeof(full_file_path), "%s%s/%s",
                     read_line_char_from(cache_file, CONTENT_MOUNT),
                     read_line_char_from(cache_file, CONTENT_DIR),
                     file_name);

            snprintf(pointer, sizeof(pointer), "%s\n%s\n%s",
                     full_file_path, system_sub, content_name);

            snprintf(content, sizeof(content), INFO_HIS_PATH "/%s-%08X.cfg",
                     content_name, fnv1a_hash_str(full_file_path));

            write_text_to_file(content, "w", CHAR, pointer);
            write_text_to_file(LAST_PLAY_FILE, "w", CHAR, cache_file);

            write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
            write_text_to_file(MUOS_CON_LOAD, "w", CHAR, assigned_con);
            write_text_to_file(MUOS_ROM_LOAD, "w", CHAR, read_all_char_from(content_loader_file));
        }

        LOG_SUCCESS(mux_module, "Content Loaded Successfully")

        return 1;
    }

    return 0;
}

char *load_content_core(int force, int run_quit, char *sys_dir, char *file_name) {
    char content_core[MAX_BUFFER_SIZE] = {0};
    const char *last_subdir = get_last_subdir(sys_dir, '/', 4);

    if (strcasecmp(last_subdir, strip_dir(STORAGE_PATH)) == 0) {
        snprintf(content_core, sizeof(content_core), INFO_COR_PATH "/core.cfg");
    } else {
        snprintf(content_core, sizeof(content_core), INFO_COR_PATH "/%s/%s.cfg",
                 last_subdir, strip_ext(file_name));

        if (file_exist(content_core) && !force) {
            LOG_SUCCESS(mux_module, "Loading Individual Core: %s", content_core)

            char *core = build_core(content_core, CONTENT_CORE, CONTENT_SYSTEM,
                                    CONTENT_CATALOGUE, CONTENT_LOOKUP, CONTENT_ASSIGN);

            if (core) return core;

            LOG_ERROR(mux_module, "Failed to build individual core")
        }

        snprintf(content_core, sizeof(content_core), INFO_COR_PATH "/%s/core.cfg", last_subdir);
    }

    if (file_exist(content_core) && !force) {
        LOG_SUCCESS(mux_module, "Loading Global Core: %s", content_core)

        char *core = build_core(content_core, GLOBAL_CORE, GLOBAL_SYSTEM,
                                GLOBAL_CATALOGUE, GLOBAL_LOOKUP, GLOBAL_ASSIGN);

        if (core) return core;

        LOG_ERROR(mux_module, "Failed to build global core")
    }

    load_assign(MUOS_ASS_LOAD, file_name, sys_dir, "none", force, 0);
    if (run_quit) mux_input_stop();

    LOG_INFO(mux_module, "No core detected")
    return NULL;
}

char *build_core(char core_path[MAX_BUFFER_SIZE], int line_core, int line_system,
                 int line_catalogue, int line_lookup, int line_launch) {
    const char *core_line = read_line_char_from(core_path, line_core) ?: "unknown";
    const char *system_line = read_line_char_from(core_path, line_system) ?: "unknown";
    const char *catalogue_line = read_line_char_from(core_path, line_catalogue) ?: "unknown";
    const char *lookup_line = read_line_char_from(core_path, line_lookup) ?: "unknown";
    const char *launch_line = read_line_char_from(core_path, line_launch) ?: "unknown";

    size_t required_size = snprintf(NULL, 0, "%s\n%s\n%s\n%s\n%s",
                                    core_line, system_line, catalogue_line,
                                    lookup_line, launch_line) + 1;

    char *b_core = malloc(required_size);
    if (!b_core) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
        return NULL;
    }

    snprintf(b_core, required_size, "%s\n%s\n%s\n%s\n%s",
             core_line, system_line, catalogue_line, lookup_line, launch_line);

    return b_core;
}

void add_to_collection(char *filename, const char *pointer, char *sys_dir) {
    play_sound(SND_CONFIRM);

    char new_content[MAX_BUFFER_SIZE];
    snprintf(new_content, sizeof(new_content), "%s\n%s\n%s",
             filename, pointer, get_last_subdir(sys_dir, '/', 4));

    write_text_to_file(ADD_MODE_WORK, "w", CHAR, new_content);

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    load_mux("collection");

    close_input();
    mux_input_stop();
}

int set_scaling_governor(const char *governor, int show_done) {
    if (!governor) return -1;

    FILE *fp = fopen(device.CPU.GOVERNOR, "w");
    if (!fp) {
        LOG_ERROR(mux_module, "Failed to open %s: %s", device.CPU.GOVERNOR, strerror(errno))
        return -1;
    }

    if (fprintf(fp, "%s", governor) < 0) {
        LOG_ERROR(mux_module, "Failed to write '%s' to %s: %s", governor, device.CPU.GOVERNOR, strerror(errno))
        fclose(fp);
        return -1;
    }

    if (show_done) LOG_SUCCESS(mux_module, "Governor switched to '%s' successfully", governor)

    fclose(fp);
    return 0;
}

void turbo_time(int toggle, int show_done) {
    set_scaling_governor(toggle ? "performance" : device.CPU.DEFAULT, show_done);
}

int bc64(uint64_t n) {
    int count = 0;
    while (n) {
        n &= (n - 1);
        ++count;
    }
    return count;
}

char **split_command(const char *cmd, size_t *argc_out) {
#define FREE_ARGV                                        \
    do {                                                 \
        for (size_t i = 0; i < argc; i++) free(argv[i]); \
        free(argv);                                      \
        return NULL;                                     \
    } while (0)

    if (!cmd || !*cmd) return NULL;

    size_t argc = 0;
    size_t cap = 8;

    char **argv = malloc(cap * sizeof(char *));
    if (!argv) return NULL;

    const char *p = cmd;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char buf[MAX_BUFFER_SIZE];
        size_t len = 0;
        bool in_quote = false;
        char quote_char = 0;

        while (*p && (in_quote || (*p != ' ' && *p != '\t'))) {
            if (*p == '\\') {
                p++;
                if (*p) buf[len++] = *p++;
            } else if (!in_quote && (*p == '\'' || *p == '"')) {
                in_quote = true;
                quote_char = *p++;
            } else if (in_quote && *p == quote_char) {
                in_quote = false;
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
            size_t new_cap = cap * 2;

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

void free_array(char **array, size_t count) {
    if (!array) return;
    for (size_t i = 0; i < count; ++i) free(array[i]);
    free(array);
}

int scan_directory_list(const char *dirs[], const char *exts[], char ***results,
                        size_t dir_count, size_t ext_count, size_t *result_count) {
    size_t count = 0;
    size_t capacity = 0;
    char **list = NULL;

    for (size_t d = 0; d < dir_count; ++d) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;

        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (ent->d_type != DT_REG) continue;

            const char *dot = strrchr(ent->d_name, '.');
            if (!dot) continue;

            int match = 0;
            for (size_t e = 0; e < ext_count; ++e) {
                if (strcasecmp(dot, exts[e]) == 0) {
                    match = 1;
                    break;
                }
            }

            if (!match) continue;

            if (count >= capacity) {
                size_t new_capacity = (capacity == 0) ? 8 : capacity * 2;
                char **tmp = realloc(list, new_capacity * sizeof(char *));

                if (!tmp) {
                    LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM)
                    free_array(list, count);
                    closedir(dir);
                    return -1;
                }

                memset(tmp + capacity, 0, (new_capacity - capacity) * sizeof(char *));

                list = tmp;
                capacity = new_capacity;
            }

            char full[MAX_BUFFER_SIZE];
            snprintf(full, sizeof(full), "%s/%s", dirs[d], ent->d_name);

            list[count] = strdup(full);
            if (!list[count]) {
                LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DUP_STRING)
                free_array(list, count);
                closedir(dir);
                return -1;
            }

            count++;
        }

        closedir(dir);
    }

    if (count == 0) {
        free(list);
        list = NULL;
    }

    *results = list;
    *result_count = count;

    return 0;
}
