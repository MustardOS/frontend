#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/base/options.h>
#include <common/display/theme.h>
#include <common/platform/device.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/list_frame.h>
#include <common/ui/nav.h>
#include <module/muxshare.h>
#include "menu.h"
#include "reader.h"
#include "view.h"

#define MENU_ROW_MAX     1024
#define MENU_GOTO_JUMP   10
#define MENU_SECTION_KEY "muxreader_settings"
#define MENU_FONT_MAX    256

typedef enum {
    page_none,
    page_root,
    page_settings,
    page_font,
    page_contents,
} menu_page;

typedef enum {
    root_resume,
    root_contents,
    root_goto,
    root_settings,
    root_quit,
    root_total,
} root_item;

typedef enum {
    set_font,
    set_text_size,
    set_margins,
    set_spacing,
    set_fit,
    set_zoom,
    set_order,
    set_theme,
    set_rotation,
    set_transition,
    set_status,
    set_awake,
    set_total,
} setting_item;

typedef struct {
    char name[MAX_BUFFER_SIZE];
    char path[PATH_MAX];
} font_entry;

static menu_page page = page_none;
static lv_obj_t *dim = NULL;
static lv_obj_t *row_panel[MENU_ROW_MAX];
static lv_obj_t *row_label[MENU_ROW_MAX];
static lv_obj_t *row_glyph[MENU_ROW_MAX];
static lv_obj_t *row_value[MENU_ROW_MAX];
static int row_item[MENU_ROW_MAX];
static int row_total = 0;
static int goto_target = 0;
static int contents_from_root = 0;
static list_frame frames[2];

static font_entry fonts[MENU_FONT_MAX];
static int font_count = 0;
static int fonts_loaded = 0;

static int is_font_file(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return strcasecmp(dot, ".ttf") == 0 || strcasecmp(dot, ".otf") == 0 || strcasecmp(dot, ".ttc") == 0;
}

static int is_variant_file(const char *name) {
    static const char *marks[] = {"-Bold", " Bold", "-Italic", " Italic", "Oblique"};
    for (size_t i = 0; i < A_SIZE(marks); i++)
        if (strstr(name, marks[i])) return 1;
    return 0;
}

static void add_font(const char *directory, const char *file) {
    if (font_count >= MENU_FONT_MAX) return;

    font_entry *entry = &fonts[font_count];
    snprintf(entry->path, sizeof(entry->path), "%s/%s", directory, file);
    snprintf(entry->name, sizeof(entry->name), "%s", file);

    char *dot = strrchr(entry->name, '.');
    if (dot) *dot = '\0';

    for (int i = 0; i < font_count; i++)
        if (strcasecmp(fonts[i].name, entry->name) == 0) return;

    font_count++;
}

static void scan_fonts(const char *directory, const int nested) {
    DIR *dir = opendir(directory);
    if (!dir) return;

    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;

        if (entry->d_type == DT_DIR) {
            if (!nested) continue;
            char sub[PATH_MAX];
            snprintf(sub, sizeof(sub), "%s/%s", directory, entry->d_name);
            scan_fonts(sub, 0);
            continue;
        }

        if (is_font_file(entry->d_name) && !is_variant_file(entry->d_name)) add_font(directory, entry->d_name);
    }

    closedir(dir);
}

static int compare_fonts(const void *left, const void *right) {
    return strcasecmp(((const font_entry *) left)->name, ((const font_entry *) right)->name);
}

static void load_fonts(void) {
    if (fonts_loaded) return;
    fonts_loaded = 1;

    scan_fonts(INTERNAL_FONTS, 0);

    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};
    for (size_t i = 0; i < A_SIZE(mounts); i++) {
        if (!mounts[i] || !mounts[i][0]) continue;
        char directory[PATH_MAX];
        snprintf(directory, sizeof(directory), "%s/%s", mounts[i], USER_FONTS);
        scan_fonts(directory, 1);
    }

    qsort(fonts, (size_t) font_count, sizeof(fonts[0]), compare_fonts);
}

static const char *font_name(void) {
    if (!settings.font[0]) return lang.muxreader.font_default;

    load_fonts();
    for (int i = 0; i < font_count; i++)
        if (strcmp(fonts[i].path, settings.font) == 0) return fonts[i].name;

    const char *slash = strrchr(settings.font, '/');
    return slash ? slash + 1 : settings.font;
}

static const char *toggle_text(const int value) {
    return value ? lang.generic.enabled : lang.generic.disabled;
}

static const char *theme_name(const int value) {
    switch (value) {
        case reader_theme_sepia:
            return lang.muxreader.theme_sepia;
        case reader_theme_dark:
            return lang.muxreader.theme_dark;
        case reader_theme_night:
            return lang.muxreader.theme_night;
        default:
            return lang.muxreader.theme_light;
    }
}

static const char *transition_name(const int value) {
    switch (value) {
        case reader_transition_crossfade:
            return lang.muxreader.transition_crossfade;
        case reader_transition_slide:
            return lang.muxreader.transition_slide;
        case reader_transition_page_turn:
            return lang.muxreader.transition_page_turn;
        default:
            return lang.muxreader.transition_instant;
    }
}

static const char *rotation_name(const int value) {
    switch (value) {
        case reader_rotation_right:
            return lang.muxreader.rotation_right;
        case reader_rotation_left:
            return lang.muxreader.rotation_left;
        case reader_rotation_upside:
            return lang.muxreader.rotation_upside;
        default:
            return lang.muxreader.rotation_none;
    }
}

static void root_value(const root_item item, char *out, const size_t size) {
    out[0] = '\0';
    if (item == root_goto) snprintf(out, size, "%d / %d", goto_target + 1, reader_page_count());
}

static void setting_value(const setting_item item, char *out, const size_t size) {
    switch (item) {
        case set_font:
            snprintf(out, size, "%s", font_name());
            break;
        case set_text_size:
            snprintf(out, size, "%d px", settings.text_size);
            break;
        case set_margins:
            snprintf(out, size, "%d px", settings.margin);
            break;
        case set_spacing:
            if (reader_spacing_steps[settings.line_spacing] == 0)
                snprintf(out, size, "%s", lang.generic.normal);
            else
                snprintf(out, size, "%d%%", reader_spacing_steps[settings.line_spacing]);
            break;
        case set_fit:
            snprintf(out, size, "%s", settings.fit_page ? lang.muxreader.fit_page : lang.muxreader.fit_width);
            break;
        case set_zoom:
            snprintf(out, size, "%d%%", reader_zoom_steps[settings.zoom]);
            break;
        case set_order:
            snprintf(out, size, "%s", reader_reversed() ? lang.muxreader.order_reversed : lang.generic.normal);
            break;
        case set_theme:
            snprintf(out, size, "%s", theme_name(settings.theme));
            break;
        case set_rotation:
            snprintf(out, size, "%s", rotation_name(settings.rotation));
            break;
        case set_transition:
            snprintf(out, size, "%s", transition_name(settings.transition));
            break;
        case set_status:
            snprintf(out, size, "%s", toggle_text(settings.page_status));
            break;
        case set_awake:
            snprintf(out, size, "%s", toggle_text(settings.stay_awake));
            break;
        default:
            out[0] = '\0';
            break;
    }
}

static void chrome_show(const char *title) {
    lv_label_set_text(ui_lbl_title, title);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dim);
    lv_obj_move_foreground(ui_pnl_header);
    lv_obj_move_foreground(ui_pnl_content);
    lv_obj_move_foreground(ui_pnl_footer);
}

static void chrome_hide(void) {
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(ui_screen);
}

static void begin_page(const menu_page next) {
    if (page == page_settings && next != page_settings) list_frame_remember_section_key(MENU_SECTION_KEY);

    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;
    row_total = 0;
    page = next;
}

static void add_row(const int item, const char *glyph, const char *label, const char *value) {
    if (row_total >= MENU_ROW_MAX) return;

    const int has_value = value && value[0];
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(panel);

    lv_obj_t *text = lv_label_create(panel);
    apply_theme_option_item_label(&theme, text, label, has_value);

    lv_obj_t *icon = lv_img_create(panel);
    apply_theme_list_glyph(&theme, icon, mux_module, glyph);

    lv_obj_t *detail = lv_label_create(panel);
    apply_theme_list_value(&theme, detail, has_value ? value : "");

    lv_group_add_obj(ui_group, text);
    lv_group_add_obj(ui_group_value, detail);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);

    if (has_value) adjust_label_value_width(panel, text, detail);
    apply_option_label_long_dot(text);
    apply_option_value_long_dot(detail);

    row_panel[row_total] = panel;
    row_label[row_total] = text;
    row_glyph[row_total] = icon;
    row_value[row_total] = detail;
    row_item[row_total] = item;
    row_total++;
    ui_count_static++;
}

static int current_row(void) {
    if (page == page_settings) return list_frame_current_row();
    return current_item_index >= 0 && current_item_index < row_total ? current_item_index : -1;
}

static int index_of(const int item) {
    for (int i = 0; i < row_total; i++)
        if (row_item[i] == item) return i;
    return -1;
}

static void focus_plain(const int index) {
    if (row_total <= 0) return;

    const int row = index > 0 && index < row_total ? index : 0;
    current_item_index = row;
    nav_suppress_next_shake();

    lv_group_focus_obj(row_label[row]);
    lv_group_focus_obj(row_value[row]);
    lv_group_focus_obj(row_glyph[row]);
    lv_group_focus_obj(row_panel[row]);

    update_scroll_position(theme.mux.item.count, theme.mux.item.panel, ui_count_static, row, ui_pnl_content);
    set_option_label_scroll_mode(row_label[row]);
    set_option_value_scroll_mode(row_value[row]);
    nav_refresh_list_overflow(ui_pnl_content);
}

static void set_row_value(const int row, const char *value) {
    if (row < 0 || row >= row_total) return;
    lv_label_set_text(row_value[row], value);
    if (value[0])
        lv_obj_clear_flag(row_value[row], LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(row_value[row], LV_OBJ_FLAG_HIDDEN);
    adjust_label_value_width(row_panel[row], row_label[row], row_value[row]);
}

static void footer_prepare(void) {
    nav_hide_all();
    lv_obj_set_align(ui_pnl_footer, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(ui_pnl_footer, lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha, MU_OBJ_MAIN_DEFAULT);
}

static void show_nav(void) {
    footer_prepare();

    if (page == page_font || page == page_contents) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.select, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    } else {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.generic.change, 0},
                                      {ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.select, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, page == page_root ? lang.generic.close : lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    }

    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_pnl_footer);
}

static void remember_root(void) {
    if (page != page_root) return;
    const int row = current_row();
    if (row >= 0 && row_item[row] != root_quit) settings.menu_item = row_item[row];
}

static void open_root(const int focus_item) {
    const doc_toc_entry *entries = NULL;
    const int toc_count = reader_toc(&entries);

    begin_page(page_root);
    goto_target = reader_page();

    char value[MAX_BUFFER_SIZE];
    add_row(root_resume, "resume", lang.muxreader.resume, "");
    if (toc_count > 0) add_row(root_contents, "contents", lang.muxreader.contents, "");
    root_value(root_goto, value, sizeof(value));
    add_row(root_goto, "page", lang.muxreader.go_to_page, value);
    add_row(root_settings, "settings", lang.muxreader.settings, "");
    add_row(root_quit, "quit", lang.muxreader.quit, "");

    chrome_show(reader_title());
    focus_plain(index_of(focus_item));
    show_nav();
}

static void open_settings(const int focus_item) {
    begin_page(page_settings);

    char value[MAX_BUFFER_SIZE];
    const setting_item layout_rows[] = {set_font, set_text_size, set_margins, set_spacing};
    const setting_item page_rows[] = {set_fit, set_zoom, set_order};
    const setting_item *first = reader_reflowable() ? layout_rows : page_rows;
    const int first_count = reader_reflowable() ? (int) A_SIZE(layout_rows) : (int) A_SIZE(page_rows);

    static const char *glyphs[set_total] = {"font",  "fontsize", "margin", "spacing",    "fit",    "zoom",
                                            "order", "theme",    "rotate", "transition", "status", "awake"};
    const char *labels[set_total] = {
        lang.muxreader.font,     lang.muxreader.text_size,  lang.muxreader.margins,     lang.muxreader.line_spacing,
        lang.muxreader.page_fit, lang.muxreader.zoom,       lang.muxreader.page_order,  lang.muxreader.theme,
        lang.muxreader.rotation, lang.muxreader.transition, lang.muxreader.page_status, lang.muxreader.stay_awake,
    };

    for (int i = 0; i < first_count; i++) {
        setting_value(first[i], value, sizeof(value));
        add_row(first[i], glyphs[first[i]], labels[first[i]], value);
    }

    const setting_item reading_rows[] = {set_theme, set_rotation, set_transition, set_status, set_awake};
    for (size_t i = 0; i < A_SIZE(reading_rows); i++) {
        if (reading_rows[i] == set_theme && reader_plain()) continue;
        setting_value(reading_rows[i], value, sizeof(value));
        add_row(reading_rows[i], glyphs[reading_rows[i]], labels[reading_rows[i]], value);
    }

    frames[0] = (list_frame) {lang.muxreader.section_layout, 0, first_count};
    frames[1] = (list_frame) {lang.muxreader.section_reading, first_count, row_total - first_count};

    chrome_show(lang.muxreader.settings);

    if (list_frame_init(&theme, ui_pnl_content, frames, 2, row_panel, row_label, row_glyph, row_value, row_total)) {
        const int row = focus_item >= 0 ? index_of(focus_item) : -1;
        if (row >= 0) {
            list_frame_apply();
            list_frame_go(row < first_count ? 0 : 1);
            const int steps = list_frame_steps_to_row(row);
            if (steps > 0) gen_step_movement(steps, 1, 2, 0, 0);
        } else {
            const int steps = list_frame_restore_key(MENU_SECTION_KEY);
            if (steps > 0) gen_step_movement(steps, 1, 2, 0, 0);
        }
    }

    show_nav();
}

static void open_fonts(void) {
    load_fonts();
    begin_page(page_font);

    int focus = 0;
    add_row(-1, "font", lang.muxreader.font_default, settings.font[0] ? "" : lang.muxreader.font_current);
    for (int i = 0; i < font_count; i++) {
        const int current = strcmp(fonts[i].path, settings.font) == 0;
        if (current) focus = row_total;
        add_row(i, "font", fonts[i].name, current ? lang.muxreader.font_current : "");
    }

    chrome_show(lang.muxreader.font);
    focus_plain(focus);
    show_nav();
}

static void open_contents_page(const int from_root) {
    const doc_toc_entry *entries = NULL;
    const int count = reader_toc(&entries);
    if (count <= 0) {
        toast_message(lang.muxreader.no_contents, 1500);
        return;
    }

    remember_root();
    contents_from_root = from_root;
    begin_page(page_contents);

    int focus = 0;
    const int current = reader_page();
    char label[MAX_BUFFER_SIZE];
    char value[32];
    for (int i = 0; i < count && row_total < MENU_ROW_MAX; i++) {
        const int indent = entries[i].depth > 6 ? 6 : entries[i].depth;
        snprintf(label, sizeof(label), "%*s%s", indent * 2, "", entries[i].title);
        snprintf(value, sizeof(value), "%d", entries[i].page + 1);
        add_row(i, "", label, value);
        if (entries[i].page <= current) focus = i;
    }

    chrome_show(lang.muxreader.contents);
    focus_plain(focus);
    show_nav();
}

void menu_init(void) {
    dim = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, device.mux.width, device.mux.height);
    lv_obj_set_style_bg_color(dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim, LV_OPA_90, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);

    init_ui_item_counter(&theme);
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);

    header_and_footer_setup();
    chrome_hide();
}

int menu_active(void) {
    return page != page_none;
}

void menu_open(void) {
    open_root(settings.menu_item >= 0 && settings.menu_item < root_total ? settings.menu_item : root_resume);
}

void menu_open_contents(void) {
    open_contents_page(0);
}

void menu_close(void) {
    if (page == page_none) return;

    remember_root();
    begin_page(page_none);
    chrome_hide();
    reader_settings_save(&settings);
}

void menu_move(const int direction) {
    if (page == page_none || row_total < 2) return;
    gen_step_movement(1, direction, 2, 0, 1);
}

void menu_refresh_values(void) {
    char value[MAX_BUFFER_SIZE];

    if (page == page_root) {
        goto_target = goto_target >= reader_page_count() ? reader_page_count() - 1 : goto_target;
        root_value(root_goto, value, sizeof(value));
        set_row_value(index_of(root_goto), value);
    } else if (page == page_settings) {
        for (int row = 0; row < row_total; row++) {
            setting_value((setting_item) row_item[row], value, sizeof(value));
            set_row_value(row, value);
        }
    }
}

static int adjust_setting(const setting_item item, const int direction) {
    switch (item) {
        case set_font:
            open_fonts();
            return 1;
        case set_text_size:
        case set_zoom:
            reader_change_size(direction);
            break;
        case set_margins:
            reader_change_margin(direction);
            break;
        case set_spacing:
            reader_change_spacing(direction);
            break;
        case set_fit:
            reader_toggle_fit();
            break;
        case set_order:
            reader_toggle_reversed();
            break;
        case set_theme:
            reader_cycle_theme(direction);
            break;
        case set_rotation:
            reader_cycle_rotation(direction);
            break;
        case set_transition:
            settings.transition = (settings.transition + reader_transition_count + direction) % reader_transition_count;
            view_set_transition(settings.transition);
            break;
        case set_status:
            reader_toggle_status();
            break;
        case set_awake:
            settings.stay_awake = !settings.stay_awake;
            break;
        default:
            return 0;
    }

    menu_refresh_values();
    return 1;
}

void menu_change(const int direction) {
    if (page == page_root) {
        const int row = current_row();
        if (row < 0 || row_item[row] != root_goto) return;

        int next = goto_target + direction;
        if (next < 0) next = 0;
        if (next >= reader_page_count()) next = reader_page_count() - 1;
        if (next == goto_target) return;

        goto_target = next;
        menu_refresh_values();
        play_sound(snd_option);
        return;
    }

    if (page != page_settings) return;

    if (list_frame_focused()) {
        if (list_frame_move(direction)) {
            play_sound(snd_navigate);
            gen_step_movement(0, 1, 2, 0, 0);
            show_nav();
        }
        return;
    }

    const int row = current_row();
    if (row < 0 || row_item[row] == set_font) return;
    if (adjust_setting((setting_item) row_item[row], direction)) play_sound(snd_option);
}

void menu_shoulder(const int direction) {
    if (page == page_root) {
        const int row = current_row();
        if (row >= 0 && row_item[row] == root_goto) {
            int next = goto_target + direction * MENU_GOTO_JUMP;
            if (next < 0) next = 0;
            if (next >= reader_page_count()) next = reader_page_count() - 1;
            goto_target = next;
            menu_refresh_values();
            play_sound(snd_option);
        }
        return;
    }

    if (page == page_settings) {
        if (list_frame_move(direction)) {
            play_sound(snd_navigate);
            gen_step_movement(0, 1, 2, 0, 0);
            show_nav();
        }
        return;
    }

    const int rows = theme.mux.item.count > 0 ? theme.mux.item.count : 8;
    int steps = rows;
    if (direction < 0 && steps > current_item_index) steps = current_item_index;
    if (direction > 0 && steps > row_total - current_item_index - 1) steps = row_total - current_item_index - 1;
    if (steps > 0) gen_step_movement(steps, direction, 2, 0, 1);
}

void menu_accept(void) {
    const int row = current_row();

    switch (page) {
        case page_root:
            if (row < 0) return;
            switch ((root_item) row_item[row]) {
                case root_resume:
                    menu_close();
                    break;
                case root_contents:
                    open_contents_page(1);
                    break;
                case root_goto: {
                    const int target = goto_target;
                    menu_close();
                    reader_go_to(target);
                    break;
                }
                case root_settings:
                    remember_root();
                    open_settings(-1);
                    break;
                case root_quit:
                    reader_quit();
                    break;
                default:
                    break;
            }
            break;
        case page_settings:
            if (row >= 0) adjust_setting((setting_item) row_item[row], 1);
            break;
        case page_font:
            if (row < 0) return;
            reader_set_font(row_item[row] < 0 ? "" : fonts[row_item[row]].path);
            open_settings(set_font);
            break;
        case page_contents: {
            if (row < 0) return;
            const doc_toc_entry *entries = NULL;
            reader_toc(&entries);
            const int target = entries[row_item[row]].page;
            menu_close();
            reader_go_to(target);
            break;
        }
        default:
            break;
    }
}

void menu_back(void) {
    switch (page) {
        case page_settings:
            open_root(root_settings);
            break;
        case page_font:
            open_settings(set_font);
            break;
        case page_contents:
            if (contents_from_root)
                open_root(root_contents);
            else
                menu_close();
            break;
        default:
            menu_close();
            break;
    }
}
