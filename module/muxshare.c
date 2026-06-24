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
int ui_count = 0;

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

int is_ksk(int k) {
    return kiosk.ENABLE ? k : 0;
}

void hold_call_set(void) {
    hold_call = 1;
}

void hold_call_release(void) {
    hold_call = 0;
}

void run_tweak_script(char *message) {
    toast_message(message, FOREVER);

    const char *args[] = {OPT_PATH "script/mux/tweak.sh", NULL};
    run_exec(args, A_SIZE(args), 0, 0, NULL, NULL);

    refresh_config = 1;
    refresh_device = 1;
}

void shuffle_index(int current, int *dir, int *target) {
    // We'll check this here again, better to be safe than sorry!
    if (ui_count < 2) {
        *target = current;
        *dir = +1;
        return;
    }

    int t;
    for (int i = 0; i < 4; i++) {
        // Pick a random index, at least 3 times, excluding the current one
        // Probably wasteful but it no longer deadlocks!
        int ran = (int) (random() % (long) (ui_count - 1));
        t = (ran >= current) ? ran + 1 : ran;

        // Avoid immediately repeating the last shuffled item if possible...
        if (t != last_shuffle) break;
    }

    last_shuffle = t;
    *target = t;
    *dir = (t > current) ? +1 : -1;
}

void adjust_box_art(void) {
    switch (config.VISUAL.BOX_ART) {
        case 0: // Behind
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 1: // Front
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 2: // Fullscreen + Behind
            lv_obj_set_y(ui_pnlBox, 0);
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 3: // Fullscreen + Front
            lv_obj_set_y(ui_pnlBox, 0);
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 4: // Disabled
            lv_obj_add_flag(ui_pnlBox, MU_OBJ_FLAG_HIDE_FLOAT);
            break;
    }
}

void setup_nav(struct nav_bar *nav_items) {
    for (size_t i = 0; nav_items[i].item != NULL; i++) {
        if (nav_items[i].ui_check && !ui_count) continue;

        if (nav_items[i].text && nav_items[i].text[0] != '\0') {
            lv_label_set_text(nav_items[i].item, nav_items[i].text);
        }

        lv_obj_clear_flag(nav_items[i].item, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_move_foreground(nav_items[i].item);
    }

    footer_nav_check_scroll();
}

void header_and_footer_setup(void) {
    hold_call_release();
    adjust_gen_panel();

    lv_obj_set_style_bg_opa(ui_pnlHeader, theme.HEADER.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlFooter, theme.FOOTER.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_obj_add_flag(ui_lblPreviewHeaderGlyph, LV_OBJ_FLAG_HIDDEN);

    process_visual_element(VIS_CLOCK, ui_lblDatetime);
    process_visual_element(VIS_BLUETOOTH, ui_staBluetooth);
    process_visual_element(VIS_NETWORK, ui_staNetwork);
    process_visual_element(VIS_BATTERY, ui_staCapacity);
    process_visual_element(VIS_HEADERTITLE, ui_lblTitle);

    lv_label_set_text(ui_lblMessage, "");

    crash_ui_check(&theme, &lang, lv_layer_top(), &msgbox_active);
    power_loss_ui_check(&theme, &lang, lv_layer_top(), &msgbox_active);
}

void overlay_display(void) {
    watermark(ui_screen);

    if (kiosk.ENABLE) {
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

static char *load_content_asset(char *sys_dir, const char *pointer, int force, int run_quit,
                                const char *ext, const char *label, int is_app) {
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
        while (*p == '/') p++;
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

char *load_content_governor(char *sys_dir, const char *pointer, int force, int run_quit, int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "gov", "Governor", is_app);
}

char *load_content_control_scheme(char *sys_dir, const char *pointer, int force, int run_quit, int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "con", "Control Scheme", is_app);
}

char *load_content_retroarch(char *sys_dir, const char *pointer, int force, int run_quit, int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "rac", "RetroArch Config", is_app);
}

char *load_content_filter(char *sys_dir, const char *pointer, int force, int run_quit, int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "flt", "Colour Filter", is_app);
}

char *load_content_shader(char *sys_dir, const char *pointer, int force, int run_quit, int is_app) {
    return load_content_asset(sys_dir, pointer, force, run_quit, "shd", "Shader", is_app);
}

void viewport_refresh(lv_obj_t **ui_viewport_objects, char *artwork_config, char *catalogue_folder, char *content_name) {
    mini_t *artwork_config_ini = mini_try_load(artwork_config);

    int16_t viewport_width = get_ini_int(artwork_config_ini, "viewport", "WIDTH", (int16_t) (device.MUX.WIDTH / 2));
    int16_t viewport_height = get_ini_int(artwork_config_ini, "viewport", "HEIGHT", 400);
    int16_t column_mode = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE", 0);
    int16_t column_mode_alignment = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE_ALIGNMENT", 2);

    lv_obj_set_width(ui_viewport_objects[0], viewport_width == 0 ? LV_SIZE_CONTENT : viewport_width);
    lv_obj_set_height(ui_viewport_objects[0], viewport_height == 0 ? LV_SIZE_CONTENT : viewport_height);

    if (column_mode) {
        lv_obj_set_flex_flow(ui_viewport_objects[0], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_viewport_objects[0], LV_FLEX_ALIGN_CENTER, column_mode_alignment, LV_FLEX_ALIGN_CENTER);
    }

    for (int index = 1; index < 6; index++) {
        char section_name[15];
        snprintf(section_name, sizeof(section_name), "image%d", index);
        char *folder_name = str_trim(get_ini_string(artwork_config_ini, section_name, "FOLDER", ""));

        char image[MAX_BUFFER_SIZE];
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png", INFO_CAT_PATH, catalogue_folder, folder_name, content_name);
        if (!file_exist(image)) snprintf(image, sizeof(image), "%s/%s/%s/default.png", INFO_CAT_PATH, catalogue_folder, folder_name);

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
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN);
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

void update_file_counter(lv_obj_t *counter, int file_count) {
    if ((ui_count > 0 && !file_count && config.VISUAL.MENUCOUNTERFOLDER) ||
        (file_count > 0 && config.VISUAL.MENUCOUNTERFILE)) {
        char counter_text[MAX_BUFFER_SIZE];
        snprintf(counter_text, sizeof(counter_text), "%d%s%d",
                 current_item_index + 1, theme.COUNTER.TEXT_SEPARATOR, ui_count);
        counter_message(counter, counter_text, theme.COUNTER.TEXT_FADE_TIME * 60);
    } else {
        lv_obj_add_flag(counter, LV_OBJ_FLAG_HIDDEN);
    }
}

char *get_friendly_folder_name(char *folder_name, int fn_valid, struct json fn_json) {
    char *friendly_folder_name = mux_malloc(MAX_BUFFER_SIZE);
    snprintf(friendly_folder_name, MAX_BUFFER_SIZE, "%s", folder_name);

    if (!config.VISUAL.FRIENDLYFOLDER || !fn_valid) return friendly_folder_name;

    char *folder_lower = str_tolower(folder_name);
    struct json good_name_json = json_object_get(fn_json, folder_lower);
    free(folder_lower);

    if (json_exists(good_name_json)) json_string_copy(good_name_json, friendly_folder_name, MAX_BUFFER_SIZE);

    return friendly_folder_name;
}

int folder_has_launch_file_with_extension(char *base_dir, char *dir_name, char *ext) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s.%s", base_dir, dir_name, dir_name, ext);
    if (file_exist(file_path)) {
        char item_name[MAX_BUFFER_SIZE];
        snprintf(item_name, sizeof(item_name), "%s/%s.%s", dir_name, dir_name, ext);
        char fn_name[MAX_BUFFER_SIZE];
        resolve_friendly_name(file_path, fn_name);
        add_item(&items, &item_count, item_name, fn_name, file_path, ITEM);
        return 1;
    }
    return 0;
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
    if (file_exist(file_path)) {
        char item_name[MAX_BUFFER_SIZE];
        snprintf(item_name, sizeof(item_name), "%s/%s", dir_name, dir_name);
        char fn_name[MAX_BUFFER_SIZE];
        resolve_friendly_name(file_path, fn_name);
        add_item(&items, &item_count, item_name, fn_name, file_path, ITEM);
        return 1;
    }
    return 0;
}

int folder_has_launch_file(char *base_dir, char *dir_name) {
    if (folder_has_matching_launch_file(base_dir, dir_name)) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "scummvm")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "m3u")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "cue")) return 1;
    if (folder_has_launch_file_with_extension(base_dir, dir_name, "gdi")) return 1;
    return 0;
}

void update_title(char *folder_path, int fn_valid, struct json fn_json, const char *label, const char *module_path) {
    char *display_title = get_friendly_folder_name(get_last_dir(folder_path), fn_valid, fn_json);
    adjust_visual_label(display_title, config.VISUAL.NAME, config.VISUAL.DASH);

    char title[PATH_MAX];
    const char *module_type = "";

    if (!config.VISUAL.TITLEINCLUDEROOTDRIVE) module_type = "";

    char *clean_folder_path = str_replace(folder_path, "/", "");
    char *clean_module_path = str_replace(module_path, "/", "");

    if (strcasecmp(clean_folder_path, clean_module_path) == 0 && label[0] != '\0') {
        snprintf(title, sizeof(title), "%s%s", label, module_type);
    } else {
        snprintf(title, sizeof(title), "%s%s", display_title, module_type);
    }

    lv_label_set_text(ui_lblTitle, title);

    free(display_title);
    free(clean_folder_path);
    free(clean_module_path);
}

void gen_label(char *module, char *item_glyph, char *item_text) {
    lv_obj_t *ui_pnlItem = lv_obj_create(ui_pnlContent);
    lv_obj_t *ui_lblItem = lv_label_create(ui_pnlItem);
    lv_obj_t *ui_lblItemGlyph = lv_img_create(ui_pnlItem);

    lv_group_add_obj(ui_group, ui_lblItem);
    lv_group_add_obj(ui_group_glyph, ui_lblItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlItem);

    apply_theme_list_panel(ui_pnlItem);
    apply_theme_list_item(&theme, ui_lblItem, item_text);
    apply_theme_list_glyph(&theme, ui_lblItemGlyph, module, item_glyph);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblItem, ui_lblItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblItem);
}

/* Talk about a confusing state, but here we go!
 * 0=Press A, 1=Hold A, 2=Load State, 3=Start Fresh
*/
int launch_flag(int mode, int held) {
    static const uint8_t LAUNCH[4][2] = {
            {1, 0}, // 0 Press A
            {0, 1}, // 1 Hold A
            {0, 0}, // 2 Load State (always)
            {1, 1}  // 3 Start Fresh (always)
    };
    if ((unsigned) mode > 3) mode = 0;
    return LAUNCH[mode][held ? 0 : 1];
}

void reset_ui_groups(void) {
    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    lv_group_set_focus_cb(ui_group_panel, nav_focus_bounce_cb);
    nav_suppress_next_bounce();
}

void add_ui_groups(lv_obj_t **options, lv_obj_t **values, lv_obj_t **glyphs, lv_obj_t **panels, int long_dot) {
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_obj_t *opt = options ? options[i] : NULL;
        lv_obj_t *val = values ? values[i] : NULL;
        lv_obj_t *ico = glyphs ? glyphs[i] : NULL;
        lv_obj_t *pnl = panels ? panels[i] : NULL;

        if (opt) lv_group_add_obj(ui_group, opt);
        if (val) lv_group_add_obj(ui_group_value, val);
        if (ico) lv_group_add_obj(ui_group_glyph, ico);
        if (pnl) lv_group_add_obj(ui_group_panel, pnl);

        if (long_dot && pnl && opt) apply_text_long_dot(&theme, pnl, opt);
    }
}

void adjust_gen_panel(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

void ui_gen_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, WALL_GENERAL);
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnlContent);

        nav_moved = 0;
    }
}

void gen_step_movement(int steps, int direction, int long_dot, int count_offset, int sound) {
    if (!ui_count) return;

    if (first_open) {
        first_open = 0;
    } else if (!nav_silent) {
        if (sound) play_sound(SND_NAVIGATE);
    }

    for (int step = 0; step < steps; ++step) {
        if (long_dot == 1) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));
        else if (long_dot == 2) apply_option_label_long_dot(lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT + count_offset, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    if (long_dot == 1) {
        lv_obj_update_layout(ui_pnlContent);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.VISUAL.NAMESCROLL);
    } else if (long_dot == 2) {
        set_option_label_scroll_mode(lv_group_get_focused(ui_group));
    }

    nav_moved = 1;
}

void list_nav_cb_prev(int steps) {
    gen_step_movement(steps, -1, 1, 0, 1);
}

void list_nav_cb_next(int steps) {
    gen_step_movement(steps, +1, 1, 0, 1);
}

void list_nav_cb_prev_nowrap(int steps) {
    gen_step_movement(steps, -1, 2, 0, 1);
}

void list_nav_cb_next_nowrap(int steps) {
    gen_step_movement(steps, +1, 2, 0, 1);
}

void handle_msgbox_dismiss(void) {
    msgbox_active = 0;
    progress_onscreen = 0;

    if (crash_ui_dismiss()) {
        play_sound(SND_CONFIRM);
        return;
    }

    if (power_loss_ui_dismiss()) {
        play_sound(SND_CONFIRM);
        return;
    }

    play_sound(SND_INFO_CLOSE);
    if (msgbox_element == ui_pnlHelp) {
        hide_info_box();
    } else {
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
    }
}

int build_safe_path(char *dst, size_t n, const char *base, const char *name) {
    int len = snprintf(dst, n, "%s/%s", base, name);
    if (len < 0 || (size_t) len >= n) return -1;

    char resolved[PATH_MAX];
    if (!realpath(dst, resolved)) return -1;

    size_t base_len = strlen(base);
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

static void fn_build_table(struct json root) {
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
        struct json val = json_next(key);

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

void resolve_friendly_name(char *file_path, char *out) {
    char stripped[MAX_BUFFER_SIZE];
    const char *base = strrchr(file_path, '/');

    base = base ? base + 1 : file_path;
    snprintf(stripped, sizeof(stripped), "%s", base);

    char *dot = strrchr(stripped, '.');
    if (dot) *dot = '\0';

    char lowered[MAX_BUFFER_SIZE];
    size_t idx = 0;
    for (; stripped[idx]; idx++) lowered[idx] = (char) tolower((unsigned char) stripped[idx]);
    lowered[idx] = '\0';

    int has_custom = 0;

    static char cached_file_parent[PATH_MAX];
    static char cached_name_only[MAX_BUFFER_SIZE];

    const char *slash = strrchr(file_path, '/');
    size_t parent_len = slash ? (size_t) (slash - file_path) : 0;

    if (strncmp(file_path, cached_file_parent, parent_len) != 0 ||
        cached_file_parent[parent_len] != '\0') {

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

    static char cache_path[PATH_MAX];
    static char *cache_str = NULL;
    static struct json cache_root;
    static int cache_valid = 0;

    const char *lookup_path = last_lookup_path;

    if (lookup_path) {
        if (strcmp(cache_path, lookup_path) != 0) {
            free(cache_str);
            cache_str = read_all_char_from(lookup_path);
            cache_valid = (cache_str != NULL && json_valid(cache_str));

            if (cache_valid) {
                cache_root = json_parse(cache_str);
                fn_build_table(cache_root);
            } else {
                fn_build_table((struct json) {0});
            }

            snprintf(cache_path, sizeof(cache_path), "%s", lookup_path);
        }

        if (cache_valid && fn_name_table_count > 0) {
            fn_entry_t needle = {(char *) lowered, NULL};
            fn_entry_t *found = bsearch(&needle, fn_name_table, (size_t) fn_name_table_count, sizeof(fn_entry_t), fn_entry_cmp);

            if (found) {
                snprintf(out, MAX_BUFFER_SIZE, "%s", found->val);
                has_custom = 1;
            }
        }
    }

    if (!has_custom) {
        const char *lk = lookup(stripped);
        snprintf(out, MAX_BUFFER_SIZE, "%s", lk ? lk : stripped);
    }
}

void adjust_label_value_width(lv_obj_t *panel, lv_obj_t *label, lv_obj_t *value) {
    lv_obj_update_layout(panel);

    lv_coord_t panel_width = lv_obj_get_width(panel);
    if (panel_width <= 0) return;

    const char *label_text = lv_label_get_text(label);
    if (!label_text) return;

    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    lv_coord_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);

    lv_point_t text_width;
    lv_txt_get_size(&text_width, label_text, font, letter_space, line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t label_text_width = text_width.x;

    // Okay 64 leaves a good gap for both long static and long animated
    lv_coord_t available = panel_width - label_text_width - theme.FONT.LIST_PAD_LEFT - 64;

    lv_obj_set_width(value, available);
}

void update_label_scroll() {
    if (lv_group_get_focused(ui_group_value)) {
        set_label_long_mode(&theme, lv_group_get_focused(ui_group_value), config.VISUAL.NAMESCROLL);
    }
}

void render_image_refresh(const char *image_type, char *h_core_artwork, char *h_file_name,
                          lv_obj_t *ui_imgSplash, lv_obj_t *ui_viewport_objects[],
                          int *starter_image, int *splash_valid) {
    if (strlen(h_core_artwork) <= 1) {
        if (strcasecmp(image_type, "preview") == 0) {
            lv_img_set_src(ui_imgHelpPreviewImage, &ui_img_blank);
        } else if (strcasecmp(image_type, "splash") == 0) {
            *splash_valid = 0;
            lv_img_set_src(ui_imgSplash, &ui_img_blank);
        } else {
            lv_img_set_src(ui_imgBox, &ui_img_blank);
        }
        return;
    }

    char image[MAX_BUFFER_SIZE] = {0};

    if (strcasecmp(image_type, "box") != 0 || !grid_mode_enabled || !config.VISUAL.BOX_ART_HIDE) {
        load_image_catalogue(h_core_artwork, h_file_name, "", "default", mux_dim, image_type, image, sizeof(image));
        LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);
    }

    if (strcasecmp(image_type, "splash") == 0 && !file_exist(image)) {
        load_splash_image_fallback(mux_dim, image, sizeof(image));
        LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);
    }

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcasecmp(preview_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                struct ImageSettings image_settings = {
                        image, LV_ALIGN_CENTER,
                        validate_int16((int16_t) (device.MUX.WIDTH * .9) - 60, "width"),
                        validate_int16((int16_t) (device.MUX.HEIGHT * .9) - 120, "height"),
                        0, 0, 0, 0
                };

                update_image(ui_imgHelpPreviewImage, image_settings);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgHelpPreviewImage, &ui_img_blank);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), " ");
            }
        }
    } else if (strcasecmp(image_type, "splash") == 0) {
        if (strcasecmp(splash_image_previous_path, image) != 0) {
            char image_path[MAX_BUFFER_SIZE];

            if (file_exist(image)) {
                *splash_valid = 1;

                snprintf(image_path, sizeof(image_path), "M:%s", image);
                lv_img_set_src(ui_imgSplash, image_path);

                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), "%s", image);
            } else {
                *splash_valid = 0;
                lv_img_set_src(ui_imgSplash, &ui_img_blank);
                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), " ");
            }
        }
    } else {
        if (strcasecmp(box_image_previous_path, image) != 0) {
            char artwork_config_path[MAX_BUFFER_SIZE];
            snprintf(artwork_config_path, sizeof(artwork_config_path), INFO_CAT_PATH "/%s.ini", h_core_artwork);

            if (!file_exist(artwork_config_path)) snprintf(artwork_config_path, sizeof(artwork_config_path), INFO_CAT_PATH "/default.ini");

            if (file_exist(artwork_config_path)) {
                viewport_refresh(ui_viewport_objects, artwork_config_path, h_core_artwork, h_file_name);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                char image_path[MAX_BUFFER_SIZE];

                if (file_exist(image)) {
                    *starter_image = 1;

                    int box_w = device.MUX.WIDTH;
                    int fullscreen = config.VISUAL.BOX_ART == 2 || config.VISUAL.BOX_ART == 3;
                    int box_h = fullscreen ? device.MUX.HEIGHT : device.MUX.HEIGHT - theme.HEADER.HEIGHT - theme.FOOTER.HEIGHT - 4;
                    if (box_h <= 0) box_h = device.MUX.HEIGHT;

                    int16_t max_w = (int16_t) (config.VISUAL.BOX_ART_SCALE > 0 ? box_w * config.VISUAL.BOX_ART_SCALE / 100 : 0);

                    size_t ilen = strlen(image);
                    if (ilen > 4 && strcmp(image + ilen - 4, ".svg") == 0) {
                        int svg_w = max_w > 0 ? max_w : box_w;
                        int svg_h = box_h;
                        snprintf(image_path, sizeof(image_path), "M:%s?%dx%d", image, svg_w, svg_h);
                        lv_img_set_size_mode(ui_imgBox, LV_IMG_SIZE_MODE_VIRTUAL);
                        lv_img_set_zoom(ui_imgBox, LV_IMG_ZOOM_NONE);
                        lv_img_set_src(ui_imgBox, image_path);
                    } else {
                        struct ImageSettings image_settings = {
                                image, -1,
                                max_w, (int16_t) box_h,
                                theme.IMAGE_LIST.PAD_LEFT, theme.IMAGE_LIST.PAD_RIGHT,
                                theme.IMAGE_LIST.PAD_TOP, theme.IMAGE_LIST.PAD_BOTTOM
                        };
                        update_image(ui_imgBox, image_settings);
                    }

                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
                } else {
                    lv_img_set_src(ui_imgBox, &ui_img_blank);
                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
                }
            }
        }
    }
}

void clear_box_image() {
    lv_img_set_src(ui_imgBox, &ui_img_blank);
    snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
}

void resolve_grid_item_images(const char *mux_dim, const char *mux_module, const char *glyph_name,
                              char *grid_img, size_t img_size,
                              char *grid_img_foc, size_t foc_size) {
    if (!load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name, "default", "svg", grid_img, img_size)) {
        load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name, "default", "png", grid_img, img_size);
    }

    char glyph_name_focused[MAX_BUFFER_SIZE];
    snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_name);

    grid_img_foc[0] = '\0';
    if (!load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name_focused, "default_focused", "svg", grid_img_foc, foc_size)) {
        load_element_image_specifics(mux_dim, mux_module, "grid", glyph_name_focused, "default_focused", "png", grid_img_foc, foc_size);
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
