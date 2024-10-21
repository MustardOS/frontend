#pragma once

#include "../lvgl/lvgl.h"
#include "mini/mini.h"

#define BIT(n) (UINT64_C(1) << (n))

#define TG(str) translate_generic(str)
#define TS(str) translate_specific(str)

extern int battery_capacity;
extern int msgbox_active;
extern lv_obj_t *msgbox_element;
extern int turbo_mode;
extern int input_disable;
extern int nav_sound;
extern int bar_header;
extern int bar_footer;
extern char *osd_message;
extern int progress_onscreen;
extern struct mux_config config;

struct ImageSettings {
    char *image_path;
    int16_t align;
    int16_t max_width;
    int16_t max_height;
    int16_t pad_left;
    int16_t pad_right;
    int16_t pad_top;
    int16_t pad_bottom;
};

enum count_type {
    FILES_ONLY, DIRECTORIES_ONLY, BOTH
};

enum visual_type {
    CLOCK, BLUETOOTH, NETWORK, BATTERY
};

enum write_file_type {
    CHAR, INT
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

struct pattern {
    char **patterns;
    size_t count;
    size_t capacity;
};

void refresh_screen();

int file_exist(char *filename);

int directory_exist(char *dirname);

unsigned long long total_file_size(const char *path);

int str_compare(const void *a, const void *b);

int str_startswith(const char *a, const char *b);

char *str_nonew(char *text);

char *str_remchar(char *text, char c);

char *str_remchars(char *text, char *c);

char *str_trim(char *text);

char *str_replace(char *orig, char *rep, char *with);

char *str_tolower(char *text);

char *get_last_subdir(char *text, char separator, int n);

char *get_last_dir(char *text);

char *strip_dir(char *text);

char *strip_ext(char *text);

char *get_execute_result(const char *command);

int read_battery_capacity();

char *read_battery_health();

char *read_battery_voltage();

char *read_text_from_file(char *filename);

char *read_line_from_file(const char *filename, size_t line_number);

int read_int_from_file(const char *filename, size_t line_number);

const char *get_random_hex();

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key);

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, int16_t default_value);

char *get_ini_string(mini_t *ini_config, const char *section, const char *key, char *default_value);

char *format_meta_text(char *filename);

void write_text_to_file(const char *filename, const char *mode, int type, ...);

void create_directories(const char *path);

int count_items(const char *path, enum count_type type);

int detect_storage(const char *target);

void show_help_msgbox(lv_obj_t *panel, lv_obj_t *header_element, lv_obj_t *content_element,
                      char *header_text, char *content_text);

void show_rom_info(lv_obj_t *panel, lv_obj_t *e_title, lv_obj_t *p_title, lv_obj_t *e_desc,
                   char *t_title, char *t_desc);

void nav_prev(lv_group_t *group, int count);

void nav_next(lv_group_t *group, int count);

char *get_datetime();

void datetime_task(lv_timer_t *timer);

char *get_capacity();

void capacity_task();

void osd_task(lv_timer_t *timer);

void increase_option_value(lv_obj_t *element);

void decrease_option_value(lv_obj_t *element);

void load_assign(const char *rom, const char *dir, const char *sys, int forced);

void load_gov(const char *rom, const char *dir, const char *sys, int forced);

void load_mux(const char *value);

void play_sound(const char *sound, int enabled, int wait);

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], int recursive);

void delete_files_of_name(const char *dir_path, const char *filename);

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall, lv_obj_t *ui_imgWall, 
            int animated, int animation_delay, int random);

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group);

void load_overlay_image(lv_obj_t *ui_screen, int16_t image_overlay_enabled);

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path);

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, char *current_wall);

void unload_image_animation();

void load_font_text(const char *program, lv_obj_t *screen);

void load_font_section(const char *program, const char *section, lv_obj_t *element);

int is_network_connected();

void process_visual_element(enum visual_type visual, lv_obj_t *element);

void load_skip_patterns();

int should_skip(const char *name);

void display_testing_message(lv_obj_t *screen);

void adjust_visual_label(char *text, int method, int rep_dash);

void update_image(lv_obj_t *ui_imgobj, struct ImageSettings image_settings);

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count, int current_item_index,
                            lv_obj_t *ui_pnlContent);

void load_language(const char *program);

char *translate_generic(char *key);

char *translate_help(char *key);

char *translate_specific(char *key);

void add_drop_down_options(lv_obj_t *ui_lblItemDropDown, char *options[], int count);

char *generate_number_string(int min, int max, int increment, const char *prefix, const char *infix,
                             const char *suffix, int infix_position);

char *get_script_value(const char *filename, const char *key);

void update_bars(lv_obj_t *bright_bar, lv_obj_t *volume_bar, lv_obj_t *volume_icon);

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path);

char **get_subdirectories(const char *base_dir);

void free_subdirectories(char **dir_names);

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index);

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value);

int init_nav_sound(const char *mux_module);
