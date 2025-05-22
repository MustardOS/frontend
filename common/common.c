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
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../lvgl/lvgl.h"
#include "../font/notosans.h"
#include "../font/notosans_jp.h"
#include "../font/notosans_ar.h"
#include "../font/notosans_kr.h"
#include "../font/notosans_sc.h"
#include "../font/notosans_tc.h"
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
#include "theme.h"
#include "mini/mini.h"

char mux_module[MAX_BUFFER_SIZE];
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
const char **img_paths = NULL;
int img_paths_count = 0;
const char **history_items = NULL;
int history_item_count = 0;
const char **collection_items = NULL;
int collection_item_count = 0;
char current_wall[MAX_BUFFER_SIZE];
lv_obj_t *wall_img = NULL;
struct grid_info grid_info;
CachedSound sound_cache[SOUND_TOTAL];
int is_silence_playing = 0;
Mix_Music *current_bgm = NULL;
char **bgm_files = NULL;
size_t bgm_file_count = 0;
int current_brightness = 0;
int current_volume = 0;

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
    exec[i++] = (INTERNAL_PATH "extra/muterm");
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

void extract_archive(char *filename) {
    size_t exec_count;
    const char *args[] = {(INTERNAL_PATH "script/mux/extract.sh"), filename, NULL};
    const char **exec = build_term_exec(args, &exec_count);

    if (exec) {
        config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
        run_exec(exec, exec_count, 0);
    }
    free(exec);
}

unsigned long long total_file_size(const char *path) {
    long long total_size = 0;
    struct dirent *entry;

    DIR *dir = opendir(path);
    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            struct stat file_stat;
            if (stat(full_path, &file_stat) != -1) {
                if (S_ISDIR(file_stat.st_mode)) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        total_size += total_file_size(full_path);
                    }
                } else if (S_ISREG(file_stat.st_mode)) {
                    total_size += file_stat.st_size;
                }
            }
        }
    }

    closedir(dir);
    return total_size;
}

int str_compare(const void *a, const void *b) {
    const char *str1 = *(const char **) a;
    const char *str2 = *(const char **) b;

    while (*str1 && *str2) {
        char c1 = tolower(*str1);
        char c2 = tolower(*str2);

        if (c1 != c2) return c1 - c2;

        str1++;
        str2++;
    }

    return *str1 - *str2;
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
    char *ptr = text;

    while (*ptr) {
        *ptr = tolower((unsigned char) *ptr);
        ptr++;
    }

    return text;
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

char *get_last_subdir(char *text, char separator, int n) {
    char *ptr = text;
    int count = 0;

    while (*ptr && count < n) {
        if (*ptr == separator) count++;
        ptr++;
    }

    return (*ptr) ? ptr : text;
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

int read_battery_capacity() {
    FILE *file = fopen(device.BATTERY.CAPACITY, "r");

    if (file == NULL) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return 0;
    }

    int capacity;
    if (fscanf(file, "%d", &capacity) != 1) {
        perror(lang.SYSTEM.FAIL_FILE_READ);
        return 0;
    }

    fclose(file);

    capacity += (config.SETTINGS.ADVANCED.OFFSET - 50);
    return capacity > 100 ? 100 : capacity;
}

char *read_battery_voltage() {
    FILE *file = fopen(device.BATTERY.VOLTAGE, "r");

    if (file == NULL) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return "0.00 V";
    }

    int raw_voltage;
    if (fscanf(file, "%d", &raw_voltage) != 1) {
        perror(lang.SYSTEM.FAIL_FILE_READ);
        fclose(file);
        return "0.00 V";
    }

    fclose(file);

    char *form_voltage = (char *) malloc(10);
    if (form_voltage == NULL) {
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
        return "0.00 V";
    }

    snprintf(form_voltage, 8, "%.2f V", raw_voltage / 1000000.0);
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
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
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
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return "";
    }

    char *line = (char *) malloc(MAX_BUFFER_SIZE);
    if (!line) {
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
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
    if (!fgets(line, sizeof(line), file)) {
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

    unsigned long long value = 0;

    fscanf(file, "%llu", &value);
    fclose(file);

    return value;
}

const char *get_random_hex() {
    int red = rand() % UINT8_MAX;
    int green = rand() % UINT8_MAX;
    int blue = rand() % UINT8_MAX;

    char *colour_hex = (char *) malloc(7 * sizeof(char));
    sprintf(colour_hex, "%02X%02X%02X", red, green, blue);

    return colour_hex;
}

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key, uint32_t default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    uint32_t result;
    if (strcmp(meta, "NOT FOUND") == 0) {
        result = default_value;
    } else {
        result = (uint32_t) strtoul(meta, NULL, 16);
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
        perror(lang.SYSTEM.FAIL_FILE_WRITE);
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

void show_help_msgbox(lv_obj_t *panel, lv_obj_t *header_element, lv_obj_t *content_element,
                      char *header_text, char *content_text) {
    if (msgbox_active == 0) {
        msgbox_active = 1;
        msgbox_element = panel;
        lv_label_set_text(header_element, header_text);
        lv_label_set_text(content_element, content_text);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_rom_info(lv_obj_t *panel, lv_obj_t *i_title, lv_obj_t *p_title, lv_obj_t *i_desc,
                   char *t_title, char *t_desc) {
    if (msgbox_active == 0) {
        msgbox_active = 1;
        msgbox_element = panel;
        lv_label_set_text(i_title, t_title);
        lv_label_set_text(p_title, t_title);
        lv_label_set_text(i_desc, t_desc);
        lv_obj_t *ui_pnlItem = lv_obj_get_parent(i_desc);
        lv_obj_scroll_to_y(ui_pnlItem, 0, LV_ANIM_OFF);
    }
}

void nav_move(lv_group_t *group, int direction) {
    (direction < 0 ? nav_prev : nav_next)(group, 1);
}

void nav_prev(lv_group_t *group, int count) {
    int i;
    for (i = 0; i < count; i++) lv_group_focus_prev(group);
}

void nav_next(lv_group_t *group, int count) {
    int i;
    for (i = 0; i < count; i++) lv_group_focus_next(group);
}

char *get_datetime() {
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

char *get_capacity() {
    char *battery_glyph_name = read_line_int_from(device.BATTERY.CHARGER, 1) ? "capacity_charging_" : "capacity_";

    static char capacity_str[MAX_BUFFER_SIZE];
    switch (battery_capacity) {
        case -255 ... 5:
            snprintf(capacity_str, sizeof(capacity_str), "%s0", battery_glyph_name);
            break;
        case 6 ... 10:
            snprintf(capacity_str, sizeof(capacity_str), "%s10", battery_glyph_name);
            break;
        case 11 ... 20:
            snprintf(capacity_str, sizeof(capacity_str), "%s20", battery_glyph_name);
            break;
        case 21 ... 30:
            snprintf(capacity_str, sizeof(capacity_str), "%s30", battery_glyph_name);
            break;
        case 31 ... 40:
            snprintf(capacity_str, sizeof(capacity_str), "%s40", battery_glyph_name);
            break;
        case 41 ... 50:
            snprintf(capacity_str, sizeof(capacity_str), "%s50", battery_glyph_name);
            break;
        case 51 ... 60:
            snprintf(capacity_str, sizeof(capacity_str), "%s60", battery_glyph_name);
            break;
        case 61 ... 70:
            snprintf(capacity_str, sizeof(capacity_str), "%s70", battery_glyph_name);
            break;
        case 71 ... 80:
            snprintf(capacity_str, sizeof(capacity_str), "%s80", battery_glyph_name);
            break;
        case 81 ... 90:
            snprintf(capacity_str, sizeof(capacity_str), "%s90", battery_glyph_name);
            break;
        case 91 ... 255:
            snprintf(capacity_str, sizeof(capacity_str), "%s100", battery_glyph_name);
            break;
        default:
            snprintf(capacity_str, sizeof(capacity_str), "%s0", battery_glyph_name);
            break;
    }

    return capacity_str;
}

void capacity_task() {
    battery_capacity = read_battery_capacity();
}

void increase_option_value(lv_obj_t *element) {
    uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;
    uint16_t current = lv_dropdown_get_selected(element);

    play_sound(SND_OPTION, 0);

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

    play_sound(SND_OPTION, 0);

    if (current > 0) {
        current--;
        lv_dropdown_set_selected(element, current);
    } else {
        current = (total - 1);
        lv_dropdown_set_selected(element, current);
    }
}

void load_assign(const char *rom, const char *dir, const char *sys, int forced) {
    FILE *file = fopen(MUOS_ASS_LOAD, "w");
    if (file == NULL) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d", rom, dir, sys, forced);
    fclose(file);
}

void load_gov(const char *rom, const char *dir, const char *sys, int forced) {
    FILE *file = fopen(MUOS_GOV_LOAD, "w");
    if (file == NULL) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d", rom, dir, sys, forced);
    fclose(file);
}

void load_mux(const char *value) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void play_sound(int sound, int wait) {
    if (!fe_snd || sound < 0 || sound >= SOUND_TOTAL) return;

    CachedSound *cs = &sound_cache[sound];
    if (cs->chunk) {
        int channel = Mix_PlayChannel(-1, cs->chunk, 0);
        if (wait) while (Mix_Playing(channel)) SDL_Delay(5);
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
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
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
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
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

    for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); ++i) {
        for (size_t j = 0; j < sizeof(paths) / sizeof(paths[0]); ++j) {
            for (size_t k = 0; k < sizeof(elements) / sizeof(elements[0]); ++k) {
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

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
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

int load_image_catalogue(const char *catalogue_name, const char *program, const char *program_fallback,
                         const char *mux_dimension, const char *image_type, char *image_path, size_t path_size) {
    const char *paths[] = {
            "%s/%s/%s/%s%s.png",
            "%s/%s/%s/%s.png"
    };

    const char *programs[] = {program, program_fallback};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        for (size_t j = 0; j < sizeof(programs) / sizeof(programs[0]); ++j) {
            int written;

            switch (i) {
                case 0:
                    written = snprintf(image_path, path_size, paths[i], INFO_CAT_PATH, catalogue_name,
                                       image_type, mux_dimension, programs[j]);
                    break;
                case 1:
                default:
                    written = snprintf(image_path, path_size, paths[i], INFO_CAT_PATH, catalogue_name,
                                       image_type, programs[j]);
            }

            if (written >= 0 && file_exist(image_path)) return 1;
        }
    }

    return 0;
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated, int random, int wall_type) {
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    const char *wall_extension = random ? "0.png" : (animated == 1 ? "gif" : (animated == 2 ? "0.png" : "png"));

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
        const char *element = lv_obj_get_user_data(element_focused);
        switch (wall_type) {
            case APPLICATION:
                if (load_image_catalogue("Application", element, "default", mux_dimension, "wall",
                                         wall_image_path, sizeof(wall_image_path))) {
                    int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                    if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
                    return wall_image_embed;
                }
                break;
            case ARCHIVE:
                if (load_image_catalogue("Archive", element, "default", mux_dimension, "wall",
                                         wall_image_path, sizeof(wall_image_path))) {
                    int written = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                    if (written < 0 || (size_t) written >= sizeof(wall_image_embed)) return "";
                    return wall_image_embed;
                }
                break;
            case TASK:
                if (load_image_catalogue("Task", element, "default", mux_dimension, "wall",
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
        if (load_element_image_specifics(STORAGE_THEME, mux_dimension, program, "wall", element,
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
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        const char *element = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        switch (wall_type) {
            case APPLICATION:
                if (load_image_catalogue("Application", element, "default", mux_dimension, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case ARCHIVE:
                if (load_image_catalogue("Archive", element, "default", mux_dimension, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case TASK:
                if (load_image_catalogue("Task", element, "default", mux_dimension, "box",
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
                                                 element, "default", "png", static_image_path,
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

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

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
        lv_obj_set_style_img_opa(overlay_image, config.VISUAL.OVERLAY_TRANSPARENCY, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(overlay_image);
    }
}

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image) {
    if (file_exist(KIOSK_CONFIG)) {
        const char *program = lv_obj_get_user_data(ui_screen);

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

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
}

int load_terminal_resource(const char *resource, const char *extension, char *buffer, size_t size) {
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

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
        fprintf(stderr, "Error: base_path exceeds maximum allowed length\n");
        exit(EXIT_FAILURE);
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
                perror("malloc failed");
                exit(EXIT_FAILURE);
            }

            snprintf(path_embed, needed_size, "%s.%d.png", base_path, index);
            img_paths = realloc(img_paths, (img_paths_count + 1) * sizeof(char *));
            if (!img_paths) {
                perror("realloc failed");
                free(path_embed);
                exit(EXIT_FAILURE);
            }

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

void unload_image_animation() {
    if (lv_obj_is_valid(wall_img)) lv_obj_del(wall_img);
    if (lv_obj_is_valid(img_obj)) lv_anim_del(img_obj, NULL);
}

const lv_font_t *get_language_font() {
    if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Chinese (Simplified)") == 0) {
        return &ui_font_NotoSans_SC;
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Chinese (Traditional)") == 0) {
        return &ui_font_NotoSans_TC;
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Japanese") == 0) {
        return &ui_font_NotoSans_JP;
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Arabic") == 0) {
        return &ui_font_NotoSans_AR;
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Korean") == 0) {
        return &ui_font_NotoSans_KR;
    } else {
        return &ui_font_NotoSans;
    }
}

void load_font_text_from_file(const char *filepath, lv_obj_t *element) {
    char theme_font_text_fs[MAX_BUFFER_SIZE];
    snprintf(theme_font_text_fs, sizeof(theme_font_text_fs), "M:%s", filepath);
    lv_font_t *font = lv_font_load(theme_font_text_fs);
    font->fallback = get_language_font();
    lv_obj_set_style_text_font(element, font, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void get_mux_dimension(char *mux_dimension, size_t size) {
    snprintf(mux_dimension, size, "%dx%d/", device.MUX.WIDTH, device.MUX.HEIGHT);
}

void load_font_text(lv_obj_t *screen) {
    const lv_font_t *language_font = get_language_font();

    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_text_default[MAX_BUFFER_SIZE];
        char theme_font_text[MAX_BUFFER_SIZE];

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        char *mux_dimensions[15] = {mux_dimension, ""};
        char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;

        if (theme_compat()) {
            for (int i = 0; i < 2; i++) {
                if ((snprintf(theme_font_text, sizeof(theme_font_text),
                              "%s/%sfont/%s/%s.bin", theme_location, mux_dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, mux_module) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text_default),
                              "%s/%sfont/%s/default.bin", theme_location, mux_dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text),
                              "%s/%sfont/%s.bin", theme_location, mux_dimensions[i], mux_module) >= 0 &&
                     file_exist(theme_font_text)) ||

                    (snprintf(theme_font_text, sizeof(theme_font_text_default),
                              "%s/%sfont/default.bin", theme_location, mux_dimensions[i]) >= 0 &&
                     file_exist(theme_font_text))) {

                    LOG_INFO(mux_module, "Loading Main Theme Font: %s", theme_font_text)
                    load_font_text_from_file(theme_font_text, screen);
                    return;
                }
            }
        }
    }

    LOG_INFO(mux_module, "Loading Default Language Font")
    lv_obj_set_style_text_font(screen, language_font, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void load_font_section(const char *section, lv_obj_t *element) {
    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_section[MAX_BUFFER_SIZE];

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        char *mux_dimensions[15] = {mux_dimension, ""};
        char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;

        if (theme_compat()) {
            for (int i = 0; i < 2; i++) {
                if ((snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s/%s.bin", theme_location, mux_dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, section, mux_module) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s/default.bin", theme_location, mux_dimensions[i],
                              config.SETTINGS.GENERAL.LANGUAGE, section) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/%s.bin", theme_location, mux_dimensions[i], section, mux_module) >= 0 &&
                     file_exist(theme_font_section)) ||

                    (snprintf(theme_font_section, sizeof(theme_font_section),
                              "%s/%sfont/%s/default.bin", theme_location, mux_dimensions[i], section) >= 0 &&
                     file_exist(theme_font_section))) {

                    LOG_INFO(mux_module, "Loading Section '%s' Font: %s", section, theme_font_section)
                    load_font_text_from_file(theme_font_section, element);
                    return;
                }
            }
        }
    }
}

int is_network_connected() {
    if (file_exist(device.NETWORK.STATE)) {
        if (strcasecmp("up", read_all_char_from(device.NETWORK.STATE)) == 0) return 1;
    }

    return 0;
}

void process_visual_element(enum visual_type visual, lv_obj_t *element) {
    switch (visual) {
        case CLOCK:
            if (!config.VISUAL.CLOCK) {
                lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case BLUETOOTH:
            if (!config.VISUAL.BLUETOOTH) {
                lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case NETWORK:
            if (!config.VISUAL.NETWORK) {
                lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case BATTERY:
            if (!config.VISUAL.BATTERY) {
                lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            }
            break;
    }
}

void load_skip_patterns() {
    char skip_ini[MAX_BUFFER_SIZE];
    int written = snprintf(skip_ini, sizeof(skip_ini), "%s/%s/skip.ini", device.STORAGE.SDCARD.MOUNT,
                           MUOS_INFO_PATH);
    if (written < 0 || (size_t) written >= sizeof(skip_ini)) return;

    if (!file_exist(skip_ini)) {
        written = snprintf(skip_ini, sizeof(skip_ini), "%s/%s/skip.ini", device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH);
        if (written < 0 || (size_t) written >= sizeof(skip_ini)) return;
    }

    FILE *file = fopen(skip_ini, "r");
    if (!file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    for (size_t i = 0; i < skip_pattern_list.count; i++) {
        free(skip_pattern_list.patterns[i]);
    }
    free(skip_pattern_list.patterns);

    skip_pattern_list.count = 0;
    skip_pattern_list.capacity = 2;
    skip_pattern_list.patterns = malloc(skip_pattern_list.capacity * sizeof(char *));
    if (!skip_pattern_list.patterns) {
        perror("malloc failed");
        fclose(file);
        return;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (skip_pattern_list.count >= skip_pattern_list.capacity) {
            skip_pattern_list.capacity *= 2;
            skip_pattern_list.patterns = realloc(skip_pattern_list.patterns,
                                                 skip_pattern_list.capacity * sizeof(char *));
            if (!skip_pattern_list.patterns) {
                perror("realloc failed");
                fclose(file);
                return;
            }
        }

        skip_pattern_list.patterns[skip_pattern_list.count] = strdup(line);
        if (!skip_pattern_list.patterns[skip_pattern_list.count]) {
            perror("strdup failed");
            fclose(file);
            return;
        }
        skip_pattern_list.count++;
    }

    fclose(file);
}

int should_skip(const char *name) {
    for (size_t i = 0; i < skip_pattern_list.count; i++) {
        if (fnmatch(skip_pattern_list.patterns[i], name, 0) == 0) {
            return 1;
        }
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
    lv_obj_set_style_text_color(ui_conTest, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_conTest, spec, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_conTest, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_conTest, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    int text_index = 0;
    int with_bracket = 0;

    char b_open_1, b_open_2, b_close_1, b_close_2;
    b_open_1 = b_open_2 = b_close_1 = b_close_2 = '\0';

    if (method == 1) {
        b_open_1 = '[';
        b_close_1 = ']';
    } else if (method == 2) {
        b_open_1 = '(';
        b_close_1 = ')';
    } else if (method == 3) {
        b_open_1 = '(';
        b_open_2 = '[';
        b_close_1 = ')';
        b_close_2 = ']';
    }

    for (int i = 0; i < strlen(text); i++) {
        if (text[i] == b_open_1 || (method == 3 && text[i] == b_open_2)) {
            with_bracket = 1;
        } else if (text[i] == b_close_1 || (method == 3 && text[i] == b_close_2)) {
            with_bracket = 0;
        } else if (!with_bracket) {
            text[text_index++] = text[i];
        }
    }

    text[text_index] = '\0';

    int start = 0;
    while (isspace((unsigned char) text[start])) {
        start++;
    }

    int end = strlen(text) - 1;
    while (end >= 0 && isspace((unsigned char) text[end])) {
        end--;
    }

    if (start > 0 || end < (int) strlen(text) - 1) {
        for (int i = start; i <= end; i++) {
            text[i - start] = text[i];
        }
        text[end - start + 1] = '\0';
    }

    if (rep_dash) {
        char *found = strstr(text, " - ");
        if (found != NULL) {
            size_t offset = found - text;
            text[offset] = ':';
            memmove(text + offset + 2, text + offset + 3, strlen(text) - offset - 2);
            text[offset + 1] = ' ';
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

            float width_ratio = (float) image_settings.max_width / img_header.w;
            float height_ratio = (float) image_settings.max_height / img_header.h;
            float zoom_ratio = (width_ratio < height_ratio) ? width_ratio : height_ratio;

            int zoom_factor = (int) (zoom_ratio * 256);

            lv_img_set_size_mode(ui_imgobj, LV_IMG_SIZE_MODE_REAL);
            lv_img_set_zoom(ui_imgobj, zoom_factor);
        }

        lv_obj_set_align(ui_imgobj, image_settings.align);
        lv_obj_set_style_pad_left(ui_imgobj, image_settings.pad_left, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_imgobj, image_settings.pad_right, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_imgobj, image_settings.pad_top, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_imgobj, image_settings.pad_bottom, LV_PART_MAIN | LV_STATE_DEFAULT);
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

    int content_panel_y = scroll_multiplier * mux_item_panel;
    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
    lv_obj_update_snap(ui_pnlContent, LV_ANIM_OFF);
}

void load_language_file(const char *module) {
    char language_file[MAX_BUFFER_SIZE];
    snprintf(language_file, sizeof(language_file), (RUN_STORAGE_PATH "language/%s.json"),
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
    int buffer_size = 0;
    int prefix_len = prefix ? strlen(prefix) : 0;
    int infix_len = infix ? strlen(infix) : 0;
    int suffix_len = suffix ? strlen(suffix) : 0;

    if (prefix) {
        buffer_size += prefix_len + 1;
    }
    for (int i = min; (increment > 0 ? i <= max : i >= max); i += increment) {
        buffer_size += snprintf(NULL, 0, "%d", i);
        if (infix) {
            buffer_size += infix_len;
        }
        if ((increment > 0 ? i + increment <= max : i + increment >= max)) {
            buffer_size += 1;
        }
    }
    if (suffix) {
        buffer_size += suffix_len;
    }

    char *number_string = (char *) malloc(buffer_size + 1);
    if (number_string == NULL) {
        return NULL;
    }

    char *ptr = number_string;
    if (prefix) {
        ptr += sprintf(ptr, "%s\n", prefix);
    }
    for (int i = min; (increment > 0 ? i <= max : i >= max); i += increment) {
        if (infix && infix_position == 0) {
            ptr += sprintf(ptr, "%s", infix);
        }
        ptr += sprintf(ptr, "%d", i);
        if (infix && infix_position == 1) {
            ptr += sprintf(ptr, "%s", infix);
        }
        if ((increment > 0 ? i + increment <= max : i + increment >= max)) {
            *ptr++ = '\n';
        }
    }
    if (suffix) {
        ptr += sprintf(ptr, "%s", suffix);
    }
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

int resolution_check(const char *zip_filename) {
    printf("Inspecting theme for supported resolutions: %s\n", zip_filename);
    const char *resolutions[] = {"640x480", "720x480", "720x576", "720x720", "1024x768", "1280x720"};
    size_t num_resolutions = sizeof(resolutions) / sizeof(resolutions[0]);

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, zip_filename, 0)) {
        printf("Failed to open ZIP archive!\n");
        return 0;
    }

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;

        const char *filename = file_stat.m_filename;
        char *slash_pos = strchr(filename, '/'); // Find first '/'

        if (slash_pos && slash_pos == strrchr(filename, '/')) { // Ensure it's a root folder
            size_t folder_length = slash_pos - filename;

            // Extract folder name
            char folder_name[256];
            strncpy(folder_name, filename, folder_length);
            folder_name[folder_length] = '\0';

            // Check if the folder name matches any target resolutions
            for (size_t j = 0; j < num_resolutions; j++) {
                if (strcmp(folder_name, resolutions[j]) == 0) {
                    mz_zip_reader_end(&zip);
                    printf("Found supported resolution\n");
                    return 1;  // Found a match, exit early
                }
            }
        }
    }
    mz_zip_reader_end(&zip);
    printf("No supported resolutions found\n");
    return 0;
}

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        LOG_ERROR(mux_module, "Could not open archive '%s' - Corrupt?", zip_path)
        return 0;
    }

    int file_index = mz_zip_reader_locate_file(&zip, file_name, NULL, 0);
    if (file_index == -1) {
        LOG_ERROR(mux_module, "File '%s' not found in archive", file_name)
        mz_zip_reader_end(&zip);
        return 0;
    }

    if (!mz_zip_reader_extract_to_file(&zip, file_index, output_path, 0)) {
        LOG_ERROR(mux_module, "File '%s' could not be extracted", file_name)
        mz_zip_reader_end(&zip);
        return 0;
    }

    mz_zip_reader_end(&zip);
    return 1;
}

void add_directory_to_list(char ***list, int *size, int *count, const char *dir) {
    if (*count >= *size) {
        *size += 10;
        *list = realloc(*list, *size * sizeof(char *));
    }

    (*list)[*count] = strdup(dir);
    (*count)++;
}

void collect_subdirectories(const char *base_dir, char ***list, int *size, int *count, int trim_start_count) {
    char subdir_path[PATH_MAX];
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            if (entry->d_type == DT_DIR) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    snprintf(subdir_path, sizeof(subdir_path), "%s/%s", base_dir, entry->d_name);
                    const char *trimmed_path = subdir_path + trim_start_count;
                    add_directory_to_list(list, size, count, trimmed_path);
                    collect_subdirectories(subdir_path, list, size, count, trim_start_count);
                }
            }
        }
    }
    closedir(dir);
}

char **get_subdirectories(const char *base_dir) {
    int trim_start_count = strlen(base_dir) + 1;
    int list_size = 10;
    int count = 0;

    char **subdir_list = malloc(list_size * PATH_MAX);
    collect_subdirectories(base_dir, &subdir_list, &list_size, &count, trim_start_count);
    subdir_list[count] = NULL;

    return subdir_list;
}


void free_subdirectories(char **dir_names) {
    if (dir_names == NULL) return;

    for (int i = 0; dir_names[i] != NULL; i++) {
        free(dir_names[i]);
    }
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

void free_bgm_list(void) {
    for (size_t i = 0; i < bgm_file_count; ++i) free(bgm_files[i]);
    free(bgm_files);

    bgm_files = NULL;
    bgm_file_count = 0;
}

void free_bgm(void) {
    if (current_bgm) {
        Mix_HaltMusic();
        Mix_FreeMusic(current_bgm);
        current_bgm = NULL;
    }
}

void play_random_bgm(void) {
    if (bgm_file_count == 0) return;

    static size_t last_index = SIZE_MAX;
    size_t index;

    if (bgm_file_count == 1) {
        index = 0;
    } else {
        index = rand() % bgm_file_count;
        while (index == last_index) index = rand() % bgm_file_count;
    }

    const char *path = bgm_files[index];
    last_index = index;

    free_bgm();

    current_bgm = Mix_LoadMUS(path);
    if (current_bgm) {
        is_silence_playing = 0;
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

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
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
    bgm_file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".ogg") == 0) {
            if (bgm_file_count >= capacity) {
                capacity *= 2;
                bgm_files = realloc(bgm_files, capacity * sizeof(char *));
            }

            char full_path[MAX_BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
            bgm_files[bgm_file_count++] = strdup(full_path);
        }
    }
    closedir(dir);

    if (bgm_file_count > 0) {
        srand(time(NULL));
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

char *kiosk_nope() {
    play_sound(SND_ERROR, 0);
    return lang.GENERIC.KIOSK_DISABLE;
}

void run_exec(const char *args[], size_t size, int background) {
    const char *san[size];

    size_t j = 0;
    for (size_t i = 0; i < size; ++i) {
        if (args[i]) {
            san[j++] = args[i];
        }
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
        execvp(san[0], (char *const *) san);
        _exit(EXIT_FAILURE);
    } else if (pid > 0 && !background) {
        waitpid(pid, NULL, 0);
    }
}

char *get_directory_core(char *rom_dir, size_t line_number) {
    char content_core[MAX_BUFFER_SIZE];
    snprintf(content_core, sizeof(content_core), "%s/%s/core.cfg",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));
    if (file_exist(content_core)) {
        return read_line_char_from(content_core, line_number);
    }
    return "";
}

char *get_file_core(char *rom_dir, char *rom_name) {
    char content_core[MAX_BUFFER_SIZE];
    snprintf(content_core, sizeof(content_core), "%s/%s/%s.cfg",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4), strip_ext(rom_name));
    if (file_exist(content_core)) {
        return read_line_char_from(content_core, 2);
    }
    return "";
}

char *get_directory_governor(char *rom_dir) {
    char content_governor[MAX_BUFFER_SIZE];
    snprintf(content_governor, sizeof(content_governor), "%s/%s/core.gov",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));
    if (file_exist(content_governor)) {
        return read_line_char_from(content_governor, 1);
    }
    return "";
}

char *get_file_governor(char *rom_dir, char *rom_name) {
    char content_governor[MAX_BUFFER_SIZE];
    snprintf(content_governor, sizeof(content_governor), "%s/%s/%s.gov",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4), strip_ext(rom_name));
    if (file_exist(content_governor)) {
        return read_line_char_from(content_governor, 1);
    }
    return "";
}

char *get_directory_tag(char *rom_dir) {
    char content_tag[MAX_BUFFER_SIZE];
    snprintf(content_tag, sizeof(content_tag), "%s/%s/core.tag",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));
    if (file_exist(content_tag)) {
        return read_line_char_from(content_tag, 1);
    }
    return "";
}

char *get_file_tag(char *rom_dir, char *rom_name) {
    char content_tag[MAX_BUFFER_SIZE];
    snprintf(content_tag, sizeof(content_tag), "%s/%s/%s.tag",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4), strip_ext(rom_name));
    if (file_exist(content_tag)) {
        return read_line_char_from(content_tag, 1);
    }
    return "";
}

struct screen_dimension get_device_dimensions() {
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
    uint32_t flags[] = {LV_OBJ_FLAG_HIDDEN, LV_OBJ_FLAG_FLOATING};

    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < sizeof(flags) / sizeof(flags[0]); ++j) {
            if (nav_flags[i].visible) {
                lv_obj_clear_flag(nav_flags[i].element, flags[j]);
            } else {
                lv_obj_add_flag(nav_flags[i].element, flags[j]);
            }
        }
    }
}

int16_t validate_int16(int value, const char *field) {
    if (value < INT16_MIN || value > INT16_MAX) {
        perror(lang.SYSTEM.FAIL_INT16_LENGTH);
        return (value < INT16_MIN) ? INT16_MIN : INT16_MAX;
    }
    return (int16_t) value;
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
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
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

void populate_items(const char *base_path, const char ***items, int *item_count) {
    struct dirent *entry;
    char full_path[PATH_MAX];
    DIR *dir = opendir(base_path);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            perror(lang.SYSTEM.FAIL_STAT);
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

void populate_history_items() {
    populate_items(INFO_HIS_PATH, &history_items, &history_item_count);
}

void populate_collection_items() {
    populate_items(INFO_COL_PATH, &collection_items, &collection_item_count);
}

char *get_content_explorer_glyph_name(char *file_path) {
    for (int i = 0; i < collection_item_count; i++) {
        if (strcmp(collection_items[i], file_path) == 0) {
            return "collection";
        }
    }

    for (int i = 0; i < history_item_count; i++) {
        if (strcmp(history_items[i], file_path) == 0) {
            return "history";
        }
    }

    return "rom";
}

uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 2166136261U; // FNV offset basis
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t) (*p);
        hash *= 16777619; // FNV prime
    }
    return hash;
}

bool get_glyph_path(const char *mux_module, const char *glyph_name,
                    char *glyph_image_embed, size_t glyph_image_embed_size) {
    char glyph_image_path[MAX_BUFFER_SIZE];
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));
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

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count, int *nav_moved) {
    if (!file_exist(MUOS_PDI_LOAD)) return 0;

    char *prev = read_all_char_from(MUOS_PDI_LOAD);
    if (!prev) return 0;

    int text_hit = 0;
    for (size_t i = 0; i < ui_count; i++) {
        if (!strcasecmp(lv_obj_get_user_data(ui_objects[i]), prev)) {
            text_hit = i;
            break;
        }
    }

    int nav_next_return = 0;
    if (text_hit > 0) {
        *nav_moved = 1;
        if (!strcmp(mux_module, "muxtweakgen")) {
            nav_next_return = text_hit - !device.DEVICE.HAS_HDMI;
        } else if (!strcmp(mux_module, "muxtweakadv")) {
            nav_next_return = text_hit - !device.DEVICE.HAS_NETWORK;
        } else if (strcmp(mux_module, "muxconfig") && !config.NETWORK.TYPE && !strcasecmp(prev, "connect")) {
            nav_next_return = 4;
        } else {
            nav_next_return = text_hit;
        }
    }

    free(prev);
    remove(MUOS_PDI_LOAD);

    return nav_next_return;
}

int theme_compat() {
    char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;
    char theme_version_file[MAX_BUFFER_SIZE];
    snprintf(theme_version_file, sizeof(theme_version_file), "%s/version.txt", theme_location);

    if (file_exist(theme_version_file)) {
        char *theme_version = read_line_char_from(theme_version_file, 1);
        char *internal_version = read_line_char_from((INTERNAL_PATH "config/version.txt"), 1);
        if (strstr(internal_version, theme_version)) {
            return 1;
        } else {
            LOG_WARN(mux_module, "Incompatible Theme Detected: %s", theme_version)
        }
    } else {
        LOG_WARN(mux_module, "Missing Theme Version File or Version Content")
    }

    return 0;
}

void update_bootlogo() {
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));
    char bootlogo_image[MAX_BUFFER_SIZE];

    snprintf(bootlogo_image, sizeof(bootlogo_image), "%s/%simage/bootlogo.bmp", STORAGE_THEME, mux_dimension);
    if (!file_exist(bootlogo_image)) {
        snprintf(bootlogo_image, sizeof(bootlogo_image), "%s/image/bootlogo.bmp", STORAGE_THEME);
    }

    if (file_exist(bootlogo_image)) {
        char bootlogo_dest[MAX_BUFFER_SIZE];
        snprintf(bootlogo_dest, sizeof(bootlogo_dest), "%s/bootlogo.bmp", device.STORAGE.BOOT.MOUNT);

        const char *args[] = {"cp", bootlogo_image, bootlogo_dest, NULL};
        run_exec(args, A_SIZE(args), 0);

        if (strcasecmp(device.DEVICE.NAME, "rg28xx-h") == 0) {
            const char *args[] = {"convert", bootlogo_dest, "-rotate", "270", bootlogo_dest, NULL};
            run_exec(args, A_SIZE(args), 0);
        }
    }
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
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return NULL;
    }

    char **list = malloc(MAX_BUFFER_SIZE * sizeof(char *));
    if (!list) {
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
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
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
        for (int i = 0; i < *count; i++) free(list[i]);
        free(list);

        return NULL;
    }

    return list;
}
