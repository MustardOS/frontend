#include "muxshare.h"

size_t item_count = 0;
content_item *items = NULL;

size_t tag_item_count = 0;
tag_item *tag_items = NULL;

int verify_check = 0;

int refresh_kiosk = 0;
int refresh_config = 0;
int refresh_device = 0;
int refresh_resolution = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 0;
int current_item_index = 0;
int first_open = 1;
int nav_silent = 0;
int ui_count_static = 0;

int theme_down_index = 0;
int last_shuffle = -1;

lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;
lv_group_t *ui_group_value;

char box_image_previous_path[MAX_BUFFER_SIZE];
char preview_image_previous_path[MAX_BUFFER_SIZE];
char splash_image_previous_path[MAX_BUFFER_SIZE];
char sys_dir[MAX_BUFFER_SIZE];

int is_ksk(const int k) {
    return kiosk.enable ? k : 0;
}

void hold_call_set(void) {
    hold_call = 1;
}

void hold_call_release(void) {
    hold_call = 0;
}

void run_tweak_script(const char *message) {
    toast_message(message, tst_wait_f);

    const char *args[] = {OPT_PATH "script/mux/tweak.sh", NULL};
    run_exec(args, A_SIZE(args), 0, 0, NULL, NULL);

    refresh_config = 1;
    refresh_device = 1;
}

void shuffle_index(const int current, int *dir, int *target) {
    // We'll check this here again, better to be safe than sorry!
    if (ui_count_static < 2) {
        *target = current;
        *dir = +1;
        return;
    }

    int t = 0;
    for (int i = 0; i < 4; i++) {
        // Pick a random index, at least 3 times, excluding the current one
        // Probably wasteful but it no longer deadlocks!
        const int ran = (int) (random() % (long) (ui_count_static - 1));
        t = ran >= current ? ran + 1 : ran;

        // Avoid immediately repeating the last shuffled item if possible...
        if (t != last_shuffle) break;
    }

    last_shuffle = t;
    *target = t;
    *dir = t > current ? +1 : -1;
}

void adjust_box_art(void) {
    const int content_y = theme.header.height + 2;
    const int content_h = device.mux.height - theme.header.height - theme.footer.height - 4;

    switch (config.visual.box_art) {
        case 0: // Behind — within content area, behind content; header/footer clip
        case 1: // Front — within content area, in front of content; header/footer always on top
            lv_obj_set_x(ui_pnl_box, 0);
            lv_obj_set_y(ui_pnl_box, content_y);
            lv_obj_set_width(ui_pnl_box, device.mux.width);
            lv_obj_set_height(ui_pnl_box, content_h > 0 ? content_h : device.mux.height);

            if (config.visual.box_art == 0) {
                lv_obj_move_background(ui_pnl_box);
                lv_obj_move_background(ui_pnl_wall);
            } else {
                lv_obj_move_foreground(ui_pnl_box);
            }
            break;
        case 2: // Fullscreen + Behind — full screen, behind content; header/footer render on top but do not clip
        case 3: // Fullscreen + Front — full screen, in front of everything including header/footer
            lv_obj_set_x(ui_pnl_box, 0);
            lv_obj_set_y(ui_pnl_box, 0);
            lv_obj_set_width(ui_pnl_box, device.mux.width);
            lv_obj_set_height(ui_pnl_box, device.mux.height);

            if (config.visual.box_art == 2) {
                lv_obj_move_background(ui_pnl_box);
                lv_obj_move_background(ui_pnl_wall);
            } else {
                lv_obj_move_foreground(ui_pnl_box);
            }
            break;
        case 4: // Disabled
            lv_obj_add_flag(ui_pnl_box, MU_OBJ_FLAG_HIDE_FLOAT);
            break;
        default:
            break;
    }
}

static enum nav_direction normalise_nav_direction(const enum nav_direction direction) {
    if (!swap_axis) return direction;

    switch (direction) {
        case nav_dir_up:
            return nav_dir_left;
        case nav_dir_down:
            return nav_dir_right;
        case nav_dir_left:
            return nav_dir_up;
        case nav_dir_right:
        default:
            return nav_dir_down;
    }
}

void set_nav_input_dir(const enum nav_direction direction) {
    nav_set_last_dir(normalise_nav_direction(direction));
}

void setup_nav(const struct nav_bar *nav_items) {
    for (size_t i = 0; nav_items[i].item != NULL; i++) {
        if (nav_items[i].ui_check && !ui_count_static) continue;

        if (nav_items[i].text && nav_items[i].text[0] != '\0') {
            lv_label_set_text(nav_items[i].item, nav_items[i].text);
        }

        lv_obj_clear_flag(nav_items[i].item, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_move_foreground(nav_items[i].item);
    }

    footer_nav_check_scroll();
}

void nav_show_a(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_a, text);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

void nav_show_lr(const int show) {
    if (show) {
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

void header_and_footer_setup(void) {
    hold_call_release();
    adjust_gen_panel();

    lv_obj_set_style_bg_opa(ui_pnl_header, theme.header.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_label_set_text(ui_lbl_preview_header, "");
    lv_obj_add_flag(ui_lbl_preview_header_glyph, LV_OBJ_FLAG_HIDDEN);

    process_visual_element(vis_clock, ui_lbl_datetime);
    process_visual_element(vis_bluetooth, ui_sta_bluetooth);
    process_visual_element(vis_network, ui_sta_network);
    process_visual_element(vis_battery, ui_sta_capacity);
    process_visual_element(vis_headertitle, ui_lbl_title);

    lv_label_set_text(ui_lbl_message, "");

    crash_ui_check(&theme, &lang, lv_layer_top(), &msgbox_active);
    power_loss_ui_check(&theme, &lang, lv_layer_top(), &msgbox_active);
}

void overlay_display(void) {
    watermark(ui_screen);

    if (kiosk.enable) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    load_overlay_image_sdl();
}

char *specify_asset(char *val, const char *def_val, const char *label) {
    if (!val || strlen(val) == 0 || strcasecmp(val, "(null)") == 0) {
        LOG_INFO(mux_module, "Using Default %s: %s", label, def_val);
        return strdup(def_val);
    }

    LOG_INFO(mux_module, "Assigned %s: %s", label, val);
    return val;
}

static char *load_content_asset(
    char *sys_dir, const char *pointer, int force, int run_quit, const char *ext, const char *label, int is_app
) {
    char path[MAX_BUFFER_SIZE];
    if (!sys_dir) return NULL;

    if (is_app) {
        snprintf(path, sizeof(path), "%s/mux_option.%s", sys_dir, ext);
        LOG_SUCCESS(mux_module, "Loading Application %s: %s", label, path);

        char *txt = read_all_char_from(path);
        if (txt) return txt;

        LOG_ERROR(mux_module, "Failed to Read Application %s", label);
        return NULL;
    }

    char rel_path[PATH_MAX];
    union_get_relative_path(sys_dir, rel_path, sizeof(rel_path));

    if (strncasecmp(rel_path, MAIN_ROM_DIR, 4) == 0) {
        char *p = rel_path + 4;
        while (*p == '/')
            p++;
        memmove(rel_path, p, strlen(p) + 1);
    }

    if (rel_path[0] == '\0') {
        snprintf(path, sizeof(path), INFO_CON_PATH "/core.%s", ext);
    } else {
        const char *name = pointer ? pointer : strip_ext(items[current_item_index].name);

        snprintf(path, sizeof(path), INFO_CON_PATH "/%s/%s.%s", rel_path, name, ext);
        remove_double_slashes(path);

        if (file_exist(path) && !force) {
            LOG_SUCCESS(mux_module, "Loading Content Asset %s: %s", label, path);

            char *txt = read_all_char_from(path);
            if (txt) return txt;

            LOG_ERROR(mux_module, "Failed to read Content Asset: %s", label);
        }

        snprintf(path, sizeof(path), INFO_CON_PATH "/%s/core.%s", rel_path, ext);
        remove_double_slashes(path);
    }

    if (file_exist(path) && !force) {
        LOG_SUCCESS(mux_module, "Loading Global %s: %s", label, path);

        char *txt = read_all_char_from(path);
        if (txt) return txt;

        LOG_ERROR(mux_module, "Failed to Read Global %s", label);
    }

    if (run_quit) mux_input_stop();

    LOG_INFO(mux_module, "No %s detected", label);
    return NULL;
}

char *load_content_governor(char *sys_dir, const char *pointer, const int force, const int run_quit, const int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "gov", "Governor", is_app);
}

char *
load_content_control_scheme(char *sys_dir, const char *pointer, const int force, const int run_quit, const int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "con", "Control Scheme", is_app);
}

char *
load_content_retroarch(char *sys_dir, const char *pointer, const int force, const int run_quit, const int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "rac", "RetroArch Config", is_app);
}

char *load_content_filter(char *sys_dir, const char *pointer, const int force, const int run_quit, const int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "flt", "Colour Filter", is_app);
}

char *load_content_shader(char *sys_dir, const char *pointer, const int force, const int run_quit, const int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "shd", "Shader", is_app);
}

void viewport_refresh(
    lv_obj_t **ui_viewport_objects, const char *artwork_config, char *catalogue_folder, char *content_name
) {
    mini_t *artwork_config_ini = mini_try_load(artwork_config);

    const int16_t viewport_width =
        get_ini_int(artwork_config_ini, "viewport", "WIDTH", (int16_t) (device.mux.width / 2));
    const int16_t viewport_height = get_ini_int(artwork_config_ini, "viewport", "HEIGHT", 400);
    const int16_t column_mode = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE", 0);
    const int16_t column_mode_alignment = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE_ALIGNMENT", 2);

    lv_obj_set_width(ui_viewport_objects[0], viewport_width == 0 ? LV_SIZE_CONTENT : viewport_width);
    lv_obj_set_height(ui_viewport_objects[0], viewport_height == 0 ? LV_SIZE_CONTENT : viewport_height);

    if (column_mode) {
        lv_obj_set_flex_flow(ui_viewport_objects[0], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(
            ui_viewport_objects[0], LV_FLEX_ALIGN_CENTER, column_mode_alignment, LV_FLEX_ALIGN_CENTER
        );
    }

    for (int index = 1; index < 6; index++) {
        char section_name[15];
        snprintf(section_name, sizeof(section_name), "image%d", index);
        char *folder_name = str_trim(get_ini_string(artwork_config_ini, section_name, "FOLDER", ""));

        char image[MAX_BUFFER_SIZE];
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png", INFO_CAT_PATH, catalogue_folder, folder_name, content_name);
        if (!file_exist(image))
            snprintf(image, sizeof(image), "%s/%s/%s/default.png", INFO_CAT_PATH, catalogue_folder, folder_name);

        const struct image_settings image_settings = {
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

int32_t get_directory_item_count(const char *base_dir, const char *dir_name, const int run_skip) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, dir_name);

    DIR *dir = opendir(full_path);
    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return 0;
    }

    struct dirent *entry;
    int32_t dir_count = 0;

    while ((entry = readdir(dir))) {
        if (run_skip) {
            if (entry->d_type == DT_DIR) {
                if (!should_skip(entry->d_name, 1)) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) dir_count++;
                }
            } else if (entry->d_type == DT_REG) {
                if (!should_skip(entry->d_name, 0)) {
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

void update_file_counter(lv_obj_t *counter, const int file_count) {
    if ((ui_count_static > 0 && !file_count && config.visual.menu_counter_folder)
        || (file_count > 0 && config.visual.menu_counter_file)) {
        char counter_text[MAX_BUFFER_SIZE];
        snprintf(
            counter_text, sizeof(counter_text), "%d%s%d", current_item_index + 1, theme.counter.text_separator,
            ui_count_static
        );
        counter_message(counter, counter_text, theme.counter.text_fade_time * 60);
    } else {
        lv_obj_add_flag(counter, LV_OBJ_FLAG_HIDDEN);
    }
}

char *get_friendly_folder_name(char *folder_name, const int fn_valid, const struct json fn_json) {
    char *friendly_folder_name = mux_malloc(MAX_BUFFER_SIZE);
    snprintf(friendly_folder_name, MAX_BUFFER_SIZE, "%s", folder_name);

    if (!config.visual.friendly_folder || !fn_valid) return friendly_folder_name;

    char *folder_lower = str_tolower(folder_name);
    const struct json good_name_json = json_object_get(fn_json, folder_lower);
    free(folder_lower);

    if (json_exists(good_name_json)) json_string_copy(good_name_json, friendly_folder_name, MAX_BUFFER_SIZE);

    return friendly_folder_name;
}

static int register_launch_item(const char *file_path, const char *item_name) {
    if (!file_exist(file_path)) return 0;

    char fn_name[MAX_BUFFER_SIZE];
    resolve_friendly_name(file_path, fn_name);
    add_item(&items, &item_count, item_name, fn_name, file_path, ITEM);

    return 1;
}

int folder_has_launch_file_with_extension(char *base_dir, char *dir_name, char *ext) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s.%s", base_dir, dir_name, dir_name, ext);

    char item_name[MAX_BUFFER_SIZE];
    snprintf(item_name, sizeof(item_name), "%s/%s.%s", dir_name, dir_name, ext);

    return register_launch_item(file_path, item_name);
}

int folder_has_matching_launch_file(char *base_dir, char *dir_name) {
    if (strchr(dir_name, '.') == NULL) {
        return 0;
    }

    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", base_dir, dir_name, dir_name);

    if (ends_with(dir_name, ".scummvm") && !file_exist(file_path)) {
        write_text_to_file(file_path, "w", CHAR, "");
    }

    char item_name[MAX_BUFFER_SIZE];
    snprintf(item_name, sizeof(item_name), "%s/%s", dir_name, dir_name);

    return register_launch_item(file_path, item_name);
}

int folder_has_launch_file(char *base_dir, char *dir_name) {
    if (folder_has_matching_launch_file(base_dir, dir_name)) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "scummvm")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "m3u")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "cue")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "gdi")) return 1;
    return 0;
}

void update_title(
    const char *folder_path, const int fn_valid, const struct json fn_json, const char *label, const char *module_path
) {
    char *display_title = get_friendly_folder_name(get_last_dir(folder_path), fn_valid, fn_json);
    adjust_visual_label(display_title, config.visual.name, config.visual.dash);

    char title[PATH_MAX];
    const char *module_type = "";

    if (!config.visual.title_include_root_drive) module_type = "";

    char *clean_folder_path = str_replace(folder_path, "/", "");
    char *clean_module_path = str_replace(module_path, "/", "");

    if (strcasecmp(clean_folder_path, clean_module_path) == 0 && label[0] != '\0') {
        snprintf(title, sizeof(title), "%s%s", label, module_type);
    } else {
        snprintf(title, sizeof(title), "%s%s", display_title, module_type);
    }

    lv_label_set_text(ui_lbl_title, title);

    free(display_title);
    free(clean_folder_path);
    free(clean_module_path);
}

void gen_label(const char *module, const char *item_glyph, const char *item_text) {
    lv_obj_t *ui_pnl_item = lv_obj_create(ui_pnl_content);
    lv_obj_t *ui_lbl_item = lv_label_create(ui_pnl_item);
    lv_obj_t *ui_lbl_item_glyph = lv_img_create(ui_pnl_item);

    lv_group_add_obj(ui_group, ui_lbl_item);
    lv_group_add_obj(ui_group_glyph, ui_lbl_item_glyph);
    lv_group_add_obj(ui_group_panel, ui_pnl_item);

    apply_theme_list_panel(ui_pnl_item);
    apply_theme_list_item(&theme, ui_lbl_item, item_text);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, module, item_glyph);

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, item_text);
    apply_text_long_dot(&theme, ui_lbl_item);
}

/* Talk about a confusing state, but here we go!
 * 0=Press A, 1=Hold A, 2=Load State, 3=Start Fresh
 */
int launch_flag(int mode, const int held) {
    static const uint8_t launch[4][2] = {
        {1, 0}, // 0 Press A
        {0, 1}, // 1 Hold A
        {0, 0}, // 2 Load State (always)
        {1, 1}  // 3 Start Fresh (always)
    };
    if ((unsigned) mode > 3) mode = 0;
    return launch[mode][held ? 0 : 1];
}

void reset_ui_groups(void) {
    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    lv_group_set_focus_cb(ui_group_panel, nav_focus_shake_cb);
    nav_suppress_next_shake();
}

void add_ui_groups(lv_obj_t **options, lv_obj_t **values, lv_obj_t **glyphs, lv_obj_t **panels, const int long_dot) {
    for (unsigned int i = 0; i < ui_count_static; i++) {
        lv_obj_t *opt = options ? options[i] : NULL;
        lv_obj_t *val = values ? values[i] : NULL;
        lv_obj_t *ico = glyphs ? glyphs[i] : NULL;
        lv_obj_t *pnl = panels ? panels[i] : NULL;

        if (opt) lv_group_add_obj(ui_group, opt);
        if (val) lv_group_add_obj(ui_group_value, val);
        if (ico) lv_group_add_obj(ui_group_glyph, ico);
        if (pnl) lv_group_add_obj(ui_group_panel, pnl);

        if (long_dot && pnl && opt) apply_text_long_dot(&theme, opt);
    }
}

void adjust_gen_panel(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_progress_brightness,
                                          ui_pnl_progress_volume, ui_pnl_progress, NULL});
    if (config.visual.box_art == 3) lv_obj_move_foreground(ui_pnl_box);
}

void ui_gen_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnl_content);

        nav_moved = 0;
    }
}

void gen_step_movement(
    const int steps, const int direction, const int long_dot, const int count_offset, const int sound
) {
    if (!ui_count_static) return;

    if (first_open) {
        first_open = 0;
    } else if (!nav_silent) {
        if (sound) play_sound(snd_navigate);
    }

    for (int step = 0; step < steps; ++step) {
        if (long_dot == 1)
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group));
        else if (long_dot == 2) {
            apply_option_label_long_dot(lv_group_get_focused(ui_group));
            apply_option_value_long_dot(lv_group_get_focused(ui_group_value));
        }

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(
        theme.mux.item.count + count_offset, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
    if (long_dot == 1) {
        lv_obj_update_layout(ui_pnl_content);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    } else if (long_dot == 2) {
        set_option_label_scroll_mode(lv_group_get_focused(ui_group));
        set_option_value_scroll_mode(lv_group_get_focused(ui_group_value));
    }

    nav_moved = 1;
}

void list_nav_cb_prev(const int steps) {
    gen_step_movement(steps, -1, 1, 0, 1);
}

void list_nav_cb_next(const int steps) {
    gen_step_movement(steps, +1, 1, 0, 1);
}

void list_nav_cb_prev_nowrap(const int steps) {
    gen_step_movement(steps, -1, 2, 0, 1);
}

void list_nav_cb_next_nowrap(const int steps) {
    gen_step_movement(steps, +1, 2, 0, 1);
}

void handle_msgbox_dismiss(void) {
    msgbox_active = 0;
    progress_onscreen = 0;

    if (crash_ui_dismiss()) {
        play_sound(snd_confirm);
        return;
    }

    if (power_loss_ui_dismiss()) {
        play_sound(snd_confirm);
        return;
    }

    play_sound(snd_info_close);
    if (msgbox_element == ui_pnl_help) {
        hide_info_box();
    } else {
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
    }
}

int build_safe_path(char *dst, const size_t n, const char *base, const char *name) {
    const int len = snprintf(dst, n, "%s/%s", base, name);
    if (len < 0 || (size_t) len >= n) return -1;

    char resolved[PATH_MAX];
    if (!realpath(dst, resolved)) return -1;

    const size_t base_len = strlen(base);
    if (strncmp(resolved, base, base_len) != 0) return -1;
    if (resolved[base_len] != '/' && resolved[base_len] != '\0') return -1;

    snprintf(dst, n, "%s", resolved);
    return 0;
}

typedef struct {
    char *key;
    char *val;
} fn_entry_t;

static fn_entry_t *fn_name_table = NULL;
static int fn_name_table_count = 0;

static int fn_entry_cmp(const void *a, const void *b) {
    return strcmp(((const fn_entry_t *) a)->key, ((const fn_entry_t *) b)->key);
}

static void fn_build_table(const struct json root) {
    for (int i = 0; i < fn_name_table_count; i++) {
        free(fn_name_table[i].key);
        free(fn_name_table[i].val);
    }

    free(fn_name_table);
    fn_name_table = NULL;
    fn_name_table_count = 0;

    int cap = 64;
    int count = 0;

    fn_entry_t *tbl = malloc((size_t) cap * sizeof(fn_entry_t));
    if (!tbl) return;

    struct json key = json_first(root);
    while (json_exists(key)) {
        const struct json val = json_next(key);

        char kbuf[MAX_BUFFER_SIZE], vbuf[MAX_BUFFER_SIZE];
        json_string_copy(key, kbuf, sizeof(kbuf));
        json_string_copy(val, vbuf, sizeof(vbuf));

        if (count >= cap) {
            cap *= 2;
            fn_entry_t *tmp = realloc(tbl, (size_t) cap * sizeof(fn_entry_t));
            if (!tmp) break;
            tbl = tmp;
        }

        tbl[count].key = strdup(kbuf);
        tbl[count].val = strdup(vbuf);
        count++;

        key = json_next(val);
    }

    if (count > 1) qsort(tbl, (size_t) count, sizeof(fn_entry_t), fn_entry_cmp);

    fn_name_table = tbl;
    fn_name_table_count = count;
}

static void strip_and_lower_basename(const char *file_path, char *stripped_out, char *lowered_out) {
    const char *base = strrchr(file_path, '/');

    base = base ? base + 1 : file_path;
    snprintf(stripped_out, MAX_BUFFER_SIZE, "%s", base);

    char *dot = strrchr(stripped_out, '.');
    if (dot) *dot = '\0';

    size_t idx = 0;
    for (; stripped_out[idx]; idx++)
        lowered_out[idx] = (char) tolower((unsigned char) stripped_out[idx]);
    lowered_out[idx] = '\0';
}

static const char *resolve_name_json_for_parent(const char *file_path) {
    static char cached_file_parent[PATH_MAX];
    static char cached_name_only[MAX_BUFFER_SIZE];

    const char *slash = strrchr(file_path, '/');
    const size_t parent_len = slash ? (size_t) (slash - file_path) : 0;

    if (strncmp(file_path, cached_file_parent, parent_len) != 0 || cached_file_parent[parent_len] != '\0') {

        if (parent_len < sizeof(cached_file_parent)) {
            memcpy(cached_file_parent, file_path, parent_len);
            cached_file_parent[parent_len] = '\0';
        } else {
            cached_file_parent[0] = '\0';
        }

        const char *last = strrchr(cached_file_parent, '/');
        snprintf(cached_name_only, sizeof(cached_name_only), "%s", last ? last + 1 : cached_file_parent);
    }

    char specific_rel[MAX_BUFFER_SIZE];
    snprintf(specific_rel, sizeof(specific_rel), "name/%s.json", cached_name_only);

    static char last_specific_rel[MAX_BUFFER_SIZE];
    static const char *last_lookup_path = NULL;

    if (strcmp(last_specific_rel, specific_rel) != 0) {
        last_lookup_path = resolve_info_path(specific_rel);
        if (!last_lookup_path) last_lookup_path = resolve_info_path("name/global.json");
        snprintf(last_specific_rel, sizeof(last_specific_rel), "%s", specific_rel);
    }

    return last_lookup_path;
}

static int lookup_custom_name(const char *lookup_path, const char *lowered, char *out) {
    static char cache_path[PATH_MAX];
    static char *cache_str = NULL;
    static struct json cache_root;
    static int cache_valid = 0;

    if (!lookup_path) return 0;

    if (strcmp(cache_path, lookup_path) != 0) {
        free(cache_str);
        cache_str = read_all_char_from(lookup_path);
        cache_valid = cache_str != NULL && json_valid(cache_str);

        if (cache_valid) {
            cache_root = json_parse(cache_str);
            fn_build_table(cache_root);
        } else {
            fn_build_table((struct json) {0});
        }

        snprintf(cache_path, sizeof(cache_path), "%s", lookup_path);
    }

    if (!cache_valid || fn_name_table_count == 0) return 0;

    const fn_entry_t needle = {(char *) lowered, NULL};
    const fn_entry_t *found =
        bsearch(&needle, fn_name_table, (size_t) fn_name_table_count, sizeof(fn_entry_t), fn_entry_cmp);

    if (!found) return 0;

    snprintf(out, MAX_BUFFER_SIZE, "%s", found->val);
    return 1;
}

void resolve_friendly_name(const char *file_path, char *out) {
    char stripped[MAX_BUFFER_SIZE];
    char lowered[MAX_BUFFER_SIZE];
    strip_and_lower_basename(file_path, stripped, lowered);

    const char *lookup_path = resolve_name_json_for_parent(file_path);

    if (!lookup_custom_name(lookup_path, lowered, out)) {
        const char *lk = lookup(stripped);
        snprintf(out, MAX_BUFFER_SIZE, "%s", lk ? lk : stripped);
    }
}

void gen_item_from_files(const char *base_path, const int file_count, char **file_names) {
    char *created_dirs[file_count];
    int created_count = 0;

    for (int i = 0; i < file_count; i++) {
        char fn_name[MAX_BUFFER_SIZE];

        char item_file[MAX_BUFFER_SIZE];
        snprintf(item_file, sizeof(item_file), "%s/%s", base_path, file_names[i]);

        union_rewrite_file_paths(item_file);
        char *file_path_raw = read_line_char_from(item_file, cache_core_path);

        char resolved_path[PATH_MAX];
        if (union_resolve_to_real(file_path_raw, resolved_path, sizeof(resolved_path))) {
            free(file_path_raw);
            file_path_raw = strdup(resolved_path);
        }

        const char *file_path = file_path_raw;
        char *sub_path = read_line_char_from(item_file, cache_core_dir);

        int already_created = 0;
        for (int c = 0; c < created_count; c++) {
            if (strcasecmp(created_dirs[c], sub_path) == 0) {
                already_created = 1;
                break;
            }
        }

        if (!already_created) {
            char init_meta_dir[MAX_BUFFER_SIZE];
            snprintf(init_meta_dir, sizeof(init_meta_dir), INFO_CON_PATH "/%s/", sub_path);
            create_directories(init_meta_dir, 0);

            created_dirs[created_count++] = strdup(sub_path);
        }

        resolve_friendly_name(file_path, fn_name);
        add_item(&items, &item_count, file_names[i], fn_name, file_path, ITEM);

        ui_count_static++;
        free(file_names[i]);
    }

    for (int c = 0; c < created_count; c++) {
        free(created_dirs[c]);
    }
}

void adjust_label_value_width(const lv_obj_t *panel, const lv_obj_t *label, lv_obj_t *value) {
    lv_obj_update_layout(panel);

    const lv_coord_t panel_width = lv_obj_get_width(panel);
    if (panel_width <= 0) return;

    const char *label_text = lv_label_get_text(label);
    if (!label_text) return;

    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    const lv_coord_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);

    lv_point_t text_width;
    lv_txt_get_size(&text_width, label_text, font, letter_space, line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const lv_coord_t label_text_width = text_width.x;

    // Okay 64 leaves a good gap for both long static and long animated
    const lv_coord_t available = panel_width - label_text_width - theme.font.list_pad_left - 64;

    lv_obj_set_width(value, available);
}

void update_label_scroll() {
    if (lv_group_get_focused(ui_group_value)) {
        set_label_long_mode(&theme, lv_group_get_focused(ui_group_value), config.visual.name_scroll);
    }
}

static void resolve_none_box_fallback(char *out) {
    snprintf(out, MAX_BUFFER_SIZE, "%s/%simage/none_box.png", theme_base, mux_dim);
    if (!file_exist(out)) {
        snprintf(out, MAX_BUFFER_SIZE, "%s/image/none_box.png", theme_base);
    }
}

static void apply_box_blank_or_fallback(lv_obj_t *ui_viewport_objects[], int *starter_image) {
    char none_box[MAX_BUFFER_SIZE] = {0};
    if (config.visual.box_art_placeholder) resolve_none_box_fallback(none_box);

    if (strcasecmp(box_image_previous_path, none_box) == 0) return;

    if (none_box[0] && file_exist(none_box)) {
        *starter_image = 1;

        char image_path[MAX_BUFFER_SIZE];
        snprintf(image_path, sizeof(image_path), "M:%s", none_box);

        lv_img_set_src(ui_img_box, image_path);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", none_box);
    } else {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        if (ui_viewport_objects) {
            for (int i = 1; i < 6; i++) {
                if (ui_viewport_objects[i]) lv_img_set_src(ui_viewport_objects[i], &ui_img_blank);
            }
        }
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
    }
}

static int refresh_cached_image(char *previous_path, char *image, const int is_loaded) {
    if (strcasecmp(previous_path, image) == 0) return 0;

    snprintf(previous_path, MAX_BUFFER_SIZE, "%s", is_loaded ? image : " ");
    return 1;
}

static void render_preview_refresh(char *image) {
    const int is_loaded = file_exist(image);
    if (!refresh_cached_image(preview_image_previous_path, image, is_loaded)) {
        return;
    }

    if (!is_loaded) {
        lv_img_set_src(ui_img_help_preview_image, &ui_img_blank);
        return;
    }

    const struct image_settings image_settings = {
        image,
        LV_ALIGN_CENTER,
        validate_int16((int16_t) (device.mux.width * .9) - 60, "width"),
        validate_int16((int16_t) (device.mux.height * .9) - 120, "height"),
        0,
        0,
        0,
        0
    };

    update_image(ui_img_help_preview_image, image_settings);
}

static void render_splash_refresh(lv_obj_t *ui_img_splash, char *image, int *splash_valid) {
    const int is_loaded = file_exist(image);
    if (!refresh_cached_image(splash_image_previous_path, image, is_loaded)) {
        return;
    }

    *splash_valid = is_loaded;

    if (!is_loaded) {
        lv_img_set_src(ui_img_splash, &ui_img_blank);
        return;
    }

    char image_path[MAX_BUFFER_SIZE];
    snprintf(image_path, sizeof(image_path), "M:%s", image);
    lv_img_set_src(ui_img_splash, image_path);
}

static void render_box_refresh(
    char *image, char *h_core_artwork, char *h_file_name, lv_obj_t *ui_viewport_objects[], int *starter_image
) {
    if (strcasecmp(box_image_previous_path, image) == 0) return;

    char artwork_config_path[MAX_BUFFER_SIZE];
    snprintf(artwork_config_path, sizeof(artwork_config_path), INFO_CAT_PATH "/%s.ini", h_core_artwork);

    if (!file_exist(artwork_config_path))
        snprintf(artwork_config_path, sizeof(artwork_config_path), INFO_CAT_PATH "/default.ini");

    if (file_exist(artwork_config_path)) {
        viewport_refresh(ui_viewport_objects, artwork_config_path, h_core_artwork, h_file_name);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
        return;
    }

    if (!file_exist(image)) {
        apply_box_blank_or_fallback(NULL, starter_image);
        return;
    }

    *starter_image = 1;

    const int box_w = device.mux.width;
    const int fullscreen = config.visual.box_art == 2 || config.visual.box_art == 3;

    int box_h = fullscreen ? device.mux.height : device.mux.height - theme.header.height - theme.footer.height - 4;
    if (box_h <= 0) box_h = device.mux.height;

    const int16_t max_w = (int16_t) (config.visual.box_art_scale > 0 ? box_w * config.visual.box_art_scale / 100 : 0);

    const int explicit_align = config.visual.box_art_align > 0;
    static const int pad_div_map[] = {50, 100, 200, 400, 600, 800};

    const int pad_div_idx = config.settings.advanced.box_art_pad_div;
    const int pad_div = pad_div_idx >= 0 && pad_div_idx < 6 ? pad_div_map[pad_div_idx] : 400;

    const int user_pad = config.visual.box_art_padding > 0 ? box_h * config.visual.box_art_padding / pad_div : 0;

    const int pad_l = (explicit_align ? 0 : theme.image_list.pad_left) + user_pad;
    const int pad_r = (explicit_align ? 0 : theme.image_list.pad_right) + user_pad;
    const int pad_t = (explicit_align ? 0 : theme.image_list.pad_top) + user_pad;
    const int pad_b = (explicit_align ? 0 : theme.image_list.pad_bottom) + user_pad;

    char image_path[MAX_BUFFER_SIZE];
    const size_t ilen = strlen(image);
    if (ilen > 4 && strcmp(image + ilen - 4, ".svg") == 0) {
        const int svg_w = max_w > 0 ? max_w : box_w;
        snprintf(image_path, sizeof(image_path), "M:%s?%dx%d", image, svg_w, box_h);

        lv_img_set_size_mode(ui_img_box, LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_zoom(ui_img_box, LV_IMG_ZOOM_NONE);

        lv_obj_set_style_pad_left(ui_img_box, pad_l, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(ui_img_box, pad_r, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(ui_img_box, pad_t, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_img_box, pad_b, MU_OBJ_MAIN_DEFAULT);

        lv_img_set_src(ui_img_box, image_path);
    } else {
        const struct image_settings image_settings = {image, -1, max_w, (int16_t) box_h, pad_l, pad_r, pad_t, pad_b};
        update_image(ui_img_box, image_settings);
    }

    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
}

void render_image_refresh(
    const char *image_type, char *h_core_artwork, char *h_file_name, lv_obj_t *ui_img_splash,
    lv_obj_t *ui_viewport_objects[], int *starter_image, int *splash_valid
) {
    if (strlen(h_core_artwork) <= 1) {
        if (strcasecmp(image_type, "preview") == 0) {
            lv_img_set_src(ui_img_help_preview_image, &ui_img_blank);
            snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), " ");
        } else if (strcasecmp(image_type, "splash") == 0) {
            *splash_valid = 0;
            lv_img_set_src(ui_img_splash, &ui_img_blank);
            snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), " ");
        } else {
            apply_box_blank_or_fallback(ui_viewport_objects, starter_image);
        }
        return;
    }

    char image[MAX_BUFFER_SIZE] = {0};

    if (strcasecmp(image_type, "box") != 0 || !grid_mode_enabled || !config.visual.box_art_hide) {
        load_image_catalogue(h_core_artwork, h_file_name, "", "default", mux_dim, image_type, image, sizeof(image));
        LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);
    }

    if (strcasecmp(image_type, "splash") == 0 && !file_exist(image)) {
        load_splash_image_fallback(mux_dim, image, sizeof(image));
        LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);
    }

    if (strcasecmp(image_type, "preview") == 0) {
        render_preview_refresh(image);
    } else if (strcasecmp(image_type, "splash") == 0) {
        render_splash_refresh(ui_img_splash, image, splash_valid);
    } else {
        render_box_refresh(image, h_core_artwork, h_file_name, ui_viewport_objects, starter_image);
    }
}

void clear_box_image() {
    lv_img_set_src(ui_img_box, &ui_img_blank);
    snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
}

void render_video_refresh(const char *h_core_artwork, const char *h_file_name) {
    char vpath[MAX_BUFFER_SIZE];
    if (!load_video_catalogue(h_core_artwork, h_file_name, h_file_name, mux_dim, vpath, sizeof(vpath))) return;

    const int delay_ms = config.visual.video_preview == 3 ? 10000 : config.visual.video_preview == 2 ? 5000 : 3000;
    video_preview_arm(vpath, delay_ms, ui_pnl_box, ui_img_box);
}

static void write_marker_file(const char *path, const char *value, const char *op_label, const char *op_suffix) {
    if (strcasecmp(value, "none") == 0) return;

    FILE *file = fopen(path, "w");
    if (!file) {
        LOG_ERROR(mux_module, "%s: %s", lang.system.fail_file_open, path);
        return;
    }

    LOG_INFO(mux_module, "%s (%s): %s", op_label, op_suffix, value);

    fprintf(file, "%s", value);
    fclose(file);
}

static void
assign_marker_single(const char *core_dir, const char *ext, const char *op_label, const char *value, const char *rom) {
    char marker_path[MAX_BUFFER_SIZE];
    char *rom_no_ext = strip_ext(rom);

    snprintf(marker_path, sizeof(marker_path), "%s/%s.%s", core_dir, rom_no_ext, ext);
    free(rom_no_ext);

    if (file_exist(marker_path)) remove(marker_path);
    write_marker_file(marker_path, value, op_label, "Single");
}

static void assign_marker_directory(
    const char *core_dir, char *rom_dir, const char *ext, const char *op_label, const char *value, const int purge
) {
    if (purge) {
        char dot_ext[16];
        snprintf(dot_ext, sizeof(dot_ext), ".%s", ext);
        delete_files_of_type(core_dir, dot_ext, NULL, 0);
    }

    char marker_path[MAX_BUFFER_SIZE];
    snprintf(marker_path, sizeof(marker_path), INFO_CON_PATH "/%s/core.%s", get_last_subdir(rom_dir, '/', 4), ext);
    remove_double_slashes(marker_path);

    write_marker_file(marker_path, value, op_label, "Directory");
}

static void
assign_marker_parent(char *core_dir, char *rom_dir, const char *ext, const char *op_label, const char *value) {
    char dot_ext[16];
    snprintf(dot_ext, sizeof(dot_ext), ".%s", ext);
    delete_files_of_type(core_dir, dot_ext, NULL, 1);

    if (strcasecmp(value, "none") == 0) return;

    assign_marker_directory(core_dir, rom_dir, ext, op_label, value, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char marker_path[MAX_BUFFER_SIZE];
        snprintf(marker_path, sizeof(marker_path), "%s%s/core.%s", core_dir, subdirs[i], ext);

        char *marker_parent = strip_dir(marker_path);
        create_directories(marker_parent, 0);
        free(marker_parent);

        write_marker_file(marker_path, value, op_label, "Recursive");
    }

    free_subdirectories(subdirs);
}

void create_marker_assignment(
    const char *ext, const char *op_label, const char *value, const char *rom, char *rom_dir, const int is_app,
    const enum gen_type method
) {
    char core_dir[MAX_BUFFER_SIZE];

    if (is_app) {
        snprintf(core_dir, sizeof(core_dir), "%s/", rom_dir);
    } else {
        snprintf(core_dir, sizeof(core_dir), INFO_CON_PATH "/%s/", get_last_subdir(rom_dir, '/', 4));
    }

    remove_double_slashes(core_dir);
    create_directories(core_dir, 0);

    switch (method) {
        case casn_single:
            assign_marker_single(core_dir, ext, op_label, value, rom);
            break;
        case casn_parent:
            assign_marker_parent(core_dir, rom_dir, ext, op_label, value);
            break;
        case casn_dir:
            assign_marker_directory(core_dir, rom_dir, ext, op_label, value, 1);
            break;
        case casn_dir_nowipe:
        default:
            assign_marker_directory(core_dir, rom_dir, ext, op_label, value, 0);
            break;
    }
}

void net_trim(char *value) {
    if (!value) return;

    const char *start = value;
    while (*start && isspace((unsigned char) *start))
        start++;

    if (start != value) memmove(value, start, strlen(start) + 1);

    size_t len = strlen(value);
    while (len > 0 && isspace((unsigned char) value[len - 1])) {
        value[--len] = '\0';
    }
}

int read_wpa_status_value(const char *key, char *value) {
    if (!*key) return 0;

    value[0] = '\0';

    FILE *fp = popen("wpa_cli status 2>/dev/null", "r");
    if (!fp) return 0;

    char line[MAX_BUFFER_SIZE];
    const size_t key_len = strlen(key);
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        net_trim(line);

        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            snprintf(value, MAX_BUFFER_SIZE, "%s", line + key_len + 1);
            net_trim(value);
            found = *value;
            break;
        }
    }

    pclose(fp);
    return found;
}

int read_connected_ssid(char *ssid) {
    ssid[0] = '\0';

    char state[MAX_BUFFER_SIZE];
    if (!read_wpa_status_value("wpa_state", state)) return 0;
    if (strcmp(state, "COMPLETED") != 0) return 0;

    return read_wpa_status_value("ssid", ssid);
}

int profile_matches_connected_ssid(const char *profile_name, const char *ssid) {
    if (!*profile_name || !*ssid) return 0;

    if (strcmp(profile_name, ssid) == 0) return 1;

    char profile_file[MAX_BUFFER_SIZE];
    const int pf_len = snprintf(profile_file, sizeof(profile_file), STORAGE_NETWORK "/%s.ini", profile_name);
    if (pf_len < 0 || (size_t) pf_len >= sizeof(profile_file)) return 0;

    mini_t *net = mini_try_load(profile_file);
    const char *profile_ssid = mini_get_string(net, "network", "ssid", "");
    const int match = profile_ssid && *profile_ssid && strcmp(profile_ssid, ssid) == 0;
    mini_free(net);

    return match;
}

void resolve_content_artwork_names(
    char *h_core_artwork, const size_t core_size, char *h_file_name, const size_t file_size
) {
    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    get_catalogue_name(item_dir, item_file_name_buf, h_core_artwork, core_size);

    snprintf(h_file_name, file_size, "%s", item_file_name_buf);
    char *dot = strrchr(h_file_name, '.');
    if (dot) *dot = '\0';
}

void refresh_theme_preview_image(char *base_path, char *name, int *preview_index) {
    char preview_path[MAX_BUFFER_SIZE];
    if (get_theme_preview_path(base_path, name, preview_path, sizeof(preview_path), *preview_index) != 0) {
        *preview_index = -1;
    }

    lv_img_cache_invalidate_src(lv_img_get_src(ui_img_box));

    if (strcasecmp(box_image_previous_path, preview_path) != 0) {
        if (!file_exist(preview_path)) {
            lv_img_set_src(ui_img_box, &ui_img_blank);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
        } else {
            const struct image_settings image_settings = {
                preview_path,
                6,
                validate_int16((int16_t) (device.mux.width * .45), "width"),
                validate_int16(device.mux.height, "height"),
                theme.image_list.pad_left,
                theme.image_list.pad_right,
                theme.image_list.pad_top,
                theme.image_list.pad_bottom
            };
            update_image(ui_img_box, image_settings);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", preview_path);
        }
    }
}

void resolve_grid_item_images(
    const char *mux_dim, const char *mux_module, const char *glyph_name, char *grid_img, const size_t img_size,
    char *grid_img_foc, const size_t foc_size
) {
    if (!load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name, "default", "svg", grid_img, img_size)) {
        load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name, "default", "png", grid_img, img_size);
    }

    char glyph_name_focused[MAX_BUFFER_SIZE];
    snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_name);

    grid_img_foc[0] = '\0';
    if (!load_element_image_specifics(
            mux_dim, mux_module, "grid", glyph_name_focused, "default_focused", "svg", grid_img_foc, foc_size
        )) {
        load_element_image_specifics(
            mux_dim, mux_module, "grid", glyph_name_focused, "default_focused", "png", grid_img_foc, foc_size
        );
    }
}

char *read_shader_info(const char *shader_store, const char *key) {
    if (!shader_store || !*shader_store || !*key) return NULL;

    char shader_path[PATH_MAX];
    snprintf(shader_path, sizeof(shader_path), "%s/%s.frag", STORAGE_SHADER, shader_store);
    remove_double_slashes(shader_path);

    FILE *f = fopen(shader_path, "r");
    if (!f) return NULL;

    char line[MAX_BUFFER_SIZE];

    while (fgets(line, sizeof(line), f)) {
        char *s = str_trim(line);
        if (!s || !*s) continue;

        if (strncmp(s, "//", 2) != 0) break;

        s += 2;
        s = str_trim(s);
        if (!s || !*s) continue;

        char *colon = strchr(s, ':');
        if (!colon) continue;

        *colon = '\0';

        char *k = str_trim(s);
        char *v = str_trim(colon + 1);

        if (!k || !v || !*v) continue;

        if (strcasecmp(k, key) == 0) {
            char *out = strdup(v);
            fclose(f);
            return out;
        }
    }

    fclose(f);
    return NULL;
}
