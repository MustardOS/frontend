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
extern int nav_sound;
extern int bar_header;
extern int bar_footer;
extern int progress_onscreen;
extern struct mux_config config;
extern char *mux_module;
extern lv_obj_t *ui_lblDatetime;
extern lv_obj_t *ui_staCapacity;
extern lv_obj_t *ui_lblMessage;
extern lv_obj_t *ui_pnlMessage;

struct screen_dimension {
    int WIDTH;
    int HEIGHT;
};

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

struct grid_info {
    int item_count;
    int last_row_item_count;
    int column_count;
    int last_row_index;
};

extern struct grid_info grid_info;

enum wall_type {
    APPLICATION, ARCHIVE, GENERAL, TASK
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

struct pattern {
    char **patterns;
    size_t count;
    size_t capacity;
};

struct nav_flag {
    lv_obj_t *element;
    int visible;
};

void mux_init();

void input_init(int *js_fd, int *js_fd_sys);

void timer_init(void (*glyph_task_func)(lv_timer_t *), void (*ui_refresh_task)(lv_timer_t *),
                void (*update_system_info)(lv_timer_t *));

void refresh_screen(int wait);

int file_exist(char *filename);

int directory_exist(char *dirname);

unsigned long long total_file_size(const char *path);

int str_compare(const void *a, const void *b);

int str_startswith(const char *a, const char *b);

char *str_nonew(char *text);

char *str_remchar(char *text, char c);

char *str_remchars(char *text, char *c);

void str_split(char *text, char sep, char *p1, char *p2);

char *str_trim(char *text);

char *str_replace(const char *orig, const char *rep, const char *with);

int str_replace_segment(const char *orig, const char *prefix, const char *suffix,
                        const char *with, char **replacement);

int str_extract(const char *orig, const char *prefix, const char *suffix, char **extraction);

char *str_tolower(char *text);

char *get_last_subdir(char *text, char separator, int n);

char *get_last_dir(char *text);

char *strip_dir(char *text);

char *strip_ext(char *text);

char *get_execute_result(const char *command);

int read_battery_capacity();

char *read_battery_health();

char *read_battery_voltage();

char *read_text_from_file(const char *filename);

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

void increase_option_value(lv_obj_t *element);

void decrease_option_value(lv_obj_t *element);

void load_assign(const char *rom, const char *dir, const char *sys, int forced);

void load_gov(const char *rom, const char *dir, const char *sys, int forced);

void load_mux(const char *value);

void play_sound(const char *sound, int enabled, int wait, int background);

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], int recursive);

void delete_files_of_name(const char *dir_path, const char *filename);

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall, lv_obj_t *ui_imgWall,
                    int animated, int animation_delay, int random, int wall_type);

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, int wall_type);

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image, int16_t overlay_enabled);

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image);

int load_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                         const char *image_type, const char *image_extension, char *image_path, size_t path_size);

int load_element_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                                 const char *image_type, const char *element, const char *image_extension,
                                 char *image_path, size_t path_size);

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path);

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, char *current_wall);

void unload_image_animation();

void get_mux_dimension(char *mux_dimension, size_t size);

void load_font_text(const char *program, lv_obj_t *screen);

void load_font_section(const char *program, const char *section, lv_obj_t *element);

int is_network_connected();

void process_visual_element(enum visual_type visual, lv_obj_t *element);

void load_skip_patterns();

int should_skip(const char *name);

void display_testing_message(lv_obj_t *screen);

void adjust_visual_label(char *text, int method, int rep_dash);

void update_image(lv_obj_t *ui_imgobj, struct ImageSettings image_settings);

void update_grid_scroll_position(int col_count, int row_count, int row_height,
                                 int current_item_index, lv_obj_t *ui_pnlGrid);

void scroll_object_to_middle(lv_obj_t *container, lv_obj_t *obj);

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count, int current_item_index,
                            lv_obj_t *ui_pnlContent);

void load_language_file(const char *module);

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

int safe_atoi(const char *str);

void init_grid_info(int item_count, int column_count);

int get_grid_row_index(int current_item_index);

int get_grid_column_index(int current_item_index);

int get_grid_row_item_count(int current_item_index);

char *get_glyph_from_file(const char *directory_name, const char *item_name, char *item_default);

char *kiosk_nope();

void run_exec(const char *args[]);

char *get_directory_core(char *rom_dir);

char *get_file_core(char *rom_dir, char *rom_name);

char *get_directory_governor(char *rom_dir);

char *get_file_governor(char *rom_dir, char *rom_name);

int load_image_catalogue(const char *catalogue_name, const char *program, const char *program_fallback,
                         const char *mux_dimension, const char *image_type, char *image_path, size_t path_size);

struct screen_dimension get_device_dimensions();

void set_nav_flags(struct nav_flag *nav_flags, size_t count);

int16_t validate_int16(int value, const char *field);

int at_base(char *sys_dir, char *base_name);

int search_for_config(const char *base_path, const char *file_name, const char *system_name);

uint32_t fnv1a_hash(const char *str);

bool get_glyph_path(const char *mux_module, char *glyph_name, char *glyph_image_embed, size_t glyph_image_embed_size);

void populate_history_items();

void populate_collection_items();

char *get_content_explorer_glyph_name(char *file_path);
