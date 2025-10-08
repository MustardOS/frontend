#pragma once

#include <SDL2/SDL_mixer.h>
#include "../lvgl/lvgl.h"
#include "mini/mini.h"
#include "options.h"

#define A_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BIT(n) (UINT64_C(1) << (n))
#define TS(str) translate_specific(str)

extern int msgbox_active;
extern int block_input;
extern lv_obj_t *msgbox_element;
extern int battery_capacity;
extern int fe_snd;
extern int fe_bgm;
extern int progress_onscreen;
extern struct mux_config config;
extern char mux_dimension[15];
extern char mux_module[MAX_BUFFER_SIZE];
extern char current_wall[MAX_BUFFER_SIZE];
extern int is_silence_playing;
extern Mix_Music *current_bgm;
extern int current_brightness;
extern int current_volume;
extern int is_blank;

extern char *theme_back_compat[];

extern char *disabled_enabled[];
extern char *allowed_restricted[];

#define SOUND_TOTAL 12

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

enum toast_show {
    FOREVER,
    SHORT = 1000,
    MEDIUM = 1750,
    LONG = 2500
};

/* The following enums start at 1 because the values are used to
 * reference specific line numbers in associated core content files.
 *
 * Index 0 is unused, and line 'n' corresponds directly to value 'n'.
 */
enum content_type {
    CONTENT_NAME = 1,
    CONTENT_CORE,
    CONTENT_SYSTEM,
    CONTENT_CATALOGUE,
    CONTENT_LOOKUP,
    CONTENT_ASSIGN,
    CONTENT_MOUNT,
    CONTENT_DIR,
    CONTENT_FULL
};

enum global_type {
    GLOBAL_CORE = 1,
    GLOBAL_SYSTEM,
    GLOBAL_CATALOGUE,
    GLOBAL_LOOKUP,
    GLOBAL_ASSIGN
};

enum cache_type {
    CACHE_CORE_PATH = 1,
    CACHE_CORE_DIR,
    CACHE_CORE_NAME
};

enum parse_mode {
    LINES, TOKENS
};

enum wall_type {
    APPLICATION, ARCHIVE, GENERAL, TASK
};

enum count_type {
    FILES_ONLY, DIRECTORIES_ONLY, BOTH
};

enum visual_type {
    CLOCK, BLUETOOTH, NETWORK, BATTERY
};

enum time_type {
    TIME_12H, TIME_24H
};

enum sound_type {
    SND_CONFIRM, SND_BACK, SND_KEYPRESS, SND_NAVIGATE,
    SND_ERROR, SND_MUOS, SND_REBOOT, SND_SHUTDOWN,
    SND_STARTUP, SND_INFO_OPEN, SND_INFO_CLOSE, SND_OPTION
};

typedef struct {
    Mix_Chunk *chunk;
} CachedSound;

extern CachedSound sound_cache[SOUND_TOTAL];

enum write_file_type {
    CHAR, INT
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

int file_exist(char *filename);

int directory_exist(char *dirname);

const char **build_term_exec(const char **term_cmd, size_t *term_cnt);

void extract_archive(char *filename, char *screen);

void update_bootlogo();

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

char *str_capital(char *text);

char *str_capital_all(char *text);

char *str_rem_last_char(char *text, int count);

char *str_tolower(char *text);

char *str_toupper(char *text);

char *get_last_subdir(char *text, char separator, int n);

char *get_last_dir(char *text);

void remove_double_slashes(char *str);

char *strip_dir(char *text);

char *strip_ext(char *text);

char *grab_ext(char *text);

char *get_execute_result(const char *command);

int read_battery_capacity();

char *read_battery_voltage();

char *read_all_char_from(const char *filename);

char *read_line_char_from(const char *filename, size_t line_number);

int read_all_int_from(const char *filename, size_t buffer);

int read_line_int_from(const char *filename, size_t line_number);

unsigned long long read_all_long_from(const char *filename);

const char *get_random_hex();

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key, uint32_t default_value);

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, int16_t default_value);

char *get_ini_string(mini_t *ini_config, const char *section, const char *key, char *default_value);

void write_text_to_file(const char *filename, const char *mode, int type, ...);

void create_directories(const char *path);

void show_help_msgbox(lv_obj_t *panel, lv_obj_t *header_element, lv_obj_t *content_element,
                      char *header_text, char *content_text);

void show_info_box(char *title, char *content, int is_content);

void nav_move(lv_group_t *group, int direction);

void nav_prev(lv_group_t *group, int count);

void nav_next(lv_group_t *group, int count);

char *get_datetime();

void datetime_task(lv_timer_t *timer);

char *get_capacity();

void capacity_task();

void increase_option_value(lv_obj_t *element);

void decrease_option_value(lv_obj_t *element);

void load_assign(const char *loader, const char *rom, const char *dir, const char *sys, int forced, int app);

void load_mux(const char *value);

void play_sound(int sound);

void delete_files_of_type(const char *dir_path, const char *extension, const char *exception[], int recursive);

void delete_files_of_name(const char *dir_path, const char *filename);

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall,
                    lv_obj_t *ui_imgWall, int wall_type);

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, int wall_type);

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image);

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image);

int load_terminal_resource(const char *resource, const char *extension, char *buffer, size_t size);

int load_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                         const char *image_type, const char *image_extension, char *image_path, size_t path_size);

int load_element_image_specifics(const char *theme_base, const char *mux_dimension, const char *program,
                                 const char *image_type, const char *element, const char *element_fallback,
                                 const char *image_extension, char *image_path, size_t path_size);

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path);

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, int repeat_count, char *current_wall);

void unload_image_animation();

void load_font_text(lv_obj_t *screen);

void load_font_section(const char *section, lv_obj_t *element);

int is_network_connected();

void process_visual_element(enum visual_type visual, lv_obj_t *element);

void load_skip_patterns();

int should_skip(const char *name, int is_dir);

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

char *get_script_value(const char *filename, const char *key, const char *not_found);

int resolution_check(const char *zip_filename);

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path);

char **get_subdirectories(const char *base_dir);

void free_subdirectories(char **dir_names);

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index);

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value);

void play_silence_bgm(void);

int init_audio_backend(void);

void init_fe_snd(int *fe_snd, int snd_type, int re_init);

void init_fe_bgm(int *fe_bgm, int bgm_type, int re_init);

int safe_atoi(const char *str);

void init_grid_info(int item_count, int column_count);

int get_grid_row_index(int current_item_index);

int get_grid_column_index(int current_item_index);

int get_grid_row_item_count(int current_item_index);

void kiosk_denied();

void run_exec(const char *args[], size_t size, int background);

char *get_content_line(char *dir, char *name, char *ext, size_t line);

char *get_application_line(char *dir, char *ext, size_t line);

int load_image_catalogue(const char *catalogue_name, const char *program, const char *program_alt,
                         const char *program_default, const char *mux_dimension, const char *image_type,
                         char *image_path, size_t path_size);

struct screen_dimension get_device_dimensions();

void set_nav_flags(struct nav_flag *nav_flags, size_t count);

int16_t validate_int16(int value, const char *field);

int at_base(char *sys_dir, char *base_name);

int search_for_config(const char *base_path, const char *file_name, const char *system_name);

uint32_t fnv1a_hash_str(const char *str);

uint32_t fnv1a_hash_file(FILE *fp);

bool get_glyph_path(const char *mux_module, const char *glyph_name,
                    char *glyph_image_embed, size_t glyph_image_embed_size);

void apply_app_glyph(const char *app_name, const char *glyph_name, lv_obj_t *ui_lblItemGlyph);

void get_app_grid_glyph(const char *app_name, const char *glyph_name, const char *fallback_name, char *glyph_image_path, size_t glyph_image_path_size);

void populate_history_items();

void populate_collection_items();

char *get_content_explorer_glyph_name(char *file_path);

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count, int *nav_moved);

void load_splash_image_fallback(const char *mux_dimension, char *image, size_t image_size);

int theme_compat();

void update_bootlogo();

int brightness_to_percent(int val);

int volume_to_percent(int val);

char **str_parse_file(const char *filename, int *count, enum parse_mode mode);

int is_partition_mounted(const char *partition);

void get_storage_info(const char *partition, double *total, double *free, double *used);

char *get_build_version();

int copy_file(const char *from, const char *to);

int load_content(int add_collection, char *sys_dir, char *file_name);

char *load_content_core(int force, int run_quit, char *sys_dir, char *file_name);

char *build_core(char core_path[MAX_BUFFER_SIZE], int line_core, int line_system,
                 int line_catalogue, int line_lookup, int line_launch);

void add_to_collection(char *filename, const char *pointer, char *sys_dir);

int set_scaling_governor(const char *governor);
