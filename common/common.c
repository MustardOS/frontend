#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "common.h"
#include "options.h"
#include "theme.h"
#include "mini/mini.h"

int file_exist(char *filename) {
    return access(filename, F_OK) == 0;
}

int file_size(char *filename, int filesize) {
    FILE * file = fopen(filename, "rb");

    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (file_size > filesize * 1024 * 1024) {
        perror("File is larger than specified");
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
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

int time_compare_for_history(const void *a, const void *b) {
    const char *file_a = strip_label_placement(strip_ext(*(char **) a));
    const char *file_b = strip_label_placement(strip_ext(*(char **) b));

    if (strcasecmp(file_a, DUMMY_ROM) == 0 || strcasecmp(file_b, DUMMY_ROM) == 0) {
        return 0;
    }

    char mod_file_a[MAX_BUFFER_SIZE];
    char mod_file_b[MAX_BUFFER_SIZE];

    snprintf(mod_file_a, sizeof(mod_file_a), "/%s/history/%s.cfg", MUOS_INFO_PATH, file_a);
    snprintf(mod_file_b, sizeof(mod_file_b), "/%s/history/%s.cfg", MUOS_INFO_PATH, file_b);

    if (access(mod_file_a, F_OK) != 0) {
        perror("Error: mod_file_a does not exist");
        return 0;
    }

    if (access(mod_file_b, F_OK) != 0) {
        perror("Error: mod_file_b does not exist");
        return 0;
    }

    struct stat stat_a, stat_b;

    if (stat(mod_file_a, &stat_a) != 0) {
        perror("Error getting file information for mod_file_a");
        return 0;
    }

    if (stat(mod_file_b, &stat_b) != 0) {
        perror("Error getting file information for mod_file_b");
        return 0;
    }

    struct timespec time_a = stat_a.st_mtim;
    struct timespec time_b = stat_b.st_mtim;

    if (time_a.tv_sec > time_b.tv_sec) {
        return -1;
    } else if (time_a.tv_sec < time_b.tv_sec) {
        return 1;
    } else {
        if (time_a.tv_nsec > time_b.tv_nsec) {
            return -1;
        } else if (time_a.tv_nsec < time_b.tv_nsec) {
            return 1;
        } else {
            return 0;
        }
    }
}

char *str_append(char *old_text, const char *new_text) {
    size_t len = old_text ? strlen(old_text) : 0;
    char *temp = realloc(old_text, len + strlen(new_text) + 2);

    if (!temp) return old_text;
    old_text = temp;
    strcpy(old_text + len, new_text);

    *(old_text + len + strlen(new_text)) = '\n';
    *(old_text + len + strlen(new_text) + 1) = '\0';

    return old_text;
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

int get_label_placement(char *text) {
    char *place_text = strstr(text, " :: ");

    if (place_text != NULL) {
        return atoi(place_text + strlen(" :: "));
    }

    return 0;
}

char *strip_label_placement(char *text) {
    char *place_text = strstr(text, " :: ");

    if (place_text != NULL) {
        text[place_text - text] = '\0';
    }

    return text;
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
    char *ext = get_ext(result);

    if (*ext != '\0') {
        result[strlen(result) - strlen(ext)] = '\0';
    }

    return result;
}

char *get_ext(char *text) {
    size_t i;
    int last_dot_index = -1;

    for (i = strlen(text); i > 0; i--) {
        if (text[i] == '.') {
            last_dot_index = i;
            break;
        } else if (text[i] == '/' || text[i] == '\\') {
            return "";
        }
    }

    if (last_dot_index != -1) {
        return &text[last_dot_index];
    }

    return "";
}

char *get_execute_result(const char *command) {
    FILE * fp = popen(command, "r");
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

char *current_datetime() {
    time_t current_time;
    struct tm *timeinfo;

    time(&current_time);
    timeinfo = localtime(&current_time);

    char *formatted_time = (char *) malloc(22 * sizeof(char));
    strftime(formatted_time, 22, "%Y-%m-%d %I:%M %p", timeinfo);

    return formatted_time;
}

int read_battery_capacity() {
    FILE * file = fopen(BATT_CAPACITY, "r");

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

    capacity = capacity + (get_ini_int(muos_config, "settings.advanced", "offset", LABEL) - 50);

    if (capacity > 100) {
        return 100;
    } else {
        return capacity;
    }
}

char *read_battery_health() {
    FILE * file = fopen(BATT_HEALTH, "r");

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
    FILE * file = fopen(BATT_VOLTAGE, "r");

    if (file == NULL) {
        perror("Error opening voltage file");
        return "0.00v";
    }

    int raw_voltage;
    if (fscanf(file, "%d", &raw_voltage) != 1) {
        perror("Error reading integer");
        fclose(file);
        return "0.00v";
    }

    fclose(file);

    char *form_voltage = (char *) malloc(10);
    if (form_voltage == NULL) {
        perror("Error allocating memory");
        return "0.00v";
    }

    snprintf(form_voltage, 8, "%.2fv", raw_voltage / 1000000.0);
    return form_voltage;
}

char *read_text_from_file(char *filename) {
    char *text = NULL;
    FILE * file = fopen(filename, "r");

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

char *read_line_from_file(char *filename, int line_number) {
    char *line = NULL;
    FILE * file = fopen(filename, "r");

    if (file == NULL) {
        return "";
    }

    size_t buffer_size = MAX_BUFFER_SIZE;
    line = (char *) malloc(buffer_size);

    if (line != NULL) {
        int current_line = 0;
        while (current_line < line_number && fgets(line, buffer_size, file) != NULL) {
            current_line++;
        }

        if (current_line < line_number) {
            free(line);
            perror("line number exceeded");
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

const char *get_random_hex() {
    int red = rand() % UINT8_MAX;
    int green = rand() % UINT8_MAX;
    int blue = rand() % UINT8_MAX;

    char *colour_hex = (char *) malloc(7 * sizeof(char));
    sprintf(colour_hex, "%02X%02X%02X", red, green, blue);

    return colour_hex;
}

const char *get_random_int() {
    uint8_t random_num = rand() % (UINT8_MAX + 1);

    char *result = (char *) malloc(2 * sizeof(char));
    sprintf(result, "%d", random_num);

    return result;
}

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key) {
    const char *meta = mini_get_string(ini_config, section, key, get_random_hex());

    uint32_t
    result = (uint32_t)
    strtoul(meta, NULL, 16);
    //printf("HEX\t%s: %s (%d)\n", key, meta, result);

    return result;
}

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, enum element_type type) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    int16_t result;
    if (strcmp(meta, "NOT FOUND") == 0) {
        switch (type) {
            case LABEL:
                result = (int16_t)
                strtol("0", NULL, 10);
                break;
            case VALUE:
                result = (int16_t)
                strtol("255", NULL, 10);
                break;
            case IGNORE:
            default:
                result = (int16_t)
                strtol(get_random_int(), NULL, 10);
                break;
        }
    } else {
        result = (int16_t)
        strtol(meta, NULL, 10);
    }

    //printf("INT\t%s: %s (%d)\n", key, meta, result);

    return result;
}

int set_ini_int(mini_t *ini_config, const char *section, const char *key, int value) {
    int meta = mini_set_int(ini_config, section, key, value);
    return meta;
}

const char *get_ini_string(mini_t *ini_config, const char *section, const char *key) {
    const char *meta = mini_get_string(ini_config, section, key, "??");

    //printf("STR\t%s: %s\n", key, meta);

    return meta;
}

int set_ini_string(mini_t *ini_config, const char *section, const char *key, const char *value) {
    int meta = mini_set_string(ini_config, section, key, value);
    return meta;
}

char *format_meta_text(char *filename) {
    char meta_cut[MAX_BUFFER_SIZE];
    snprintf(meta_cut, sizeof(meta_cut), "/opt/muos/script/mux/metacut.sh \"%s\"", filename);

    FILE * fp = popen(meta_cut, "r");
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

void write_text_to_file(const char *filename, const char *text, const char *mode) {
    FILE * file = fopen(filename, mode);

    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    fprintf(file, "%s", text);
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

const char *read_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
            count++;
        }
    }

    const char **options = (const char **) malloc((count + 1) * sizeof(const char *));

    rewinddir(dir);

    count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
            options[count++] = strdup(entry->d_name);
        }
    }

    options[count] = NULL;
    closedir(dir);

    return *options;
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

int detect_sd2() {
    FILE * fp;
    char line[MAX_BUFFER_SIZE];
    const char *target = "mmcblk1";
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

int detect_e_usb() {
    FILE * fp;
    char line[MAX_BUFFER_SIZE];
    const char *target = "sda1";
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

    if (get_ini_int(muos_config, "clock", "notation", LABEL)) {
        strftime(datetime_str, sizeof(datetime_str), TIME_STRING_24, time_info);
    } else {
        strftime(datetime_str, sizeof(datetime_str), TIME_STRING_12, time_info);
    }

    return datetime_str;
}

void datetime_task(lv_timer_t *timer) {
    struct dt_task_param *dt_par = timer->user_data;
    lv_label_set_text(dt_par->lblDatetime, get_datetime());
}

char *get_capacity() {
    static char capacity_str[MAX_BUFFER_SIZE];

    switch (read_battery_capacity()) {
        case -255 ... 15:
            snprintf(capacity_str, sizeof(capacity_str), "\uF244");
            break;
        case 16 ... 30:
            snprintf(capacity_str, sizeof(capacity_str), "\uF243");
            break;
        case 31 ... 50:
            snprintf(capacity_str, sizeof(capacity_str), "\uF242");
            break;
        case 51 ... 75:
            snprintf(capacity_str, sizeof(capacity_str), "\uF241");
            break;
        case 76 ... 255:
            snprintf(capacity_str, sizeof(capacity_str), "\uF240");
            break;
    }

    return capacity_str;
}

void capacity_task(lv_timer_t *timer) {
    struct bat_task_param *bat_par = timer->user_data;
    lv_label_set_text(bat_par->staCapacity, get_capacity());
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

void set_governor(char *governor) {
    FILE * file = fopen(GOVERNOR_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%s", governor);
        fclose(file);
    } else {
        perror("Failed to open scaling_governor file");
    }
}

void set_cpu_scale(int speed) {
    FILE * file = fopen(SCALE_MN_FILE, "w");
    if (file != NULL) {
        fprintf(file, "%d", speed);
        fclose(file);
    } else {
        perror("Failed to open scaling_max_freq file");
    }
}

void increase_option_value(lv_obj_t *element, int *current, int total) {
    if (*current < (total - 1)) {
        (*current)++;
        lv_dropdown_set_selected(element, *current);
    } else {
        (*current) = 0;
        lv_dropdown_set_selected(element, *current);
    }
}

void decrease_option_value(lv_obj_t *element, int *current, int total) {
    if (*current > 0) {
        (*current)--;
        lv_dropdown_set_selected(element, *current);
    } else {
        (*current) = (total - 1);
        lv_dropdown_set_selected(element, *current);
    }
}

void run_shell_script(const char *shell_script) {
    if (shell_script) system(shell_script);
}

void load_system(const char *value) {
    FILE * file = fopen(MUOS_SYS_LOAD, "w");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void load_assign(const char *dir, const char *sys) {
    FILE * file = fopen(MUOS_ASS_LOAD, "w");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n%s", dir, sys);
    fclose(file);
}

void load_mux(const char *value) {
    FILE * file = fopen(MUOS_ACT_LOAD, "w");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    fprintf(file, "%s\n", value);
    fclose(file);
}

void play_sound(const char *sound, int enabled) {
    if (enabled) {
        char send_sound[MAX_BUFFER_SIZE];
        snprintf(send_sound, sizeof(send_sound), "echo %s > /tmp/muplay_pipe", sound);
        system(send_sound);
    }
}

void delete_files_of_type(const char *dir_path, const char *extension) {
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

                    if (remove(file_path) != 0) {
                        perror("Error deleting file");
                    }
                }
            } else if (entry->d_type == DT_DIR && strcasecmp(entry->d_name, ".") != 0 &&
                       strcasecmp(entry->d_name, "..") != 0) {
                char sub_dir_path[PATH_MAX];
                snprintf(sub_dir_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
                delete_files_of_type(sub_dir_path, extension);
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

void hex_to_rgb(char hex[7], int *r, int *g, int *b) {
    unsigned int hex_value = (unsigned int) strtol(hex, NULL, 16);
    *r = (hex_value >> 16) & 0xFF;
    *g = (hex_value >> 8) & 0xFF;
    *b = hex_value & 0xFF;
}

char *extract_in_brackets(const char *input) {
    const char *start = strchr(input, '(');
    const char *end = strchr(input, ')');

    if (start != NULL && end != NULL && start < end) {
        size_t length = end - start - 1;

        char *result = malloc(length + 1);

        strncpy(result, start + 1, length);
        result[length] = '\0';

        return result;
    }

    return NULL;
}

char *load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    char *wall_extension;
    if (animated) {
        wall_extension = "gif";
    } else {
        wall_extension = "png";
    }

    if (lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
        const char *element = lv_obj_get_user_data(element_focused);

        if (snprintf(wall_image_path, sizeof(wall_image_path), "/%s/wall/%s/%s.%s",
                     MUOS_IMAGE_PATH, program, element, wall_extension) >= 0 &&
            file_exist(wall_image_path)) {
            snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s/wall/%s/%s.%s",
                     MUOS_IMAGE_PATH, program, element, wall_extension);
            return wall_image_embed;
        }
    }

    if (snprintf(wall_image_path, sizeof(wall_image_path), "/%s/wall/%s.%s",
                 MUOS_IMAGE_PATH, program, wall_extension) >= 0 &&
        file_exist(wall_image_path)) {
        snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s/wall/%s.%s",
                 MUOS_IMAGE_PATH, program, wall_extension);
        return wall_image_embed;
    }

    if (snprintf(wall_image_path, sizeof(wall_image_path), "/%s/wall/default.%s",
                 MUOS_IMAGE_PATH, wall_extension) >= 0 &&
        file_exist(wall_image_path)) {
        snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s/wall/default.%s",
                 MUOS_IMAGE_PATH, wall_extension);
        return wall_image_embed;
    }

    return "";
}

void load_font(const char *program, lv_obj_t *screen) {
    if (get_ini_int(muos_config, "settings.advanced", "font", LABEL)) {
        char theme_font_default[MAX_BUFFER_SIZE];
        char theme_font_mux[MAX_BUFFER_SIZE];
        snprintf(theme_font_default, sizeof(theme_font_default), "/%s/default.bin", MUOS_FONT_PATH);
        snprintf(theme_font_mux, sizeof(theme_font_mux), "/%s/%s.bin", MUOS_FONT_PATH, program);
        if (file_exist(theme_font_mux)) {
            char theme_font_mux_fs[MAX_BUFFER_SIZE];
            snprintf(theme_font_mux_fs, sizeof(theme_font_mux_fs), "M:%s/%s.bin", MUOS_FONT_PATH, program);
            lv_obj_set_style_text_font(screen, lv_font_load(theme_font_mux_fs), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            if (file_exist(theme_font_default)) {
                char theme_font_default_fs[MAX_BUFFER_SIZE];
                snprintf(theme_font_default_fs, sizeof(theme_font_default_fs), "M:%s/default.bin", MUOS_FONT_PATH);
                lv_obj_set_style_text_font(screen, lv_font_load(theme_font_default_fs),
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
}

int is_network_connected() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Socket creation error");
        return 0;
    }

    struct ifreq iface;
    memset(&iface, 0, sizeof(iface));

    const char *config_iface = get_ini_string(muos_config, "network", "interface");
    snprintf(iface.ifr_name, sizeof(iface.ifr_name), "%s", config_iface);

    if (ioctl(sock, SIOCGIFFLAGS, &iface) == -1) {
        perror("IOCTL error");
        close(sock);
        return 0;
    }

    close(sock);

    return (iface.ifr_flags & IFF_UP) && (iface.ifr_flags & IFF_RUNNING);
}

void process_visual_element(const char *visual, lv_obj_t *element) {
    if (!get_ini_int(muos_config, "visual", visual, LABEL)) {
        lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
    }
}

int get_volume_percentage() {
    char command[MAX_BUFFER_SIZE];
    char result[MAX_BUFFER_SIZE];
    int volume_percentage;

    snprintf(command, sizeof(command), "amixer get \"%s\" | grep -oE '[0-9]+' | tail -n 1", VOL_SPK_MASTER);

    FILE * pipe;
    pipe = popen(command, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Failed to run command\n");
        return -1;
    }

    if (fgets(result, sizeof(result), pipe) != NULL) {
        result[strcspn(result, "\n")] = '\0';
        volume_percentage = atoi(result);
    } else {
        fprintf(stderr, "Failed to read output from command\n");
        pclose(pipe);
        return -1;
    }

    pclose(pipe);
    return volume_percentage;
}

int should_skip(char *name) {
    const char skip_prefix[] = {
            '.', '_'
    };
    for (int i = 0; i < sizeof(skip_prefix) / sizeof(skip_prefix[0]); i++) {
        if (name[0] == skip_prefix[i] && !get_ini_int(muos_config, "settings.general", "hidden", 0)) {
            return 1;
        }
    }

    const char *skip_directories[] = {
            DUMMY_DIR
    };
    for (int i = 0; i < sizeof(skip_directories) / sizeof(skip_directories[0]); i++) {
        if (strcasecmp(name, skip_directories[i]) == 0) {
            return 1;
        }
    }

    const char *skip_files[] = {
            DUMMY_ROM, "control.txt", "neogeo.zip", "easy_rpg.log"
    };
    for (int i = 0; i < sizeof(skip_files) / sizeof(skip_files[0]); i++) {
        if (strcasecmp(name, skip_files[i]) == 0) {
            return 1;
        }
    }

    const char *skip_extensions[] = {
            ".bps", ".ips", ".sav", ".srm",
            ".ups", ".msu", ".pcm"
    };

    for (int i = 0; i < sizeof(skip_extensions) / sizeof(skip_extensions[0]); i++) {
        if (strcasecmp(get_ext(name), skip_extensions[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

int get_brightness_percentage(int brightness) {
    int brightness_percentage;
    brightness_percentage = (brightness * 100) / BL_MAX;

    if (brightness_percentage < 0)
        brightness_percentage = 0;
    else if (brightness_percentage > 100)
        brightness_percentage = 100;

    return brightness_percentage;
}

int get_brightness() {
    int current_brightness = -1;
    int disp = open("/dev/disp", O_RDWR);

    if (disp >= 0) {
        unsigned long b_val[3];
        memset(b_val, 0, sizeof(b_val));
        b_val[0] = 0;
        current_brightness = ioctl(disp, DISP_LCD_GET_BRIGHTNESS, (void *) b_val);
        close(disp);
    }

    return current_brightness;
}

void set_brightness(int brightness) {
    int disp = open("/dev/disp", O_RDWR);

    if (disp >= 0) {
        unsigned long b_val[3];
        memset(b_val, 0, sizeof(b_val));
        b_val[0] = 0;
        b_val[1] = brightness;
        ioctl(disp, DISP_LCD_SET_BRIGHTNESS, (void *) b_val);
        close(disp);
    }
}

char *get_friendly_name(char *search, char *file) {
    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command),
             "/opt/muos/bin/rg -w --max-count=1 \"%s\" \"%s\" | cut -d'=' -f2-",
             search, file);

    FILE * fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to obtain name\n");
        return search;
    }

    char result[MAX_BUFFER_SIZE];
    if (fgets(result, sizeof(result), fp) != NULL) {
        result[strcspn(result, "\n")] = '\0';
        pclose(fp);
        return strdup(result);
    }

    pclose(fp);

    puts(search);

    return search;
}
