#include "muxshare.h"
#include "ui/ui_muxspace.h"

#define SPACE(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(SPACE_ELEMENTS) };
#undef SPACE

#define SPACE_BAR_WARN 70
#define SPACE_BAR_FULL 90

#define SPACE_COLOR_OK   0x4FC04F
#define SPACE_COLOR_WARN 0xE0A020
#define SPACE_COLOR_FULL 0xEE3F3F

#define SPACE_BAR_BASE_PX  24
#define SPACE_BAR_BOOST_PX 0
#define SPACE_DETAIL_ROWS  5

static int show_details = 0;

typedef struct {
    lv_obj_t *value_panel;
    lv_obj_t *bar_panel;
    lv_obj_t *value;
    lv_obj_t *bar;
    lv_obj_t *title;
    const char *partition;
    int show_msd;
} mount;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *row[SPACE_DETAIL_ROWS];
    lv_obj_t *label[SPACE_DETAIL_ROWS];
    lv_obj_t *value[SPACE_DETAIL_ROWS];
} detail_ui;

typedef struct {
    int has_verdict;
    char verdict[32];
    char manufacturer[32];
    char model[32];
    char date[16];
} msd_info;

static detail_ui details_ui[4];

static void show_help(void) {
    show_info_box(lang.muxspace.title, lang.muxspace.help, 0);
}

static void init_space_bars(void) {
    lv_bar_set_range(ui_bar_primary_space, 0, 100);
    lv_bar_set_range(ui_bar_secondary_space, 0, 100);
    lv_bar_set_range(ui_bar_external_space, 0, 100);
    lv_bar_set_range(ui_bar_system_space, 0, 100);

    lv_bar_set_mode(ui_bar_primary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_secondary_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_external_space, LV_BAR_MODE_NORMAL);
    lv_bar_set_mode(ui_bar_system_space, LV_BAR_MODE_NORMAL);
}

static void resolve_mount(const char *mount_point, char *dev_out, char *fs_out) {
    dev_out[0] = '\0';
    fs_out[0] = '\0';

    if (!mount_point || !*mount_point) return;

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return;

    char src[128], mnt[256], fs[32];
    while (fscanf(fp, "%127s %255s %31s %*s %*d %*d\n", src, mnt, fs) == 3) {
        if (strcmp(mnt, mount_point) != 0) continue;
        snprintf(dev_out, 128, "%s", src);
        snprintf(fs_out, 32, "%s", fs);
        break;
    }

    fclose(fp);
}

static int extract_kv(const char *line, const char *key, char *out, const size_t out_len) {
    const char *p = line;
    if (strncmp(p, "INFO: ", 6) == 0 || strncmp(p, "WARN: ", 6) == 0) {
        p += 6;
    } else {
        return 0;
    }

    const size_t klen = strlen(key);
    if (strncmp(p, key, klen) != 0) return 0;

    p += klen;
    if (*p != ':') return 0;

    p++;
    while (*p == ' ')
        p++;

    snprintf(out, out_len, "%s", p);
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }

    return 1;
}

static void strip_paren_suffix(const char *s) {
    char *paren = strrchr(s, '(');
    if (!paren || paren == s) return;

    char *t = paren - 1;
    while (t > s && *t == ' ')
        t--;

    *(t + 1) = '\0';
}

static int read_msd_info(const char *dev_path, msd_info *out) {
    memset(out, 0, sizeof(*out));

    if (!*dev_path) return 0;

    const char *base = strrchr(dev_path, '/');
    base = base ? base + 1 : dev_path;
    if (strncmp(base, "mmcblk", 6) != 0) return 0;

    char block[32];
    snprintf(block, sizeof(block), "%s", base);
    char *p = strchr(block, 'p');
    if (p) *p = '\0';

    char chk_path[64];
    snprintf(chk_path, sizeof(chk_path), "/tmp/msd_check/%s", block);

    FILE *fp = fopen(chk_path, "r");
    if (!fp) return 0;

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        snprintf(out->verdict, sizeof(out->verdict), "%s", line);
        size_t n = strlen(out->verdict);

        while (n > 0 && (out->verdict[n - 1] == '\n' || out->verdict[n - 1] == ' ')) {
            out->verdict[--n] = '\0';
        }

        char *sp = strchr(out->verdict, ' ');
        if (sp) *sp = '\0';

        out->has_verdict = out->verdict[0] != '\0';
    }

    while (fgets(line, sizeof(line), fp)) {
        if (!out->manufacturer[0] && extract_kv(line, "Manufacturer", out->manufacturer, sizeof(out->manufacturer))) {
            strip_paren_suffix(out->manufacturer);
            continue;
        }

        if (!out->model[0] && extract_kv(line, "Card name", out->model, sizeof(out->model))) {
            continue;
        }

        if (!out->date[0] && extract_kv(line, "Manufacturing date", out->date, sizeof(out->date))) {
        }
    }

    fclose(fp);
    return out->has_verdict;
}

static const char *verdict_tag(const char *verdict) {
    if (!verdict || !*verdict) return NULL;

    if (!strcmp(verdict, "GENUINE")) return lang.muxspace.quality.genuine;
    if (!strcmp(verdict, "LIKELY_GENUINE")) return lang.muxspace.quality.likely_genuine;
    if (!strcmp(verdict, "SUSPICIOUS")) return lang.muxspace.quality.suspicious;
    if (!strcmp(verdict, "SUSPECTED_FAKE")) return lang.muxspace.quality.suspected_fake;
    if (!strcmp(verdict, "FAKE")) return lang.muxspace.quality.fake;
    if (!strcmp(verdict, "TRASH")) return lang.muxspace.quality.trash;

    return NULL;
}

static void build_detail_ui(detail_ui *du, const lv_obj_t *bar_panel) {
    if (du->container) return;

    lv_obj_t *parent = lv_obj_get_parent(bar_panel);
    if (!parent) return;

    du->container = lv_obj_create(parent);
    lv_obj_remove_style_all(du->container);
    lv_obj_set_width(du->container, LV_PCT(100));
    lv_obj_set_height(du->container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(du->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(du->container, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(du->container, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_left(du->container, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_right(du->container, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(du->container, 4, LV_PART_MAIN);
    lv_obj_add_flag(du->container, LV_OBJ_FLAG_HIDDEN);

    const uint32_t bar_idx = lv_obj_get_index(bar_panel);
    lv_obj_move_to_index(du->container, (int16_t) bar_idx + 1);

    for (int i = 0; i < SPACE_DETAIL_ROWS; i++) {
        du->row[i] = lv_obj_create(du->container);
        lv_obj_remove_style_all(du->row[i]);
        lv_obj_set_width(du->row[i], LV_PCT(100));
        lv_obj_set_height(du->row[i], LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(du->row[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(du->row[i], LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        du->label[i] = lv_label_create(du->row[i]);
        du->value[i] = lv_label_create(du->row[i]);

        lv_obj_add_flag(du->row[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void apply_detail_style(const detail_ui *du) {
    if (!du->container) return;

    for (int i = 0; i < SPACE_DETAIL_ROWS; i++) {
        if (!du->label[i] || !du->value[i]) continue;
        lv_obj_set_style_text_color(du->label[i], lv_color_hex(theme.list_default.text), LV_PART_MAIN);
        lv_obj_set_style_text_opa(du->label[i], theme.list_default.text_alpha, LV_PART_MAIN);
        lv_obj_set_style_text_color(du->value[i], lv_color_hex(theme.list_default.text), LV_PART_MAIN);
        lv_obj_set_style_text_opa(du->value[i], theme.list_default.text_alpha, LV_PART_MAIN);
    }
}

static void set_detail_row(const detail_ui *du, const int idx, const char *label, const char *value) {
    if (!du->row[idx]) return;

    if (!value || !*value) {
        lv_obj_add_flag(du->row[idx], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(du->label[idx], label);
    lv_label_set_text(du->value[idx], value);
    lv_obj_clear_flag(du->row[idx], LV_OBJ_FLAG_HIDDEN);
}

static void hide_detail_rows(const detail_ui *du) {
    if (!du->container) return;

    for (int i = 0; i < SPACE_DETAIL_ROWS; i++) {
        if (du->row[i]) lv_obj_add_flag(du->row[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_flag(du->container, LV_OBJ_FLAG_HIDDEN);
}

static void update_storage_info() {
    const mount storage_info[] = {
        {ui_pnl_primary_space, ui_pnl_primary_bar_space, ui_val_primary_space, ui_bar_primary_space,
         ui_lbl_primary_space, device.storage.rom.mount, 1},

        {ui_pnl_secondary_space, ui_pnl_secondary_bar_space, ui_val_secondary_space, ui_bar_secondary_space,
         ui_lbl_secondary_space, device.storage.sdcard.mount, 1},

        {ui_pnl_external_space, ui_pnl_external_bar_space, ui_val_external_space, ui_bar_external_space,
         ui_lbl_external_space, device.storage.usb.mount, 0},

        {ui_pnl_system_space, ui_pnl_system_bar_space, ui_val_system_space, ui_bar_system_space, ui_lbl_system_space,
         device.storage.root.mount, 0},
    };

    int selected = -1;
    for (size_t i = 0; i < A_SIZE(storage_info); i++) {
        if (lv_obj_has_state(storage_info[i].title, LV_STATE_FOCUSED)) {
            selected = (int) i;
            break;
        }
    }

    for (size_t i = 0; i < A_SIZE(storage_info); i++) {
        double total_space, free_space, used_space;
        get_storage_info(storage_info[i].partition, &total_space, &free_space, &used_space);

        build_detail_ui(&details_ui[i], storage_info[i].bar_panel);
        apply_detail_style(&details_ui[i]);

        const int show_for_this = show_details && (int) i == selected;

        if (total_space > 0) {
            lv_obj_clear_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);

            int percentage = (int) (used_space / total_space * 100.0 + 0.5);

            if (percentage < 0) percentage = 0;
            if (percentage > 100) percentage = 100;

            lv_bar_set_value(storage_info[i].bar, percentage, LV_ANIM_ON);

            char space_info[48];
            snprintf(space_info, sizeof(space_info), "%.2f GB / %.2f GB (%d%%)", used_space, total_space, percentage);
            lv_label_set_text(storage_info[i].value, space_info);

            if (show_for_this) {
                char dev_src[128], fs_type[32];
                msd_info msd = {0};

                resolve_mount(storage_info[i].partition, dev_src, fs_type);
                if (storage_info[i].show_msd) read_msd_info(dev_src, &msd);

                set_detail_row(&details_ui[i], 0, lang.muxspace.detail.filesystem, fs_type);
                set_detail_row(&details_ui[i], 1, lang.muxspace.detail.manufacturer, msd.manufacturer);
                set_detail_row(&details_ui[i], 2, lang.muxspace.detail.model, msd.model);
                set_detail_row(&details_ui[i], 3, lang.muxspace.detail.date, msd.date);
                set_detail_row(
                    &details_ui[i], 4, lang.muxspace.detail.quality_check,
                    msd.has_verdict ? verdict_tag(msd.verdict) : NULL
                );

                lv_obj_clear_flag(details_ui[i].container, LV_OBJ_FLAG_HIDDEN);
            } else {
                hide_detail_rows(&details_ui[i]);
            }

            const lv_coord_t target_h = show_for_this ? SPACE_BAR_BASE_PX + SPACE_BAR_BOOST_PX : SPACE_BAR_BASE_PX;

            if (lv_obj_get_height(storage_info[i].bar_panel) != target_h) {
                lv_obj_set_height(storage_info[i].bar_panel, target_h);
            }

            uint32_t bar_color;
            if (percentage >= SPACE_BAR_FULL) {
                bar_color = SPACE_COLOR_FULL;
            } else if (percentage >= SPACE_BAR_WARN) {
                bar_color = SPACE_COLOR_WARN;
            } else {
                bar_color = SPACE_COLOR_OK;
            }
            lv_obj_set_style_bg_color(storage_info[i].bar, lv_color_hex(bar_color), MU_OBJ_INDI_DEFAULT);
            lv_obj_set_style_bg_opa(storage_info[i].bar, 255, MU_OBJ_INDI_DEFAULT);
        } else {
            lv_obj_add_flag(storage_info[i].value_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(storage_info[i].bar_panel, LV_OBJ_FLAG_HIDDEN);
            hide_detail_rows(&details_ui[i]);
        }
    }
}

static void update_storage_info_cb(const lv_timer_t *timer) {
    (void) timer;
    update_storage_info();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, space, primary, lang.muxspace.primary, "primary", "");
    INIT_VALUE_ITEM(-1, space, secondary, lang.muxspace.secondary, "secondary", "");
    INIT_VALUE_ITEM(-1, space, external, lang.muxspace.external, "external", "");
    INIT_VALUE_ITEM(-1, space, system, lang.muxspace.system, "system", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
    if (show_details) update_storage_info();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "space");

    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    show_details = !show_details;
    play_sound(show_details ? snd_confirm : snd_back);
    update_storage_info();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.details, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxspace_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    show_details = 0;
    memset(details_ui, 0, sizeof(details_ui));

    init_ui_common_screen(&theme, &device, &lang, lang.muxspace.title);

    init_muxspace(ui_pnl_content);
    init_elements();
    init_space_bars();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    update_storage_info();

    init_timer(ui_gen_refresh_task, update_storage_info_cb);
    list_nav_next(0);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
