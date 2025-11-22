#pragma once

#include <assert.h>
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

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "../lvgl/lvgl.h"
#include "../common/init.h"
#include "../common/log.h"
#include "../common/options.h"
#include "../common/device.h"
#include "../common/config.h"
#include "../common/theme.h"
#include "../common/kiosk.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/common_core.h"
#include "../common/overlay.h"
#include "../common/language.h"
#include "../common/collection.h"
#include "../common/passcode.h"
#include "../common/timezone.h"
#include "../common/img/nothing.h"
#include "../common/input/list_nav.h"
#include "../common/json/json.h"
#include "../font/notosans_big.h"
#include "../font/notosans_big_hd.h"
#include "../lookup/lookup.h"

extern size_t item_count;
extern content_item *items;

extern int refresh_kiosk;
extern int refresh_config;
extern int refresh_device;
extern int refresh_resolution;

extern int nav_moved;
extern int current_item_index;
extern int first_open;
extern int ui_count;
extern int hold_call;

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

enum passcode_type {
    PCT_BOOT,
    PCT_CONFIG,
    PCT_LAUNCH
};

int is_ksk(int k);

void hold_call_set(void);

void hold_call_release(void);

void run_tweak_script();

void shuffle_index(int current, int *dir, int *target);

void adjust_box_art();

void setup_nav(struct nav_bar *nav_items);

void header_and_footer_setup();

void overlay_display();

void viewport_refresh(lv_obj_t **ui_viewport_objects, char *artwork_config,
                      char *catalogue_folder, char *content_name);

char *specify_asset(char *val, const char *def_val, const char *label);

char *load_content_governor(char *sys_dir, char *pointer, int force, int run_quit, int is_app);

char *load_content_control_scheme(char *sys_dir, char *pointer, int force, int run_quit, int is_app);

int32_t get_directory_item_count(const char *base_dir, const char *dir_name, int run_skip);

void update_file_counter(lv_obj_t *counter, int file_count);

char *get_friendly_folder_name(char *folder_name, int fn_valid, struct json fn_json);

void update_title(char *folder_path, int fn_valid, struct json fn_json,
                  const char *label, const char *module_path);

void gen_label(char *module, char *item_glyph, char *item_text);

int launch_flag(int mode, int held);

int muxapp_main();

int muxappcon_main(int auto_assign, char *name, char *dir, char *sys, int app);

int muxarchive_main();

int muxassign_main(int auto_assign, char *name, char *dir, char *sys, int app);

int muxbackup_main();

int muxcollect_main(int add, char *dir, int last_index);

int muxconfig_main();

int muxconnect_main();

int muxcontrol_main(int auto_assign, char *name, char *dir, char *sys, int app);

int muxcustom_main();

int muxdanger_main();

int muxdevice_main();

int muxdownload_main(char *type);

int muxgov_main(int auto_assign, char *name, char *dir, char *sys, int app);

int muxhdmi_main();

int muxhistory_main(int his_index);

int muxinfo_main();

int muxinstall_main();

int muxkiosk_main();

int muxlanguage_main();

int muxlaunch_main();

int muxnetadv_main();

int muxnetinfo_main();

int muxnetprofile_main();

int muxnetscan_main();

int muxnetwork_main();

int muxoption_main(int nothing, char *name, char *dir, char *sys, int app);

int muxpass_main(int auth_type);

int muxpicker_main(char *type, char *ex_dir);

int muxplore_main(int index, char *dir);

int muxpower_main();

int muxrtc_main();

int muxsearch_main(char *dir);

int muxshot_main();

int muxspace_main();

int muxsplash_main(char *splash_image, bool apply_recolour);

int muxstorage_main();

int muxsysinfo_main();

int muxtext_main();

int muxtag_main(int nothing, char *name, char *dir, char *sys, int app);

int muxtask_main(char *ex_dir);

int muxthemedown_main();

int muxthemefilter_main();

int muxtester_main();

int muxtimezone_main();

int muxtweakadv_main();

int muxtweakgen_main();

int muxvisual_main();

int muxwebserv_main();

#define SAFE_DELETE(ELEMENT, DEL_FUNC) \
    do {                               \
        if ((ELEMENT) != NULL) {       \
            DEL_FUNC(ELEMENT);         \
            (ELEMENT) = NULL;          \
        }                              \
    } while (0)

#define INIT_OPTION_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, OPTION, COUNT)          \
    do {                                                                            \
        int _idx = ((INDEX) < 0) ? ui_count : (ui_count + (INDEX));                 \
                                                                                    \
        apply_theme_list_panel(ui_pnl##NAME##_##MODULE);                            \
        apply_theme_list_item(&theme, ui_lbl##NAME##_##MODULE, LABEL);              \
        apply_theme_list_glyph(&theme, ui_ico##NAME##_##MODULE, mux_module, GLYPH); \
        apply_theme_list_drop_down(&theme, ui_dro##NAME##_##MODULE, NULL);          \
                                                                                    \
        if ((OPTION) != NULL && (COUNT) > 0) {                                      \
            add_drop_down_options(ui_dro##NAME##_##MODULE, OPTION, COUNT);          \
        }                                                                           \
                                                                                    \
        ui_objects[_idx] = ui_lbl##NAME##_##MODULE;                                 \
        ui_objects_value[_idx] = ui_dro##NAME##_##MODULE;                           \
        ui_objects_glyph[_idx] = ui_ico##NAME##_##MODULE;                           \
        ui_objects_panel[_idx] = ui_pnl##NAME##_##MODULE;                           \
                                                                                    \
        ui_count++;                                                                 \
    } while (0)

#define HIDE_OPTION_ITEM(MODULE, NAME)                                    \
    do {                                                                  \
        lv_obj_add_flag(ui_pnl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
        lv_obj_add_flag(ui_lbl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
        lv_obj_add_flag(ui_ico##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
        lv_obj_add_flag(ui_dro##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
                                                                          \
        ui_count--;                                                       \
    } while (0)

#define INIT_STATIC_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, NOGEN)                                                       \
    do {                                                                                                                 \
        int _idx = ((INDEX) < 0) ? ui_count : (ui_count + (INDEX));                                                      \
                                                                                                                         \
        if (!(NOGEN)) {                                                                                                  \
            apply_theme_list_panel(ui_pnl##NAME##_##MODULE);                                                             \
            apply_theme_list_item(&theme, ui_lbl##NAME##_##MODULE, LABEL);                                               \
            apply_theme_list_glyph(&theme, ui_ico##NAME##_##MODULE, mux_module, GLYPH);                                  \
        }                                                                                                                \
                                                                                                                         \
        ui_objects[_idx] = ui_lbl##NAME##_##MODULE;                                                                      \
                                                                                                                         \
        if (!(NOGEN)) {                                                                                                  \
            ui_objects_glyph[_idx] = ui_ico##NAME##_##MODULE;                                                            \
            ui_objects_panel[_idx] = ui_pnl##NAME##_##MODULE;                                                            \
        }                                                                                                                \
                                                                                                                         \
        apply_size_to_content(&theme, ui_pnl##NAME##_##MODULE, ui_lbl##NAME##_##MODULE, ui_ico##NAME##_##MODULE, LABEL); \
        apply_text_long_dot(&theme, ui_pnl##NAME##_##MODULE, ui_lbl##NAME##_##MODULE);                                   \
                                                                                                                         \
        ui_count++;                                                                                                      \
    } while (0)

#define HIDE_STATIC_ITEM(MODULE, NAME)                                    \
    do {                                                                  \
        lv_obj_add_flag(ui_pnl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
        lv_obj_add_flag(ui_lbl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
        lv_obj_add_flag(ui_ico##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
                                                                          \
        ui_count--;                                                       \
    } while (0)

#define INIT_VALUE_ITEM(INDEX, MODULE, NAME, LABEL, GLYPH, VALUE)                   \
    do {                                                                            \
        int _idx = ((INDEX) < 0) ? ui_count : (ui_count + (INDEX));                 \
                                                                                    \
        apply_theme_list_panel(ui_pnl##NAME##_##MODULE);                            \
        apply_theme_list_item(&theme, ui_lbl##NAME##_##MODULE, LABEL);              \
        apply_theme_list_glyph(&theme, ui_ico##NAME##_##MODULE, mux_module, GLYPH); \
        apply_theme_list_value(&theme, ui_lbl##NAME##Value_##MODULE, VALUE);        \
                                                                                    \
        ui_objects[_idx] = ui_lbl##NAME##_##MODULE;                                 \
        ui_objects_value[_idx] = ui_lbl##NAME##Value_##MODULE;                      \
        ui_objects_glyph[_idx] = ui_ico##NAME##_##MODULE;                           \
        ui_objects_panel[_idx] = ui_pnl##NAME##_##MODULE;                           \
                                                                                    \
        ui_count++;                                                                 \
    } while (0)

#define HIDE_VALUE_ITEM(MODULE, NAME)                                          \
    do {                                                                       \
        lv_obj_add_flag(ui_pnl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);      \
        lv_obj_add_flag(ui_lbl##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);      \
        lv_obj_add_flag(ui_ico##NAME##_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT);      \
        lv_obj_add_flag(ui_lbl##NAME##Value_##MODULE, MU_OBJ_FLAG_HIDE_FLOAT); \
                                                                               \
        ui_count--;                                                            \
    } while (0)

#define CHECK_AND_SAVE_KSK(MODULE, NAME, FILE, TYPE)                         \
    do {                                                                     \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);     \
        if (current != NAME##_original) {                                    \
            is_modified++;                                                   \
            write_text_to_file(CONF_KIOSK_PATH FILE, "w", TYPE, current);  \
        }                                                                    \
    } while (0)

#define CHECK_AND_SAVE_STD(MODULE, NAME, FILE, TYPE, OFFSET)                          \
    do {                                                                              \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);              \
        if (current != NAME##_original) {                                             \
            is_modified++;                                                            \
            write_text_to_file(CONF_CONFIG_PATH FILE, "w", TYPE, current + OFFSET); \
        }                                                                             \
    } while (0)

#define CHECK_AND_SAVE_DEV(MODULE, NAME, FILE, TYPE, OFFSET)                          \
    do {                                                                              \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);              \
        if (current != NAME##_original) {                                             \
            is_modified++;                                                            \
            write_text_to_file(CONF_DEVICE_PATH FILE, "w", TYPE, current + OFFSET); \
        }                                                                             \
    } while (0)

#define CHECK_AND_SAVE_DEV_VAL(MODULE, NAME, FILE, TYPE, VALUES)                     \
    do {                                                                             \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);             \
        if (current != NAME##_original) {                                            \
            is_modified++;                                                           \
            write_text_to_file(CONF_DEVICE_PATH FILE, "w", TYPE, VALUES[current]); \
        }                                                                            \
    } while (0)

#define CHECK_AND_SAVE_VAL(MODULE, NAME, FILE, TYPE, VALUES)                         \
    do {                                                                             \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);             \
        if (current != NAME##_original) {                                            \
            is_modified++;                                                           \
            write_text_to_file(CONF_CONFIG_PATH FILE, "w", TYPE, VALUES[current]); \
        }                                                                            \
    } while (0)

#define CHECK_AND_SAVE_MAP(MODULE, NAME, FILE, VALUES, COUNT, DEFAULT)            \
    do {                                                                          \
        int current = lv_dropdown_get_selected(ui_dro##NAME##_##MODULE);          \
        if (current != NAME##_original) {                                         \
            int mapped = map_drop_down_to_value(current, VALUES, COUNT, DEFAULT); \
            is_modified++;                                                        \
            write_text_to_file(CONF_CONFIG_PATH FILE, "w", INT, mapped);        \
        }                                                                         \
    } while (0)
