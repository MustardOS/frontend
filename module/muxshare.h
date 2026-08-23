#pragma once

// IWYU pragma: begin_exports
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <pthread.h>
#include "../lvgl/lvgl.h"
#include "../common/mini/mini.h"
#include "../common/init.h"
#include "../common/log.h"
#include "../common/options.h"
#include "../common/device.h"
#include "../common/config.h"
#include "../common/theme.h"
#include "../common/kiosk.h"
#include "../common/strutil.h"
#include "../common/fileio.h"
#include "../common/audio.h"
#include "../common/archive.h"
#include "../common/catalogue.h"
#include "../common/content.h"
#include "../common/exec.h"
#include "../common/sysinfo.h"
#include "../common/ini.h"
#include "../common/datetime.h"
#include "../common/util.h"
#include "../common/rgb.h"
#include "../common/debug.h"
#include "../common/display.h"
#include "../common/ui/dialogue.h"
#include "../common/video.h"
#include "../common/crash.h"
#include "../common/board.h"
#include "../common/union.h"
#include "../common/ui/common.h"
#include "../common/ui/transition.h"
#include "../common/ui/font.h"
#include "../common/ui/osk.h"
#include "../common/ui/nav.h"
#include "../common/ui/image.h"
#include "../common/ui/grid.h"
#include "../common/ui/glyph.h"
#include "../common/ui/cache.h"
#include "../common/overlay.h"
#include "../common/colour.h"
#include "../common/language.h"
#include "../common/collection/common.h"
#include "../common/collection/tag.h"
#include "../common/collection/order.h"
#include "../common/skip.h"
#include "../common/passcode.h"
#include "../common/timezone.h"
#include "../common/bluetooth.h"
#include "../common/verify.h"
#include "../common/core/common.h"
#include "../common/input/list_nav.h"
#include "../common/input/grid_nav.h"
#include "../common/json/json.h"
#include "../common/lookup.h"
// IWYU pragma: end_exports

extern size_t item_count;
extern content_item *items;

extern size_t tag_item_count;
extern tag_item *tag_items;

extern int verify_check;

extern int refresh_kiosk;
extern int refresh_config;
extern int refresh_device;
extern int refresh_resolution;

extern int nav_moved;
extern int current_item_index;
extern int first_open;
extern int nav_silent;
extern int ui_count_static;

extern int theme_down_index;

extern lv_obj_t *overlay_image;
extern lv_obj_t *kiosk_image;

extern lv_group_t *ui_group;
extern lv_group_t *ui_group_glyph;
extern lv_group_t *ui_group_panel;
extern lv_group_t *ui_group_value;

extern char box_image_previous_path[MAX_BUFFER_SIZE];
extern char preview_image_previous_path[MAX_BUFFER_SIZE];
extern char splash_image_previous_path[MAX_BUFFER_SIZE];
extern char sys_dir[MAX_BUFFER_SIZE];

enum passcode_type { pct_boot, pct_config, pct_launch };

int is_ksk(int k);

void set_owned_user_data(lv_obj_t *obj, void *data);

void hold_call_set(void);

void hold_call_release(void);

void run_tweak_script(const char *message);

void shuffle_index(int current, int *dir, int *target);

void adjust_box_art(void);

void set_nav_input_dir(enum nav_direction direction);

void setup_nav(const struct nav_bar *nav_items);

void nav_show_a(int show, const char *text);

void nav_show_lr(int show);

void set_preview_hint(int visible);

int preview_hint_active(void);

void header_and_footer_setup(void);

void overlay_display(void);

void viewport_refresh(
    lv_obj_t **ui_viewport_objects, const char *artwork_config, char *catalogue_folder, char *content_name
);

char *specify_asset(char *val, const char *def_val, const char *label);

char *load_content_governor(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

char *load_content_control_scheme(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

char *load_content_retroarch(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

char *load_content_filter(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

char *load_content_shader(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

char *load_content_overlay(char *sys_dir, const char *pointer, int force, int run_quit, int is_app);

int32_t get_directory_item_count(const char *base_dir, const char *dir_name, int run_skip);

void update_file_counter(lv_obj_t *counter, int file_count);

char *get_friendly_folder_name(char *folder_name, int fn_valid, struct json fn_json);

int folder_has_launch_file(char *base_dir, char *dir_name);

int folder_is_content(const char *base_dir, const char *dir_name);

void update_title(
    const char *folder_path, int fn_valid, struct json fn_json, const char *label, const char *module_path
);

void gen_label(const char *module, const char *item_glyph, const char *item_text);

void gen_peek_label(const char *module, const char *item_glyph, const char *item_text);

int launch_flag(int mode, int held);

void reset_ui_groups(void);

void add_ui_groups(lv_obj_t **options, lv_obj_t **values, lv_obj_t **glyphs, lv_obj_t **panels, int long_dot);

void adjust_gen_panel(void);

void ui_gen_refresh_task(lv_timer_t *timer __attribute__((unused)));

void gen_step_movement(int steps, int direction, int long_dot, int count_offset, int sound);

void list_nav_cb_prev(int steps);

void list_nav_cb_next(int steps);

void list_nav_cb_prev_nowrap(int steps);

void list_nav_cb_next_nowrap(int steps);

void handle_msgbox_dismiss(void);

int build_safe_path(char *dst, size_t n, const char *base, const char *name);

void resolve_friendly_name(const char *file_path, char *out);

void gen_item_from_files(const char *base_path, int file_count, char **file_names);

void adjust_label_value_width(const lv_obj_t *panel, const lv_obj_t *label, lv_obj_t *value);

void update_label_scroll(void);

void render_image_refresh(
    const char *image_type, char *h_core_artwork, char *h_file_name, lv_obj_t *ui_img_splash,
    lv_obj_t *ui_viewport_objects[], int *starter_image, int *splash_valid
);

void clear_box_image(void);

void render_video_refresh(const char *h_core_artwork, const char *h_file_name);

void create_marker_assignment(
    const char *ext, const char *op_label, const char *value, const char *rom, char *rom_dir, int is_app,
    enum gen_type method
);

char *read_shader_info(const char *shader_store, const char *key);

void net_trim(char *value);

int read_wpa_status_value(const char *key, char *value);

int read_connected_ssid(char *ssid);

int profile_matches_connected_ssid(const char *profile_name, const char *ssid);

void resolve_content_artwork_names(char *h_core_artwork, size_t core_size, char *h_file_name, size_t file_size);

void refresh_theme_preview_image(char *base_path, char *name, int *preview_index);

int muxaccess_main(void);

int muxactivity_main(void);

int muxapp_main(void);

void muxappcon_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxarchive_main(void);

void muxassign_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxbackup_main(void);

int muxbtall_main(void);

int muxbtcon_main(void);

int muxbtdev_main(void);

void muxcolfilter_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxcollect_main(int add, const char *dir, int last_index);

int muxconfig_main(void);

int muxconnect_main(void);

void muxcontrol_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxcustom_main(void);

int muxdanger_main(void);

int muxdetail_main(void);

int muxdevice_main(void);

int muxdistemp_main(void);

int muxdownload_main(char *type);

void muxgov_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxhdmi_main(void);

int muxhistory_main(int his_index);

int muxinfo_main(void);

int muxinstall_main(void);

int muxkiosk_main(void);

int muxlanguage_main(void);

int muxsoundfont_main(void);

int muxlaunch_main(void);

int muxlogo_main(void);

int muxnetadv_main(void);

int muxnetprofile_main(void);

int muxnetproxy_main(void);

int muxnetscan_main(void);

int muxnetwork_main(void);

int muxnews_main(void);

void muxoption_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

void muxoverlay_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxpass_main(int auth_type);

int muxpasscfg_main(void);

int muxpicker_main(char *type, char *ex_dir);

int muxremap_main(void);

int muxtheme_main(char *ex_dir);

int muxthird_main(void);

int muxplore_main(int index, char *dir);

int muxpower_main(void);

void muxraopt_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxrgb_main(void);

int muxrgbzone_main(void);

int muxrtc_main(void);

int muxsearch_main(char *dir);

void muxshader_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxshot_main(void);

int muxsort_main(void);

int muxorder_main(void);

int muxspace_main(void);

int muxsplash_main(char *splash_image, int apply_recolour);

int muxstorage_main(void);

int muxtext_main(void);

void muxtag_main(int auto_assign, const char *name, const char *dir, const char *sys, int app);

int muxtask_main(char *ex_dir);

int muxthemedown_main(void);

int muxthemefilter_main(void);

int muxtester_main(void);

int muxtimezone_main(void);

int muxtweakadv_main(void);

int muxtweakgen_main(void);

int muxlink_main(void);

int muxwebserv_main(void);

void resolve_grid_item_images(
    const char *mux_dim, const char *mux_module, const char *glyph_name, char *grid_img, size_t img_size,
    char *grid_img_foc, size_t foc_size
);

#define SAFE_DELETE(ELEMENT, DEL_FUNC)                                                                                 \
    do {                                                                                                               \
        if ((ELEMENT) != NULL) {                                                                                       \
            DEL_FUNC(ELEMENT);                                                                                         \
            (ELEMENT) = NULL;                                                                                          \
        }                                                                                                              \
    } while (0)

#define RESET_PATH(ELEMENT)                                                                                            \
    do {                                                                                                               \
        snprintf(ELEMENT, sizeof(ELEMENT), "");                                                                        \
    } while (0)

#define INIT_OPTION_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, OPTION, COUNT)                                             \
    do {                                                                                                               \
        int _idx = ((INDEX) < 0) ? ui_count_static : (ui_count_static + (INDEX));                                      \
                                                                                                                       \
        apply_theme_list_panel(ui_pnl_##NAME##_##MODULE);                                                              \
        apply_theme_option_item_label(&theme, ui_lbl_##NAME##_##MODULE, LABEL, (OPTION) != NULL && (COUNT) > 0);       \
        apply_theme_list_glyph(&theme, ui_ico_##NAME##_##MODULE, mux_module, GLYPH);                                   \
        apply_theme_list_drop_down(&theme, ui_lbl_##NAME##_##MODULE, ui_dro_##NAME##_##MODULE, NULL);                  \
                                                                                                                       \
        if ((OPTION) != NULL && (COUNT) > 0) {                                                                         \
            add_drop_down_options(ui_dro_##NAME##_##MODULE, OPTION, COUNT);                                            \
        }                                                                                                              \
                                                                                                                       \
        ui_objects[_idx] = ui_lbl_##NAME##_##MODULE;                                                                   \
        ui_objects_value[_idx] = ui_dro_##NAME##_##MODULE;                                                             \
        ui_objects_glyph[_idx] = ui_ico_##NAME##_##MODULE;                                                             \
        ui_objects_panel[_idx] = ui_pnl_##NAME##_##MODULE;                                                             \
                                                                                                                       \
        ui_count_static++;                                                                                             \
    } while (0)

#define HIDE_OPTION_ITEM(MODULE, NAME)                                                                                 \
    do {                                                                                                               \
        if (!lv_obj_has_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT)) {                                      \
            lv_obj_add_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_lbl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_ico_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_dro_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
                                                                                                                       \
            ui_count_static--;                                                                                         \
        }                                                                                                              \
    } while (0)

#define SHOW_OPTION_ITEM(MODULE, NAME)                                                                                 \
    do {                                                                                                               \
        if (lv_obj_has_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT)) {                                       \
            lv_obj_clear_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_lbl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_ico_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_dro_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
                                                                                                                       \
            ui_count_static++;                                                                                         \
        }                                                                                                              \
    } while (0)

#define INIT_STATIC_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, NOGEN)                                                     \
    do {                                                                                                               \
        int _idx = ((INDEX) < 0) ? ui_count_static : (ui_count_static + (INDEX));                                      \
                                                                                                                       \
        if (!(NOGEN)) {                                                                                                \
            apply_theme_list_panel(ui_pnl_##NAME##_##MODULE);                                                          \
            apply_theme_list_item(&theme, ui_lbl_##NAME##_##MODULE, LABEL);                                            \
            apply_theme_list_glyph(&theme, ui_ico_##NAME##_##MODULE, mux_module, GLYPH);                               \
        }                                                                                                              \
                                                                                                                       \
        ui_objects[_idx] = ui_lbl_##NAME##_##MODULE;                                                                   \
                                                                                                                       \
        if (!(NOGEN)) {                                                                                                \
            ui_objects_glyph[_idx] = ui_ico_##NAME##_##MODULE;                                                         \
            ui_objects_panel[_idx] = ui_pnl_##NAME##_##MODULE;                                                         \
        }                                                                                                              \
                                                                                                                       \
        apply_size_to_content(                                                                                         \
            &theme, ui_pnl_##NAME##_##MODULE, ui_lbl_##NAME##_##MODULE, ui_ico_##NAME##_##MODULE, LABEL                \
        );                                                                                                             \
        apply_text_long_dot(&theme, ui_lbl_##NAME##_##MODULE);                                                         \
                                                                                                                       \
        ui_count_static++;                                                                                             \
    } while (0)

#define HIDE_STATIC_ITEM(MODULE, NAME)                                                                                 \
    do {                                                                                                               \
        lv_obj_add_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                             \
        lv_obj_add_flag(ui_lbl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                             \
        lv_obj_add_flag(ui_ico_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                             \
                                                                                                                       \
        ui_count_static--;                                                                                             \
    } while (0)

#define INIT_VALUE_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, VALUE)                                                      \
    do {                                                                                                               \
        int _idx = ((INDEX) < 0) ? ui_count_static : (ui_count_static + (INDEX));                                      \
                                                                                                                       \
        apply_theme_list_panel(ui_pnl_##NAME##_##MODULE);                                                              \
        apply_theme_option_item_label(&theme, ui_lbl_##NAME##_##MODULE, LABEL, 1);                                     \
        apply_theme_list_glyph(&theme, ui_ico_##NAME##_##MODULE, mux_module, GLYPH);                                   \
        apply_theme_list_value(&theme, ui_val_##NAME##_##MODULE, VALUE);                                               \
                                                                                                                       \
        ui_objects[_idx] = ui_lbl_##NAME##_##MODULE;                                                                   \
        ui_objects_value[_idx] = ui_val_##NAME##_##MODULE;                                                             \
        ui_objects_glyph[_idx] = ui_ico_##NAME##_##MODULE;                                                             \
        ui_objects_panel[_idx] = ui_pnl_##NAME##_##MODULE;                                                             \
                                                                                                                       \
        ui_count_static++;                                                                                             \
    } while (0)

#define HIDE_VALUE_ITEM(MODULE, NAME)                                                                                  \
    do {                                                                                                               \
        if (!lv_obj_has_flag(ui_pnl_##NAME##_##MODULE, LV_OBJ_FLAG_HIDDEN)) {                                          \
            lv_obj_add_flag(ui_lbl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_ico_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_val_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
            lv_obj_add_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                         \
                                                                                                                       \
            lv_group_remove_obj(ui_pnl_##NAME##_##MODULE);                                                             \
            lv_group_remove_obj(ui_lbl_##NAME##_##MODULE);                                                             \
            lv_group_remove_obj(ui_ico_##NAME##_##MODULE);                                                             \
            lv_group_remove_obj(ui_val_##NAME##_##MODULE);                                                             \
                                                                                                                       \
            ui_count_static--;                                                                                         \
        }                                                                                                              \
    } while (0)

#define SHOW_VALUE_ITEM(MODULE, NAME)                                                                                  \
    do {                                                                                                               \
        if (lv_obj_has_flag(ui_pnl_##NAME##_##MODULE, LV_OBJ_FLAG_HIDDEN)) {                                           \
            lv_obj_clear_flag(ui_pnl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_lbl_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_ico_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
            lv_obj_clear_flag(ui_val_##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);                                       \
                                                                                                                       \
            lv_group_add_obj(ui_group_panel, ui_pnl_##NAME##_##MODULE);                                                \
            lv_group_add_obj(ui_group, ui_lbl_##NAME##_##MODULE);                                                      \
            lv_group_add_obj(ui_group_glyph, ui_ico_##NAME##_##MODULE);                                                \
            lv_group_add_obj(ui_group_value, ui_val_##NAME##_##MODULE);                                                \
                                                                                                                       \
            ui_count_static++;                                                                                         \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_KSK(MODULE, NAME, FILE, TYPE)                                                                   \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_KIOSK_PATH FILE, "w", TYPE, current)) save_failed++;                          \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_STD(MODULE, NAME, FILE, TYPE, OFFSET)                                                           \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_CONFIG_PATH FILE, "w", TYPE, current + OFFSET)) save_failed++;                \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_DEV(MODULE, NAME, FILE, TYPE, OFFSET)                                                           \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_DEVICE_PATH FILE, "w", TYPE, current + OFFSET)) save_failed++;                \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_DEV_VAL(MODULE, NAME, FILE, TYPE, VALUES)                                                       \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_DEVICE_PATH FILE, "w", TYPE, VALUES[current])) save_failed++;                 \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_VAL(MODULE, NAME, FILE, TYPE, VALUES)                                                           \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_CONFIG_PATH FILE, "w", TYPE, VALUES[current])) save_failed++;                 \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_MAP(MODULE, NAME, FILE, VALUES, COUNT, DEFAULT)                                                 \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        if (current != NAME##_original) {                                                                              \
            int mapped = map_drop_down_to_value(current, VALUES, COUNT, DEFAULT);                                      \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_CONFIG_PATH FILE, "w", INT, mapped)) save_failed++;                           \
        }                                                                                                              \
    } while (0)

#define CHECK_AND_SAVE_PCT(MODULE, NAME, FILE, TYPE, VAL_MIN, VAL_MAX)                                                 \
    do {                                                                                                               \
        int current = lv_dropdown_get_selected(ui_dro_##NAME##_##MODULE);                                              \
        int value = ((VAL_MIN) == (VAL_MAX)) ? (VAL_MIN) : pct_to_int(current, VAL_MIN, VAL_MAX);                      \
        if (value != NAME##_original) {                                                                                \
            is_modified++;                                                                                             \
            if (!write_text_to_file(CONF_CONFIG_PATH FILE, "w", TYPE, value)) save_failed++;                           \
        }                                                                                                              \
    } while (0)

#define REPORT_SAVE_FAILURE()                                                                                          \
    do {                                                                                                               \
        if (save_failed > 0) {                                                                                         \
            LOG_ERROR(mux_module, "%s (%d)", lang.generic.save_fail, save_failed);                                     \
            toast_message(lang.generic.save_fail, tst_wait_m);                                                         \
        }                                                                                                              \
    } while (0)

#define SELECT_VISIBLE_ENTRY(ENTRIES, ENTRY_VAR)                                                                       \
    const menu_entry *ENTRY_VAR;                                                                                       \
    do {                                                                                                               \
        const menu_entry *visible_entries[ui_count_dynamic];                                                           \
        size_t visible_count = 0;                                                                                      \
        for (size_t _sve_i = 0; _sve_i < A_SIZE(ENTRIES); _sve_i++) {                                                  \
            if ((ENTRIES)[_sve_i].visible && !(ENTRIES)[_sve_i].visible()) continue;                                   \
            visible_entries[visible_count++] = &(ENTRIES)[_sve_i];                                                     \
        }                                                                                                              \
        if ((unsigned) current_item_index >= visible_count) return;                                                    \
        ENTRY_VAR = visible_entries[current_item_index];                                                               \
    } while (0)

#define OPTION_APPLY_WIDTH(NAME)                                                                                       \
    adjust_label_value_width(ui_pnl_##NAME##_option, ui_val_##NAME##_option, ui_lbl_##NAME##_option)

#define OPTION_APPLY_LONG(NAME)                                                                                        \
    apply_text_long_dot(&theme, ui_lbl_##NAME##_option);                                                               \
    apply_text_long_dot(&theme, ui_val_##NAME##_option)
