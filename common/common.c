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
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "miniz/miniz.h"
#include "font/notosans.h"
#include "font/notosans_jp.h"
#include "font/notosans_kr.h"
#include "font/notosans_sc.h"
#include "font/notosans_tc.h"
#include "img/nothing.h"
#include "json/json.h"
#include "common.h"
#include "log.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "mini/mini.h"

__thread uint64_t start_ms = 0;
struct json translation_generic;
struct json translation_specific;
struct pattern skip_pattern_list = {NULL, 0, 0};
int battery_capacity = 100;
lv_anim_t animation;
lv_obj_t *img_obj;
const char **img_paths = NULL;
int img_paths_count = 0;
char current_wall[MAX_BUFFER_SIZE];
lv_obj_t *wall_img = NULL;

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) {
        start_ms = now_ms;
    }

    return (uint32_t) (now_ms - start_ms);
}

void refresh_screen() {
    lv_task_handler();
    usleep(device.SCREEN.WAIT);
}

int file_exist(char *filename) {
    return access(filename, F_OK) == 0;
}

int directory_exist(char *dirname) {
    struct stat stats;
    return stat(dirname, &stats) == 0 && S_ISDIR(stats.st_mode);
}

unsigned long long total_file_size(const char *path) {
    long long total_size = 0;
    struct dirent *entry;

    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
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

        if (c1 != c2) {
            return c1 - c2;
        }

        str1++;
        str2++;
    }

    return *str1 - *str2;
}

int str_startswith(const char *a, const char *b) {
    if (strncmp(a, b, strlen(b)) == 0) {
        return 1;
    }

    return 0;
}

char *str_nonew(char *text) {
    char *newline_pos = strchr(text, '\n');

    if (newline_pos != NULL) {
        *newline_pos = '\0';
    }

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


char *str_trim(char *text) {
    if (!text || !*text) {
        return text;
    }

    while (isspace((unsigned char) (*text))) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char) (*end))) {
        end--;
    }

    *(end + 1) = '\0';
    return text;
}

char *str_replace(char *orig, char *rep, char *with) {
    char *result;
    char *ins;
    char *tmp;
    size_t len_rep;
    size_t len_with;
    size_t len_front;
    int count;

    if (!orig || !rep)
        return NULL;

    len_rep = strlen(rep);

    if (len_rep == 0)
        return NULL;

    if (!with)
        with = "";

    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = (char *) malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

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

char *get_last_subdir(char *text, char separator, int n) {
    char *ptr = text;
    int count = 0;

    while (*ptr && count < n) {
        if (*ptr == separator) {
            count++;
        }
        ptr++;
    }

    return (*ptr) ? ptr : text;
}

char *get_last_dir(char *text) {
    char *last_slash = strrchr(text, '/');

    if (last_slash != NULL) {
        return last_slash + 1;
    }

    return "";
}

char *strip_dir(char *text) {
    char *result = strdup(text);
    char *last_slash = strrchr(result, '/');

    if (last_slash != NULL) {
        *last_slash = '\0';
    }

    return result;
}

char *strip_ext(char *text) {
    char *result = strdup(text);
    char *ext = strrchr(result, '.');

    if (ext != NULL) {
        *ext = '\0';
    }

    return result;
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
    if (newline != NULL)
        *newline = '\0';

    return result;
}

int read_battery_capacity() {
    FILE *file = fopen(device.BATTERY.CAPACITY, "r");

    if (file == NULL) {
        perror("Error opening capacity file");
        return 0;
    }

    int capacity;
    if (fscanf(file, "%d", &capacity) != 1) {
        perror("Error reading integer");
        return 0;
    }

    fclose(file);

    capacity = capacity + ((config.SETTINGS.ADVANCED.OFFSET) - 50);

    if (capacity > 100) {
        return 100;
    } else {
        return capacity;
    }
}

char *read_battery_health() {
    FILE *file = fopen(device.BATTERY.HEALTH, "r");

    if (file == NULL) {
        perror("Error opening health file");
        return strdup("Unknown");
    }

    char *health = NULL;
    if (fscanf(file, "%m[^\n]", &health) != 1) {
        perror("Error reading health file");
        fclose(file);
        return strdup("Unknown");
    }

    fclose(file);
    return health;
}

char *read_battery_voltage() {
    FILE *file = fopen(device.BATTERY.VOLTAGE, "r");

    if (file == NULL) {
        perror("Error opening voltage file");
        return "0.00 V";
    }

    int raw_voltage;
    if (fscanf(file, "%d", &raw_voltage) != 1) {
        perror("Error reading integer");
        fclose(file);
        return "0.00 V";
    }

    fclose(file);

    char *form_voltage = (char *) malloc(10);
    if (form_voltage == NULL) {
        perror("Error allocating memory");
        return "0.00 V";
    }

    snprintf(form_voltage, 8, "%.2f V", raw_voltage / 1000000.0);
    return form_voltage;
}

char *read_text_from_file(const char *filename) {
    char *text = NULL;
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        return "";
    }

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
        perror("Error allocating memory for text");
    }

    fclose(file);
    return text;
}

char *read_line_from_file(const char *filename, size_t line_number) {
    char *line = NULL;
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        return "";
    }

    line = (char *) malloc(MAX_BUFFER_SIZE);

    if (line != NULL) {
        size_t current_line = 0;
        while (current_line < line_number && fgets(line, MAX_BUFFER_SIZE, file) != NULL) {
            current_line++;
        }

        if (current_line < line_number) {
            free(line);
            return "";
        } else {
            size_t length = strlen(line);
            if (length > 0 && line[length - 1] == '\n') {
                line[length - 1] = '\0';
            }
        }
    } else {
        perror("allocating memory for line");
    }

    fclose(file);
    return line;
}

int read_int_from_file(const char *filename, size_t line_number) {
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

const char *get_random_hex() {
    int red = rand() % UINT8_MAX;
    int green = rand() % UINT8_MAX;
    int blue = rand() % UINT8_MAX;

    char *colour_hex = (char *) malloc(7 * sizeof(char));
    sprintf(colour_hex, "%02X%02X%02X", red, green, blue);

    return colour_hex;
}

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key) {
    const char *meta = mini_get_string(ini_config, section, key, get_random_hex());

    uint32_t result;

    result = (uint32_t)
            strtoul(meta, NULL, 16);

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

char *format_meta_text(char *filename) {
    char meta_cut[MAX_BUFFER_SIZE];
    snprintf(meta_cut, sizeof(meta_cut), "/opt/muos/script/mux/metacut.sh \"%s\"", filename);

    FILE *fp = popen(meta_cut, "r");
    if (fp == NULL) {
        perror("popen");
        return "Could not open metadata!";
    }

    char buffer[MAX_BUFFER_SIZE * 4];
    size_t meta_size = 0;
    char *result = NULL;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t len = strlen(buffer);
        result = realloc(result, meta_size + len + 1);
        if (result == NULL) {
            perror("realloc");
            return "Could not read metadata!";
        }
        strcpy(result + meta_size, buffer);
        meta_size += len;
    }

    pclose(fp);
    return result;
}

void write_text_to_file(const char *filename, const char *mode, int type, ...) {
    FILE *file = fopen(filename, mode);

    if (file == NULL) {
        perror("Error opening file for writing");
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

    if (stat(path, &st) == 0) {
        return;
    }

    char *path_copy = strdup(path);
    char *slash = strrchr(path_copy, '/');
    if (slash != NULL) {
        *slash = '\0';
        create_directories(path_copy);
        *slash = '/';
        // recursive bullshit
    }

    if (mkdir(path_copy, 0777) == -1) {
        free(path_copy);
    }
}

int count_items(const char *path, enum count_type type) {
    struct dirent *entry;
    struct stat file_info;
    int count = 0;

    DIR *dir;
    dir = opendir(path);

    if (dir == NULL) {
        perror("opendir");
        return 0;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            if (stat(full_path, &file_info) == -1) {
                perror("stat");
                continue;
            }

            if (type == FILES_ONLY && S_ISREG(file_info.st_mode)) {
                count++;
            } else if (type == DIRECTORIES_ONLY && S_ISDIR(file_info.st_mode)) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    count++;
                }
            } else if (type == BOTH) {
                if (S_ISREG(file_info.st_mode) ||
                    (S_ISDIR(file_info.st_mode) &&
                     strcmp(entry->d_name, ".") != 0 &&
                     strcmp(entry->d_name, "..") != 0)) {
                    count++;
                }
            }
        }
    }

    closedir(dir);
    return count;
}

int detect_storage(const char *target) {
    FILE *fp;
    char line[MAX_BUFFER_SIZE];
    int found = 0;

    fp = fopen("/proc/partitions", "r");
    if (!fp) {
        perror("Error opening /proc/partitions");
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, target)) {
            found = 1;
            break;
        }
    }

    fclose(fp);
    return found;
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
    }
}

void nav_prev(lv_group_t *group, int count) {
    int i;
    for (i = 0; i < count; i++) {
        lv_group_focus_prev(group);
    }
}

void nav_next(lv_group_t *group, int count) {
    int i;
    for (i = 0; i < count; i++) {
        lv_group_focus_next(group);
    }
}

char *get_datetime() {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);

    static char datetime_str[MAX_BUFFER_SIZE];
    char *notation;

    if (config.CLOCK.NOTATION) {
        notation = TIME_STRING_24;
    } else {
        notation = TIME_STRING_12;
    }

    strftime(datetime_str, sizeof(datetime_str), notation, time_info);

    return datetime_str;
}

void datetime_task(lv_timer_t *timer) {
    struct dt_task_param *dt_par = timer->user_data;
    lv_label_set_text(dt_par->lblDatetime, get_datetime());
}

char *get_capacity() {
    char *battery_glyph_name = read_int_from_file(device.BATTERY.CHARGER, 1) ? "capacity_charging_" : "capacity_";

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

void osd_task(lv_timer_t *timer) {
    struct osd_task_param *osd_par = timer->user_data;
    if (osd_message != NULL) {
        lv_obj_clear_flag(osd_par->pnlMessage, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(osd_par->lblMessage, osd_message);
        osd_par->count++;
        if (osd_par->count == 3) {
            osd_message = NULL;
            osd_par->count = 0;
        }
    } else {
        lv_label_set_text(osd_par->lblMessage, " ");
        lv_obj_add_flag(osd_par->pnlMessage, LV_OBJ_FLAG_HIDDEN);
    }
}

void increase_option_value(lv_obj_t *element) {
    uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;
    uint16_t current = lv_dropdown_get_selected(element);

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
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d", rom, dir, sys, forced);
    fclose(file);
}

void load_gov(const char *rom, const char *dir, const char *sys, int forced) {
    FILE *file = fopen(MUOS_GOV_LOAD, "w");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n%s\n%s\n%d", rom, dir, sys, forced);
    fclose(file);
}

void load_mux(const char *value) {
    FILE *file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void play_sound(const char *sound, int enabled, int wait) {
    if (enabled) {
        char ns_file[MAX_BUFFER_SIZE];
        snprintf(ns_file, sizeof(ns_file), "%s/sound/%s.wav", STORAGE_THEME, sound);

        if (file_exist(ns_file)) {
            printf("PLAYING SOUND: %s\n", ns_file);
            Mix_Chunk *sound_chunk = Mix_LoadWAV(ns_file);

            if (sound_chunk) {
                int sound_play = Mix_PlayChannel(-1, sound_chunk, 0);

                if (wait) {
                    while (Mix_Playing(sound_play)) {
                        SDL_Delay(MAX_BUFFER_SIZE); // Could be shorter but some sounds get cut off so I'm cheating!~~
                    }
                }
            }
        }
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
                            perror("Error deleting file");
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && strcasecmp(entry->d_name, ".") != 0 &&
                       strcasecmp(entry->d_name, "..") != 0) {
                if (recursive) {
                    char sub_dir_path[PATH_MAX];
                    snprintf(sub_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
                    delete_files_of_type(sub_dir_path, extension, exception, recursive);
                }
            }
        }

        closedir(dir);
    } else {
        perror("Error opening directory");
    }
}

void delete_files_of_name(const char *dir_path, const char *filename) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strcasecmp(entry->d_name, ".") == 0 || strcasecmp(entry->d_name, "..") == 0) {
                continue;
            }

            char full_path[PATH_MAX];
            snprintf(full_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);

            if (entry->d_type == DT_DIR) {
                delete_files_of_name(full_path, filename);
            } else if (entry->d_type == DT_REG && strcmp(entry->d_name, filename) == 0) {
                if (remove(full_path) != 0) {
                    perror("Error deleting file");
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error opening directory");
    }
}

int load_element_image_specifics(const char *theme_base, const char *device_dimension, const char *program,
                                 const char *image_type, const char *element, const char *image_extension,
                                 char *image_path, size_t path_size) {
    return
            (snprintf(image_path, path_size, "%s/%simage/%s/%s/%s/%s.%s", theme_base, device_dimension,
                      config.SETTINGS.GENERAL.LANGUAGE, image_type, program, element, image_extension) >= 0 &&
             file_exist(image_path)) ||
            (snprintf(image_path, path_size, "%s/%simage/%s/%s/%s.%s", theme_base, device_dimension,
                      image_type, program, element, image_extension) >= 0 && file_exist(image_path));
}

int load_image_specifics(const char *theme_base, const char *device_dimension, const char *program,
                         const char *image_type, const char *image_extension, char *image_path, size_t path_size) {
    return
            (snprintf(image_path, path_size, "%s/%simage/%s.%s", theme_base, device_dimension,
                      image_type, image_extension) >= 0 &&
             file_exist(image_path)) ||
            (snprintf(image_path, path_size, "%s/%simage/%s/%s/%s.%s", theme_base, device_dimension,
                      config.SETTINGS.GENERAL.LANGUAGE, image_type, program, image_extension) >= 0 &&
             file_exist(image_path)) ||
            (snprintf(image_path, path_size, "%s/%simage/%s/%s.%s", theme_base, device_dimension, image_type,
                      program, image_extension) >= 0 &&
             file_exist(image_path)) ||
            (snprintf(image_path, path_size, "%s/%simage/%s/%s/default.%s", theme_base, device_dimension,
                      config.SETTINGS.GENERAL.LANGUAGE, image_type, image_extension) >= 0 &&
             file_exist(image_path)) ||
            (snprintf(image_path, path_size, "%s/%simage/%s/default.%s", theme_base, device_dimension,
                      image_type, image_extension) >= 0 &&
             file_exist(image_path));
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated, int random) {
    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));
    char *device_dimensions[15] = {device_dimension, ""};

    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    const char *wall_extension = random ? "0.png" : (animated == 1 ? "gif" : (animated == 2 ? "0.png" : "png"));

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
        const char *element = lv_obj_get_user_data(element_focused);
        for (int i = 0; i < 2; i++) {
            if (load_element_image_specifics(STORAGE_THEME, device_dimensions[i], program, "wall",
                                             element, wall_extension, wall_image_path, sizeof(wall_image_path))) {

                snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
                return wall_image_embed;
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        if (load_image_specifics(STORAGE_THEME, device_dimensions[i], program, "wall",
                                 wall_extension, wall_image_path, sizeof(wall_image_path))) {

            snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", wall_image_path);
            return wall_image_embed;
        }
    }

    return "";
}

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall, lv_obj_t *ui_imgWall,
                    int animated, int animation_delay, int random) {

    static char new_wall[MAX_BUFFER_SIZE];
    snprintf(new_wall, sizeof(new_wall), "%s", get_wallpaper_path(
            ui_screen, ui_group, animated, random));

    if (strcasecmp(new_wall, current_wall) != 0) {
        snprintf(current_wall, sizeof(current_wall), "%s", new_wall);
        if (strlen(new_wall) > 3) {
            if (random) {
                load_image_random(ui_imgWall, new_wall);
            } else {
                switch (animated) {
                    case 1:
                        wall_img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(wall_img, new_wall);
                        break;
                    case 2:
                        load_image_animation(ui_imgWall, animation_delay, new_wall);
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

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group) {
    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));
    char *device_dimensions[15] = {device_dimension, ""};

    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
        const char *element = lv_obj_get_user_data(element_focused);

        for (int i = 0; i < 2; i++) {
            if (load_element_image_specifics(STORAGE_THEME, device_dimensions[i], program, "static",
                                             element, "png", static_image_path, sizeof(static_image_path))) {

                snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                return static_image_embed;
            }
        }
    }

    return "";
}

void load_overlay_image(lv_obj_t *ui_screen, int16_t image_overlay_enabled) {
    const char *program = lv_obj_get_user_data(ui_screen);

    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (load_image_specifics(STORAGE_THEME, device_dimension, program, "overlay", "png",
                             static_image_path, sizeof(static_image_path)) || 
        load_image_specifics(STORAGE_THEME, device_dimension, program, "overlay", "png",
                             static_image_path, sizeof(static_image_path))) {

        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);

        if (image_overlay_enabled) {
            lv_obj_t *overlay_img = lv_img_create(ui_screen);
            lv_img_set_src(overlay_img, static_image_embed);
            lv_obj_move_foreground(overlay_img);
        }
    }
}

static void image_anim_cb(void *var, int32_t img_idx) {
    lv_img_set_src(img_obj, img_paths[img_idx]);
}

void build_image_array(char *base_image_path) {
    char base_path[MAX_BUFFER_SIZE];
    char path[MAX_BUFFER_SIZE];
    char path_embed[MAX_BUFFER_SIZE];
    int index = 0;

    strncpy(base_path, base_image_path, strlen(base_image_path) - 6);
    base_path[strlen(base_image_path) - 6] = '\0';

    int file_exists = 1;
    while (file_exists) {
        snprintf(path, sizeof(path), "%s.%d.png", base_path + 2, index);
        file_exists = file_exist(path);
        if (file_exists) {
            img_paths = realloc(img_paths, (img_paths_count + 1) * sizeof(char *));
            snprintf(path_embed, sizeof(path_embed), "%s.%d.png", base_path, index);
            img_paths[index] = strdup(path_embed);
            img_paths_count++;
        } else {
            break;
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
        lv_img_set_src(ui_imgWall, img_paths[arc4random() % (img_paths_count - 1)]);
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }
}

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, char *base_image_path) {
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
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);

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
    } else if (strcasecmp(config.SETTINGS.GENERAL.LANGUAGE, "Korean") == 0) {
        return &ui_font_NotoSans_KR;
    } else {
        return &ui_font_NotoSans;
    }
}

void load_font_text_from_file(const char *filepath, lv_obj_t *element) {
    char theme_font_text_fs[MAX_BUFFER_SIZE];
    snprintf(theme_font_text_fs, sizeof(theme_font_text_fs), "M:%s", filepath);
    lv_font_t * font = lv_font_load(theme_font_text_fs);
    font->fallback = get_language_font();
    lv_obj_set_style_text_font(element, font, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void get_device_dimension(char *device_dimension, size_t size) {
    snprintf(device_dimension, size, "%dx%d/", device.MUX.WIDTH, device.MUX.HEIGHT);
}

void load_font_text(const char *program, lv_obj_t *screen) {
    const lv_font_t *language_font = get_language_font();
    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_text_default[MAX_BUFFER_SIZE];
        char theme_font_text[MAX_BUFFER_SIZE];

        char device_dimension[15];
        get_device_dimension(device_dimension, sizeof(device_dimension));
        char *device_dimensions[15] = {device_dimension, ""};
        for (int i = 0; i < 2; i++) {
            if ((snprintf(theme_font_text, sizeof(theme_font_text),
                          "%s/%sfont/%s/%s.bin", STORAGE_THEME, device_dimensions[i],
                          config.SETTINGS.GENERAL.LANGUAGE, program) >= 0 &&
                 file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text_default),
                          "%s/%sfont/%s/default.bin", STORAGE_THEME, device_dimensions[i],
                          config.SETTINGS.GENERAL.LANGUAGE) >=
                 0 && file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text),
                          "%s/%sfont/%s.bin", STORAGE_THEME, device_dimensions[i], program) >= 0 &&
                 file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text_default),
                          "%s/%sfont/default.bin", STORAGE_THEME, device_dimensions[i]) >= 0 &&
                 file_exist(theme_font_text))) {

                printf("Loading Main Theme Font: %s\n", theme_font_text);
                load_font_text_from_file(theme_font_text, screen);
                return;
            }
        }
    }
    printf("Loading Default Language Font\n");
    lv_obj_set_style_text_font(screen, language_font, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void load_font_section(const char *program, const char *section, lv_obj_t *element) {
    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_section[MAX_BUFFER_SIZE];

        char device_dimension[15];
        get_device_dimension(device_dimension, sizeof(device_dimension));
        char *device_dimensions[15] = {device_dimension, ""};
        for (int i = 0; i < 2; i++) {
            if ((snprintf(theme_font_section, sizeof(theme_font_section),
                          "%s/%sfont/%s/%s/%s.bin", STORAGE_THEME, device_dimensions[i],
                          config.SETTINGS.GENERAL.LANGUAGE, section,
                          program) >= 0 && file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                          "%s/%sfont/%s/%s/default.bin", STORAGE_THEME, device_dimensions[i],
                          config.SETTINGS.GENERAL.LANGUAGE,
                          section) >= 0 && file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                          "%s/%sfont/%s/%s.bin", STORAGE_THEME, device_dimensions[i], section, program) >= 0 &&
                 file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                          "%s/%sfont/%s/default.bin", STORAGE_THEME, device_dimensions[i], section) >= 0 &&
                 file_exist(theme_font_section))) {

                printf("Loading Section '%s' Font: %s\n", section, theme_font_section);
                load_font_text_from_file(theme_font_section, element);
                return;
            }
        }
    }
}

int is_network_connected() {
    if (!config.NETWORK.ENABLED) return 0;

    if (file_exist(device.NETWORK.STATE)) {
        if (strcasecmp("up", read_text_from_file(device.NETWORK.STATE)) == 0) return 1;
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
    snprintf(skip_ini, sizeof(skip_ini), "%s/info/config/skip.ini", STORAGE_PATH);

    if (!file_exist(skip_ini)) {
        snprintf(skip_ini, sizeof(skip_ini), "%s/MUOS/info/skip.ini", device.STORAGE.ROM.MOUNT);
    }

    FILE *file = fopen(skip_ini, "r");
    if (!file) {
        perror("Failed to open 'skip.ini' file");
        return;
    }

    for (size_t i = 0; i < skip_pattern_list.count; i++) {
        free(skip_pattern_list.patterns[i]);
    }

    free(skip_pattern_list.patterns);

    skip_pattern_list.count = 0;
    skip_pattern_list.capacity = 2;
    skip_pattern_list.patterns = malloc(skip_pattern_list.capacity * sizeof(char *));

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
        }

        skip_pattern_list.patterns[skip_pattern_list.count] = strdup(line);
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
    int spec = 48;

    char *test_message = "This is a test image! This is a test image! This is a test image! This is a test image!\n"
                         "is a test image! This is a test image! This is a test image! This is a test image! This\n"
                         "a test image! This is a test image! This is a test image! This is a test image! This is\n"
                         "test image! This is a test image! This is a test image! This is a test image! This is a\n"
                         "image! This is a test image! This is a test image! This is a test image! This is a test\n";

    lv_obj_t *ui_conTest = lv_obj_create(screen);
    lv_obj_remove_style_all(ui_conTest);
    lv_obj_set_width(ui_conTest, device.SCREEN.WIDTH);
    lv_obj_set_height(ui_conTest, device.SCREEN.HEIGHT);

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

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count,
                            int current_item_index, lv_obj_t *ui_pnlContent) {
    // how many items should be above the currently selected item when scrolling
    double item_distribution = (mux_item_count - 1) / (double) 2;
    // how many items are off screen
    double scroll_multiplier = (current_item_index > item_distribution) ? (current_item_index - item_distribution) : 0;
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

void load_language(const char *program) {
    char language_file[MAX_BUFFER_SIZE];
    snprintf(language_file, sizeof(language_file), "%s/language/%s.json",
             STORAGE_PATH, config.SETTINGS.GENERAL.LANGUAGE);

    if (json_valid(read_text_from_file(language_file))) {
        translation_specific = json_object_get(json_parse(read_text_from_file(language_file)), program);
        translation_generic = json_object_get(json_parse(read_text_from_file(language_file)), "generic");
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

char *get_script_value(const char *filename, const char *key) {
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

    if (value == NULL) value = strdup("");
    return value;
}

void update_bars(lv_obj_t *bright_bar, lv_obj_t *volume_bar, lv_obj_t *volume_icon) {
    if (!progress_onscreen) {
        return;
    }

    lv_bar_set_value(bright_bar, read_int_from_file(BRIGHT_PERC, 1), LV_ANIM_ON);

    int volume = read_int_from_file(VOLUME_PERC, 1);
    lv_bar_set_value(volume_bar, volume, LV_ANIM_ON);
    switch (volume) {
        default:
        case 0:
            lv_label_set_text(volume_icon, "\uF6A9");
            break;
        case 1 ... 46:
            lv_label_set_text(volume_icon, "\uF026");
            break;
        case 47 ... 71:
            lv_label_set_text(volume_icon, "\uF027");
            break;
        case 72 ... 100:
            lv_label_set_text(volume_icon, "\uF028");
            break;
    }
}

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        printf("Error: Could not open archive '%s' - Corrupt?\n", zip_path);
        return 1;
    }

    int file_index = mz_zip_reader_locate_file(&zip, file_name, NULL, 0);
    if (file_index == -1) {
        printf("Error: '%s' not found in archive\n", file_name);
        mz_zip_reader_end(&zip);
        return 1;
    }

    if (!mz_zip_reader_extract_to_file(&zip, file_index, output_path, 0)) {
        printf("Error: Could not extract '%s'\n", file_name);
        mz_zip_reader_end(&zip);
        return 1;
    }

    mz_zip_reader_end(&zip);
    return 0;
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
        perror("opendir");
        return;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            if (entry->d_type == DT_DIR) {
                if (strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
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

int init_nav_sound(const char *mux_module) {
    if (config.SETTINGS.GENERAL.SOUND) {
        if (SDL_Init(SDL_INIT_AUDIO) >= 0) {
            Mix_Init(0);
            Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
            LOG_INFO(mux_module, "SDL Init Success")
            return 1;
        } else {
            LOG_ERROR(mux_module, "SDL Failed To Init")
        }
    }

    return 0;
}
