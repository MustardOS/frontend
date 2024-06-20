#pragma once

#include "../lvgl/lvgl.h"
#include "mini/mini.h"

extern int msgbox_active;
extern lv_obj_t *msgbox_element;
extern int turbo_mode;
extern int input_disable;
extern int nav_sound;
extern int bar_header;
extern int bar_footer;
extern char *osd_message;
extern struct mux_config config;

#define DISP_LCD_SET_BRIGHTNESS 0x102
#define DISP_LCD_GET_BRIGHTNESS 0x103

struct disp_bright_value {
    int screen;
    int brightness;
};

enum count_type {
    FILES_ONLY, DIRECTORIES_ONLY, BOTH
};

enum visual_type {
    CLOCK, BLUETOOTH, NETWORK, BATTERY
};

enum element_type {
    LABEL, VALUE, IGNORE
};

struct dt_task_param {
    lv_obj_t *lblDatetime;
};

struct bat_task_param {
    lv_obj_t *staCapacity;
};

struct osd_task_param {
    lv_obj_t *lblMessage;
    lv_obj_t *pnlMessage;
    int count;
};

struct image_info {
    char *type;
    char *path_format;
    lv_obj_t *ui_element;
    lv_obj_t *ui_panel;
};

int file_exist(char *filename);

int file_size(char *filename, int filesize);

unsigned long long total_file_size(const char *path);

int time_compare_for_history(const void *a, const void *b);

char *str_append(char *old_text, const char *new_text);

int str_compare(const void *a, const void *b);

int str_startswith(const char *a, const char *b);

char *str_nonew(char *text);

char *str_remchar(char *text, char c);

char *str_trim(char *text);

char *str_replace(char *orig, char *rep, char *with);

char *str_tolower(char *text);

int get_label_placement(char *text);

char *strip_label_placement(char *text);

char *get_last_subdir(char *text, char separator, int n);

char *get_last_dir(char *text);

char *strip_dir(char *text);

char *strip_ext(char *text);

char *get_ext(char *text);

char *get_execute_result(const char *command);

char *current_datetime();

int read_battery_capacity();

char *read_battery_health();

char *read_battery_voltage();

char *read_text_from_file(char *filename);

char *read_line_from_file(char *filename, int line_number);

const char *get_random_hex();

const char *get_random_int();

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key);

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, enum element_type type);

int set_ini_int(mini_t *ini_config, const char *section, const char *key, int value);

char* get_ini_string(mini_t* ini_config, const char* section, const char* key, char* default_value);

const char *get_ini_unicode(mini_t *ini_config, const char *section, const char *key);

int set_ini_string(mini_t *ini_config, const char *section, const char *key, const char *value);

char *format_meta_text(char *filename);

void write_text_to_file(const char *filename, const char *text, const char *mode);

void create_directories(const char *path);

const char *read_directory(const char *path);

int count_items(const char *path, enum count_type type);

int detect_sd2();

int detect_e_usb();

void show_help_msgbox(lv_obj_t *panel, lv_obj_t *header_element, lv_obj_t *content_element,
                      char *header_text, char *content_text);

void show_rom_info(lv_obj_t *panel, lv_obj_t *e_title, lv_obj_t *p_title, lv_obj_t *e_desc,
                   char *t_title, char *t_desc);

void nav_prev(lv_group_t *group, int count);

void nav_next(lv_group_t *group, int count);

char *get_datetime();

void datetime_task(lv_timer_t *timer);

char *get_capacity();

void capacity_task(lv_timer_t *timer);

void osd_task(lv_timer_t *timer);

void *turbo_task();

void set_governor(char *governor);

void set_cpu_scale(int speed);

void increase_option_value(lv_obj_t *element, int *current, int total);

void decrease_option_value(lv_obj_t *element, int *current, int total);

void load_system(const char *value);

void load_assign(const char *dir, const char *sys);

void load_mux(const char *value);

void play_sound(const char *filename, int nav_sound);

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[]);

void delete_files_of_name(const char *dir_path, const char *filename);

void hex_to_rgb(char hex[7], int *r, int *g, int *b);

char *extract_in_brackets(const char *input);

void set_image(char *path, lv_obj_t *ui_element, lv_obj_t *ui_panel);

char *load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated);

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group);

char *load_overlay_image();

void load_font_text(const char *program, lv_obj_t *screen);

void load_font_glyph(const char *program, lv_obj_t *element);

int is_network_connected();

void process_visual_element(enum visual_type visual, lv_obj_t *element);

int should_skip(char *name);

void adjust_visual_label(char *text, int method);
