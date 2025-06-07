#include "muxshare.h"

size_t item_count = 0;
content_item *items = NULL;

int refresh_kiosk = 0;
int refresh_config = 0;
int refresh_resolution = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 0;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;
lv_group_t *ui_group_value;

void setup_nav(struct nav_bar *nav_items) {
    for (size_t i = 0; nav_items[i].item != NULL; i++) {
        if (nav_items[i].ui_check && !ui_count) continue;

        if (nav_items[i].text && nav_items[i].text[0] != '\0') {
            lv_label_set_text(nav_items[i].item, nav_items[i].text);
        }

        lv_obj_clear_flag(nav_items[i].item, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_move_foreground(nav_items[i].item);
    }
}

void header_and_footer_setup() {
    lv_obj_set_style_bg_opa(ui_pnlHeader, theme.HEADER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlFooter, theme.FOOTER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");
}

void overlay_display() {
#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    if (kiosk.ENABLE) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

char *load_content_governor(char *sys_dir, char *pointer, int force, int run_quit) {
    char content_gov[MAX_BUFFER_SIZE];
    const char *last_subdir = NULL;

    if (pointer == NULL) {
        last_subdir = get_last_subdir(sys_dir, '/', 4);
        if (!strcasecmp(last_subdir, strip_dir(CONTENT_PATH))) {
            snprintf(content_gov, sizeof(content_gov), "%s/core.gov", INFO_COR_PATH);
        } else {
            snprintf(content_gov, sizeof(content_gov), "%s/%s/%s.gov",
                     INFO_COR_PATH, last_subdir, strip_ext(items[current_item_index].name));

            if (file_exist(content_gov) && !force) {
                LOG_SUCCESS(mux_module, "Loading Individual Governor: %s", content_gov)
                char *gov_text = read_all_char_from(content_gov);
                if (gov_text) return gov_text;
                LOG_ERROR(mux_module, "Failed to read individual governor")
            }

            snprintf(content_gov, sizeof(content_gov), "%s/%s/core.gov",
                     INFO_COR_PATH, last_subdir);
        }
    } else {
        snprintf(content_gov, sizeof(content_gov), "%s.gov", strip_ext(pointer));

        if (file_exist(content_gov)) {
            LOG_SUCCESS(mux_module, "Loading Individual Governor: %s", content_gov)
            return read_all_char_from(content_gov);
        }

        const char *replaced = str_replace(get_last_subdir(pointer, '/', 6), get_last_dir(pointer), "");
        snprintf(content_gov, sizeof(content_gov), "%s/%s/core.gov", INFO_COR_PATH, replaced);
        snprintf(content_gov, sizeof(content_gov), "%s", str_replace(content_gov, "//", "/"));
    }

    if (file_exist(content_gov) && !force) {
        LOG_SUCCESS(mux_module, "Loading Global Governor: %s", content_gov)
        char *gov = read_all_char_from(content_gov);
        if (gov) return gov;
        LOG_ERROR(mux_module, "Failed to read global governor")
    }

    if (run_quit) mux_input_stop();

    LOG_INFO(mux_module, "No governor detected")
    return NULL;
}

void viewport_refresh(lv_obj_t **ui_viewport_objects, char *artwork_config,
                      char *catalogue_folder, char *content_name) {
    mini_t *artwork_config_ini = mini_try_load(artwork_config);

    int16_t viewport_width = get_ini_int(artwork_config_ini, "viewport", "WIDTH", (int16_t) device.MUX.WIDTH / 2);
    int16_t viewport_height = get_ini_int(artwork_config_ini, "viewport", "HEIGHT", 400);
    int16_t column_mode = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE", 0);
    int16_t column_mode_alignment = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE_ALIGNMENT", 2);

    lv_obj_set_width(ui_viewport_objects[0], viewport_width == 0 ? LV_SIZE_CONTENT : viewport_width);
    lv_obj_set_height(ui_viewport_objects[0], viewport_height == 0 ? LV_SIZE_CONTENT : viewport_height);
    if (column_mode) {
        lv_obj_set_flex_flow(ui_viewport_objects[0], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_viewport_objects[0], LV_FLEX_ALIGN_CENTER, column_mode_alignment,
                              LV_FLEX_ALIGN_CENTER);
    }

    for (int index = 1; index < 6; index++) {
        char section_name[15];
        snprintf(section_name, sizeof(section_name), "image%d", index);
        char *folder_name = get_ini_string(artwork_config_ini, section_name, "FOLDER", "");

        char image[MAX_BUFFER_SIZE];
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                 INFO_CAT_PATH, catalogue_folder, folder_name, content_name);

        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/%s/%s/default.png",
                     INFO_CAT_PATH, catalogue_folder, folder_name);
        }

        struct ImageSettings image_settings = {
                image,
                get_ini_int(artwork_config_ini, section_name, "ALIGN", 9),
                get_ini_int(artwork_config_ini, section_name, "MAX_WIDTH", 0),
                get_ini_int(artwork_config_ini, section_name, "MAX_HEIGHT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_LEFT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_RIGHT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_TOP", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_BOTTOM", 0)
        };

        update_image(ui_viewport_objects[index], image_settings);
    }

    mini_free(artwork_config_ini);
}

int32_t get_directory_item_count(const char *base_dir, const char *dir_name, int run_skip) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, dir_name);

    DIR *dir = opendir(full_path);
    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return 0;
    }

    struct dirent *entry;
    int32_t dir_count = 0;

    while ((entry = readdir(dir))) {
        if (run_skip) {
            if (!should_skip(entry->d_name)) {
                if (entry->d_type == DT_DIR) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) dir_count++;
                } else if (entry->d_type == DT_REG) {
                    dir_count++;
                }
            }
        } else {
            if (entry->d_type == DT_REG) dir_count++;
        }
    }

    closedir(dir);
    return dir_count;
}

void update_file_counter(lv_obj_t *counter, int file_count) {
    if ((ui_count > 0 && !file_count && config.VISUAL.COUNTERFOLDER) ||
        (file_count > 0 && config.VISUAL.COUNTERFILE)) {
        char counter_text[MAX_BUFFER_SIZE];
        snprintf(counter_text, sizeof(counter_text), "%d%s%d",
                 current_item_index + 1, theme.COUNTER.TEXT_SEPARATOR, ui_count);
        counter_message(counter, counter_text, theme.COUNTER.TEXT_FADE_TIME * 60);
    } else {
        lv_obj_add_flag(counter, LV_OBJ_FLAG_HIDDEN);
    }
}
