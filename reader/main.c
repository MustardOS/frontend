#include <limits.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/display/datetime.h>
#include <common/platform/audio.h>
#include <common/platform/battery.h>
#include <common/platform/board.h>
#include <common/platform/device.h>
#include <common/platform/display.h>
#include <common/platform/input.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/storage/fileio.h>
#include <common/ui/common.h>
#include <common/ui/font.h>
#include <common/ui/glyph.h>
#include <common/ui/image.h>
#include <common/ui/nav.h>
#include <common/ui/notify.h>
#include <module/muxshare.h>
#include "engine/document.h"
#include "../video/settings/assets.h"
#include "../video/settings/catalogue.h"
#include "menu.h"
#include "reader.h"
#include "settings.h"
#include "view.h"

#define CHUTNEY_VERSION "1.0.0"

#define REPEAT_DELAY_MS 350
#define REPEAT_RATE_MS  90
#define SCROLL_DIV      6
#define TOAST_MS        1500
#define MENU_PEEK_MS    250
#define GOTO_JUMP       10
#define CACHE_SLOTS     3
#define PREFETCH_IDLE   150
#define POSITION_DELAY  4000
#define AUTO_OVERLAP    12

typedef enum {
    dir_up,
    dir_down,
    dir_left,
    dir_right,
    dir_count,
} direction;

typedef struct {
    int held;
    uint32_t next;
} repeat_state;

static volatile sig_atomic_t terminate_requested = 0;

reader_settings settings;
static reader_settings global_settings;
static reader_settings baseline_settings;
static view_colour filter_colour;
static lv_obj_t *page_panel = NULL;
static lv_obj_t *page_label = NULL;
static lv_obj_t *readtime_panel = NULL;
static lv_obj_t *readtime_label = NULL;
static uint32_t readtime_started = 0;
static doc_engine *doc = NULL;
static char document_path[PATH_MAX];
static char document_title[MAX_BUFFER_SIZE];

static int page = 0;
static int page_count = 0;
static int scroll_x = 0;
static int scroll_y = 0;
static doc_image image = {0};
static int has_image = 0;
static int dirty = 0;
static int running = 1;

static doc_toc_entry *toc = NULL;
static int toc_count = 0;
static int toc_loaded = 0;

static int inhibit_published = 0;
static int reversed = 0;

static int travel = 1;
static uint32_t last_activity = 0;
static uint32_t auto_deadline = 0;
static int position_pending = 0;
static uint32_t position_deadline = 0;
static int saved_position = -1;
static int saved_reversed = -1;

static int bookmarks[READER_BOOKMARK_MAX];
static int bookmark_count = 0;

static void handle_terminate(const int signal_number) {
    (void) signal_number;
    terminate_requested = 1;
}

static void init_navigation_sound(void) {
    const useconds_t backoff[] = {10000, 25000, 50000, 100000, 200000};
    for (size_t attempt = 0; attempt < A_SIZE(backoff); attempt++) {
        if (init_audio_backend()) {
            init_fe_snd(&fe_snd, config.settings.general.sound, 0);
            return;
        }
        usleep(backoff[attempt]);
    }
}

static int reflowable(void) {
    return doc_is_reflowable(doc);
}

static int plain(void) {
    return doc_is_comic(doc);
}

static void logical_size(int *width, int *height) {
    view_logical_size(width, height);
}

static int reading_height(void) {
    int width = 0;
    int height = 0;
    logical_size(&width, &height);
    return height;
}

static int reading_width(void) {
    int width = 0;
    int height = 0;
    logical_size(&width, &height);
    return width;
}

static void forget_toc(void) {
    doc_toc_free(toc, toc_count);
    toc = NULL;
    toc_count = 0;
    toc_loaded = 0;
}

static int file_page(const int value) {
    return reversed && !reflowable() && page_count > 0 ? page_count - 1 - value : value;
}

static int clamp_page(const int value) {
    if (value < 0) return 0;
    if (value >= page_count) return page_count > 0 ? page_count - 1 : 0;
    return value;
}

static void layout(void) {
    if (reflowable()) {
        const doc_reflow reflow = {settings.text_size, settings.margin, reader_spacing_steps[settings.line_spacing]};
        page_count = doc_layout(doc, reading_width(), reading_height(), &reflow);
    } else {
        page_count = doc_page_count(doc);
    }
}

static void clamp_scroll(void) {
    if (!has_image) return;

    const int max_x = image.width - reading_width();
    const int max_y = image.height - reading_height();

    if (scroll_x > max_x) scroll_x = max_x;
    if (scroll_y > max_y) scroll_y = max_y;
    if (scroll_x < 0) scroll_x = 0;
    if (scroll_y < 0) scroll_y = 0;
}

static void ensure_toc(void) {
    if (toc_loaded || !doc) return;
    toc_count = doc_toc(doc, &toc);
    for (int i = 0; i < toc_count; i++)
        toc[i].page = file_page(toc[i].page);
    toc_loaded = 1;
}

static const char *chapter_title(const int at) {
    ensure_toc();

    const char *title = NULL;
    int best = -1;
    for (int i = 0; i < toc_count; i++) {
        if (toc[i].page > at || toc[i].page < best) continue;
        best = toc[i].page;
        title = toc[i].title;
    }
    return title;
}

static int header_shows_clock(void) {
    return settings.page_status == reader_status_clock || settings.page_status == reader_status_both;
}

static int header_shows_battery(void) {
    return settings.page_status == reader_status_battery || settings.page_status == reader_status_both;
}

static void show_if(lv_obj_t *object, const int visible) {
    if (visible)
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *corner_panel(const lv_align_t align, const lv_coord_t x, const char *glyph_name, lv_obj_t **label) {
    lv_obj_t *panel = lv_obj_create(ui_screen);
    lv_obj_set_size(panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 140, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(panel, align, x, -4);

    lv_obj_t *glyph = lv_img_create(panel);
    char embed[MAX_BUFFER_SIZE];
    if (get_glyph_path(mux_module, glyph_name, embed, sizeof(embed))) {
        set_footer_glyph_image(glyph, embed);
        lv_obj_set_style_img_opa(glyph, (lv_opa_t) theme.list_default.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor(glyph, lv_color_hex(theme.list_default.glyph_recolour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(
            glyph, (lv_opa_t) theme.list_default.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT
        );
    } else {
        lv_obj_add_flag(glyph, LV_OBJ_FLAG_HIDDEN);
    }

    load_font_section(FONT_FOOTER_DIR, panel);
    *label = lv_label_create(panel);
    lv_obj_set_style_text_color(*label, lv_color_hex(theme.footer.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(*label, theme.footer.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(*label, "");
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    return panel;
}

static void format_readtime(const uint32_t seconds, char *out, const size_t size) {
    const uint32_t hours = seconds / 3600;
    const uint32_t minutes = seconds / 60 % 60;
    if (hours > 0)
        snprintf(out, size, "%02u:%02u:%02u", hours, minutes, seconds % 60);
    else
        snprintf(out, size, "%02u:%02u", minutes, seconds % 60);
}

void reader_corners_update(void) {
    if (!page_panel || !readtime_panel) return;
    const int reading = !menu_active();

    show_if(page_panel, reading && settings.page_number && page_count > 0);
    if (settings.page_number && page_count > 0) {
        char counter[64];
        snprintf(counter, sizeof(counter), "%d / %d", page + 1, page_count);
        if (strcmp(lv_label_get_text(page_label), counter) != 0) lv_label_set_text(page_label, counter);
    }

    show_if(readtime_panel, reading && settings.readtime);
    if (settings.readtime) {
        char elapsed[32];
        format_readtime((SDL_GetTicks() - readtime_started) / 1000, elapsed, sizeof(elapsed));
        if (strcmp(lv_label_get_text(readtime_label), elapsed) != 0) lv_label_set_text(readtime_label, elapsed);
    }

    if (reading) {
        lv_obj_move_foreground(page_panel);
        lv_obj_move_foreground(readtime_panel);
    }
    dirty = 1;
}

void reader_corners_init(void) {
    page_panel = corner_panel(LV_ALIGN_BOTTOM_LEFT, 4, "page", &page_label);
    readtime_panel = corner_panel(LV_ALIGN_BOTTOM_RIGHT, -4, "playtime", &readtime_label);
    readtime_started = SDL_GetTicks();
}

void reader_header_apply(void) {
    if (menu_active()) return;

    reader_corners_update();

    if (!settings.page_status) {
        lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        dirty = 1;
        return;
    }

    lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);
    show_if(ui_lbl_datetime, header_shows_clock());
    lv_obj_add_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);

    const int battery = header_shows_battery();
    show_if(ui_sta_capacity, battery && (config.visual.battery == 0 || config.visual.battery == 2));
    show_if(ui_lbl_battery_percent, battery && (config.visual.battery == 1 || config.visual.battery == 2));

    if (header_shows_clock()) datetime_task(NULL);
    if (battery) battery_capacity_task(NULL);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_pnl_header);
    dirty = 1;
}

static void compose(void) {
    if (!menu_active()) reader_corners_update();

    view_colour colour = filter_colour;
    colour.brightness = settings.brightness;
    colour.contrast = settings.contrast;
    colour.saturation = settings.saturation;
    colour.hue_shift = settings.hue_shift;
    colour.gamma = settings.gamma;
    colour.sharpness = settings.sharpness;

    clamp_scroll();
    view_compose(has_image ? &image : NULL, scroll_x, scroll_y, settings.theme, plain(), &colour);
    dirty = 1;
}

static doc_view current_view(void) {
    return (doc_view) {reading_width(), reading_height(), settings.fit_page, reader_zoom_steps[settings.zoom],
                       settings.trim};
}

static int render(void) {
    const doc_view view = current_view();
    has_image = doc_render(doc, file_page(page), &view, &image);
    if (!has_image) toast_message(lang.muxreader.page_failed, TOAST_MS);
    return has_image;
}

static void save_position(void) {
    position_pending = 0;
    if (!doc || page_count <= 0) return;

    const int position = doc_position(doc, file_page(page));
    if (position == saved_position && reversed == saved_reversed) return;

    reader_position_save(document_path, position, reversed);
    saved_position = position;
    saved_reversed = reversed;
}

/* Saved a little after the last page turn so a flat battery loses almost nothing */
static void schedule_position(void) {
    position_pending = 1;
    position_deadline = SDL_GetTicks() + POSITION_DELAY;
}

static void reset_auto_turn(const uint32_t now) {
    auto_deadline = now + (uint32_t) reader_auto_turn_steps[settings.auto_turn] * 1000u;
}

static void show_page(const int target, const int at_end) {
    if (page_count <= 0) return;

    const int next = clamp_page(target);
    if (next == page && has_image) return;

    travel = next > page ? 1 : -1;
    view_prepare_transition(travel);
    page = next;
    render();
    scroll_x = 0;
    scroll_y = at_end ? INT_MAX : 0;
    compose();
    schedule_position();
}

static void relayout_keep(void) {
    const int64_t mark = reflowable() ? doc_bookmark(doc, page) : 0;

    layout();
    if (reflowable()) {
        page = doc_bookmark_page(doc, mark);
        forget_toc();
    }
    page = clamp_page(page);

    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void rescale_keep_centre(void) {
    if (!has_image) {
        render();
        compose();
        return;
    }

    const double fx = ((double) scroll_x + reading_width() / 2.0) / (double) image.width;
    const double fy = ((double) scroll_y + reading_height() / 2.0) / (double) image.height;

    if (!render()) return;

    scroll_x = (int) (fx * image.width - reading_width() / 2.0);
    scroll_y = (int) (fy * image.height - reading_height() / 2.0);
    compose();
}

static void change_spacing(const int delta) {
    const int next = settings.line_spacing + delta;
    if (next < 0 || next >= reader_spacing_count) return;

    const int position = doc_position(doc, page);
    settings.line_spacing = next;

    if (!doc_set_line_spacing(doc, reader_spacing_steps[next])) {
        toast_message(lang.muxreader.open_failed, TOAST_MS);
        running = 0;
        return;
    }

    layout();
    forget_toc();
    page = doc_position_page(doc, position);
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void change_size(const int delta) {
    if (reflowable()) {
        const int next = settings.text_size + delta * READER_TEXT_STEP;
        if (next < READER_TEXT_MIN || next > READER_TEXT_MAX) return;
        settings.text_size = next;
        relayout_keep();
    } else {
        const int next = settings.zoom + delta;
        if (next < 0 || next >= reader_zoom_count) return;
        settings.zoom = next;
        rescale_keep_centre();
    }
}

static void change_margin(const int delta) {
    const int next = settings.margin + delta * READER_MARGIN_STEP;
    if (next < 0 || next > READER_MARGIN_MAX) return;
    settings.margin = next;
    relayout_keep();
}

static void cycle_theme(const int delta) {
    settings.theme = (settings.theme + reader_theme_count + delta) % reader_theme_count;
    compose();
}

static void cycle_rotation(const int delta) {
    static const int order[reader_rotation_count] = {
        reader_rotation_none, reader_rotation_right, reader_rotation_upside, reader_rotation_left
    };

    int at = 0;
    for (int i = 0; i < reader_rotation_count; i++)
        if (order[i] == settings.rotation) at = i;

    settings.rotation = order[(at + reader_rotation_count + delta) % reader_rotation_count];
    view_set_rotation(settings.rotation);
    relayout_keep();
}

static void toggle_fit(void) {
    settings.fit_page = !settings.fit_page;
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void cycle_status(const int delta) {
    settings.page_status = (settings.page_status + reader_status_count + delta) % reader_status_count;
    reader_header_apply();
}

static void toggle_trim(void) {
    settings.trim = !settings.trim;
    if (reflowable()) return;

    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

static void cycle_sharpness(const int delta) {
    settings.sharpness = (settings.sharpness + reader_sharpness_count + delta) % reader_sharpness_count;
    compose();
}

static int save_global(void) {
    global_settings.menu_item = settings.menu_item;
    memcpy(global_settings.hotkey, settings.hotkey, sizeof(global_settings.hotkey));
    reader_settings_save(&global_settings);
    return 1;
}

void reader_save_state(void) {
    save_global();
}

static void save_on_exit(void) {
    if (reader_settings_same(&settings, &baseline_settings)) return;

    if (reader_scope_exists(reader_scope_content))
        reader_scope_save(&settings, reader_scope_content);
    else if (reader_scope_exists(reader_scope_directory))
        reader_scope_save(&settings, reader_scope_directory);
    else
        reader_settings_copy_scoped(&global_settings, &settings);
}

static void load_filter_key(const char *key) {
    char wanted[sizeof(config.video.colour_filter)];
    snprintf(wanted, sizeof(wanted), "%s", key);
    snprintf(config.video.colour_filter, sizeof(config.video.colour_filter), "%s", wanted);
    wasabi_assets_refresh(wasabi_asset_filter);

    filter_colour = (view_colour) {0};
    for (int i = 0; i < 9; i++)
        filter_colour.matrix[i] = i % 4 == 0 ? 1.0f : 0.0f;

    const int selected = wasabi_asset_selected(wasabi_asset_filter);
    const char *path = wasabi_asset_path(wasabi_asset_filter, selected);
    if (selected <= 0 || !path || !path[0]) return;

    FILE *file = fopen(path, "r");
    if (!file) return;

    char line[128];
    float parsed[9] = {0};
    int in_matrix = 0;
    int row = 0;
    while (row < 3 && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        if (line[0] == '[') {
            in_matrix = strcmp(line, "[matrix]") == 0;
            continue;
        }
        if (!in_matrix) continue;

        char *cursor = line;
        for (int column = 0; column < 3; column++) {
            char *end = NULL;
            const float value = strtof(cursor, &end);
            if (end == cursor || !isfinite(value)) {
                fclose(file);
                return;
            }
            parsed[row * 3 + column] = value < -2.0f ? -2.0f : value > 2.0f ? 2.0f : value;
            cursor = end;
        }
        row++;
    }
    fclose(file);

    if (row != 3) return;
    memcpy(filter_colour.matrix, parsed, sizeof(parsed));
    filter_colour.filter_active = 1;
}

static void load_filter(void) {
    load_filter_key(settings.filter);
}

static void cycle_filter(const int delta) {
    const int count = wasabi_assets_count(wasabi_asset_filter);
    if (count <= 0) return;

    const int next = (wasabi_asset_selected(wasabi_asset_filter) + count + delta) % count;
    if (!wasabi_asset_select(wasabi_asset_filter, next)) return;

    snprintf(settings.filter, sizeof(settings.filter), "%s", config.video.colour_filter);
    load_filter();
    compose();
}

static int step_value(int *value, const int delta, const int low, const int high) {
    int next = *value + delta * READER_COLOUR_STEP;
    if (next < low) next = low;
    if (next > high) next = high;
    if (next == *value) return 0;

    *value = next;
    compose();
    return 1;
}

static int jump_chapter(const int direction) {
    ensure_toc();

    int target = -1;
    for (int i = 0; i < toc_count; i++) {
        const int at = toc[i].page;
        if (direction > 0 && at > page && (target < 0 || at < target)) target = at;
        if (direction < 0 && at < page && at > target) target = at;
    }

    if (target < 0) return 0;
    show_page(target, 0);
    return 1;
}

static void cycle_auto_turn(const int delta) {
    settings.auto_turn = (settings.auto_turn + reader_auto_turn_count + delta) % reader_auto_turn_count;
    reset_auto_turn(SDL_GetTicks());
}

/* Auto Turn moves on by a screen at a time, so tall pages are scrolled before the next page */
static void advance_screen(void) {
    const int view_h = reading_height();
    if (has_image && scroll_y < image.height - view_h) {
        scroll_y += view_h - view_h / AUTO_OVERLAP;
        compose();
        return;
    }
    if (page < page_count - 1) show_page(page + 1, 0);
}

static void scroll_page(const int delta) {
    if (!has_image) {
        show_page(page + delta, delta < 0);
        return;
    }

    const int view_h = reading_height();
    const int max_y = image.height - view_h;

    if (delta > 0 && scroll_y >= max_y) {
        show_page(page + 1, 0);
        return;
    }
    if (delta < 0 && scroll_y <= 0) {
        show_page(page - 1, 1);
        return;
    }

    int step = view_h / SCROLL_DIV;
    if (step < 1) step = 1;
    scroll_y += delta * step;
    compose();
}

static void pan_page(const int delta) {
    const int view_w = reading_width();
    if (!has_image || image.width <= view_w) {
        show_page(page + delta, 0);
        return;
    }

    int step = view_w / SCROLL_DIV;
    if (step < 1) step = 1;
    scroll_x += delta * step;
    compose();
}

static void publish_inhibit(void) {
    const int want = settings.stay_awake && !menu_active() && running;
    if (want == inhibit_published) return;

    inhibit_published = want;
    if (want) {
        write_text_to_file(IDLE_GAME_INHIBIT, "w", INT, (int) getpid());
    } else if (read_line_int_from(IDLE_GAME_INHIBIT, 1) == (int) getpid()) {
        remove(IDLE_GAME_INHIBIT);
    }
}

static int direction_pressed(const mux_input_type dpad, const mux_input_type ls, const mux_input_type rs) {
    const mux_nav_type nav = get_sticknav_mask(config.settings.advanced.stick_nav);
    return mux_input_pressed(dpad) || ((nav & NAV_LEFT_STICK) && mux_input_pressed(ls))
           || ((nav & NAV_RIGHT_STICK) && mux_input_pressed(rs));
}

static int repeat_step(repeat_state *state, const int down, const uint32_t now) {
    if (!down) {
        state->held = 0;
        return 0;
    }
    if (!state->held) {
        state->held = 1;
        state->next = now + REPEAT_DELAY_MS;
        return 1;
    }
    if (SDL_TICKS_PASSED(now, state->next)) {
        state->next = now + REPEAT_RATE_MS;
        return 1;
    }
    return 0;
}

static direction logical_direction(const direction physical) {
    if (menu_active() || settings.rotation == reader_rotation_none) return physical;

    static const direction right_map[dir_count] = {dir_left, dir_right, dir_down, dir_up};
    static const direction left_map[dir_count] = {dir_right, dir_left, dir_up, dir_down};
    static const direction upside_map[dir_count] = {dir_down, dir_up, dir_right, dir_left};

    if (settings.rotation == reader_rotation_right) return right_map[physical];
    if (settings.rotation == reader_rotation_upside) return upside_map[physical];
    return left_map[physical];
}

static void handle_direction(const direction physical) {
    const direction where = logical_direction(physical);

    if (menu_active()) {
        if (where == dir_up || where == dir_down) menu_move(where == dir_up ? -1 : 1);
        if (where == dir_left || where == dir_right) menu_change(where == dir_left ? -1 : 1);
        return;
    }

    if (where == dir_up) scroll_page(-1);
    if (where == dir_down) scroll_page(1);
    if (where == dir_left) pan_page(-1);
    if (where == dir_right) pan_page(1);
}

static void handle_shoulder(const int delta) {
    if (menu_active())
        menu_shoulder(delta);
    else
        show_page(page + delta, 0);
}

#define HOTKEY_JUMP 10

static void run_hotkey(const reader_hotkey hotkey) {
    static int auto_turn_last = 3;

    switch (hotkey) {
        case reader_hotkey_quit:
            reader_quit();
            return;
        case reader_hotkey_bookmark: {
            const int result = reader_bookmark_toggle();
            toast_message(
                result > 0    ? lang.muxreader.bookmark_added
                : result == 0 ? lang.muxreader.bookmark_removed
                              : lang.muxreader.bookmark_full,
                TOAST_MS
            );
            return;
        }
        case reader_hotkey_filter:
            cycle_filter(1);
            break;
        case reader_hotkey_status:
            cycle_status(1);
            break;
        case reader_hotkey_sharpness:
            cycle_sharpness(1);
            break;
        case reader_hotkey_auto_turn:
            if (settings.auto_turn) auto_turn_last = settings.auto_turn;
            settings.auto_turn = settings.auto_turn ? 0 : auto_turn_last;
            reset_auto_turn(SDL_GetTicks());
            break;
        case reader_hotkey_brightness_down:
        case reader_hotkey_brightness_up:
            reader_step_brightness(hotkey == reader_hotkey_brightness_up ? 1 : -1);
            break;
        case reader_hotkey_contrast_down:
        case reader_hotkey_contrast_up:
            reader_step_contrast(hotkey == reader_hotkey_contrast_up ? 1 : -1);
            break;
        case reader_hotkey_jump_back:
        case reader_hotkey_jump_forward:
            show_page(page + (hotkey == reader_hotkey_jump_forward ? HOTKEY_JUMP : -HOTKEY_JUMP), 0);
            return;
        case reader_hotkey_chapter_back:
        case reader_hotkey_chapter_forward:
            if (!jump_chapter(hotkey == reader_hotkey_chapter_forward ? 1 : -1))
                toast_message(lang.muxreader.no_contents, TOAST_MS);
            return;
        default:
            return;
    }

    play_sound(snd_option);
    menu_hotkey_feedback(hotkey);
}

static int handle_hotkeys(const uint64_t mask, const uint64_t edge) {
    static int armed = 0;
    static int used = 0;

    if (menu_active()) {
        armed = 0;
        return 0;
    }

    if (edge & BIT(mux_input_menu)) {
        armed = 1;
        used = 0;
    }
    if (!armed) return 0;

    if (!(mask & BIT(mux_input_menu))) {
        armed = 0;
        if (!used) {
            play_sound(snd_info_open);
            menu_open();
            dirty = 1;
        }
        return 1;
    }

    for (int hotkey = 0; hotkey < reader_hotkey_count; hotkey++) {
        const int button = settings.hotkey[hotkey];
        if (button < 0 || button == mux_input_menu || !(edge & BIT(button))) continue;
        used = 1;
        run_hotkey((reader_hotkey) hotkey);
        dirty = 1;
    }

    return 1;
}

static void handle_input(void) {
    static uint64_t previous = 0;
    static repeat_state repeats[dir_count];
    static repeat_state repeat_l1;
    static repeat_state repeat_r1;
    static repeat_state repeat_a;
    static repeat_state repeat_b;

    const uint64_t mask = mux_input_pressed_mask();
    const uint64_t edge = mask & ~previous;
    previous = mask;
    const uint32_t now = SDL_GetTicks();

    if (mask) {
        last_activity = now;
        reset_auto_turn(now);
    }

    if (menu_active() && menu_dialogue_input(edge)) {
        dirty = 1;
        return;
    }

    if (handle_hotkeys(mask, edge)) return;

    static uint32_t menu_held_at = 0;
    static int peeking = 0;

    if (menu_active() && menu_held_at) {
        if (mask & BIT(mux_input_menu)) {
            if (!peeking && SDL_TICKS_PASSED(now, menu_held_at + MENU_PEEK_MS)) {
                peeking = 1;
                menu_peek(1);
                dirty = 1;
            }
            return;
        }

        menu_held_at = 0;
        if (peeking) {
            peeking = 0;
            menu_peek(0);
        } else {
            play_sound(snd_info_close);
            menu_close();
        }
        dirty = 1;
        return;
    }

    if (menu_active() && (edge & BIT(mux_input_menu))) {
        menu_held_at = now ? now : 1;
        return;
    }

    if (edge & (BIT(mux_input_menu) | BIT(mux_input_start))) {
        if (menu_active()) {
            play_sound(snd_info_close);
            menu_close();
        } else {
            play_sound(snd_info_open);
            menu_open();
        }
        dirty = 1;
        return;
    }

    const int pressed[dir_count] = {
        direction_pressed(mux_input_dpad_up, mux_input_ls_up, mux_input_rs_up),
        direction_pressed(mux_input_dpad_down, mux_input_ls_down, mux_input_rs_down),
        direction_pressed(mux_input_dpad_left, mux_input_ls_left, mux_input_rs_left),
        direction_pressed(mux_input_dpad_right, mux_input_ls_right, mux_input_rs_right),
    };

    for (int i = 0; i < dir_count; i++)
        if (repeat_step(&repeats[i], pressed[i], now)) handle_direction((direction) i);

    if (repeat_step(&repeat_l1, (mask & BIT(mux_input_l1)) != 0, now)) handle_shoulder(-1);
    if (repeat_step(&repeat_r1, (mask & BIT(mux_input_r1)) != 0, now)) handle_shoulder(1);

    const int a_step = repeat_step(&repeat_a, (mask & BIT(mux_input_a)) != 0, now);
    const int b_step = repeat_step(&repeat_b, (mask & BIT(mux_input_b)) != 0, now);

    if (!menu_active()) {
        if (a_step) show_page(page + 1, 0);
        if (b_step) show_page(page - 1, 0);
        if (edge & BIT(mux_input_l2)) change_size(-1);
        if (edge & BIT(mux_input_r2)) change_size(1);
        if (edge & BIT(mux_input_x)) {
            play_sound(snd_info_open);
            menu_open_contents();
        }
        if ((edge & BIT(mux_input_y)) && !plain()) cycle_theme(1);
        if (edge & BIT(mux_input_select)) cycle_rotation(1);
        return;
    }

    if (edge & BIT(mux_input_a)) {
        play_sound(snd_confirm);
        menu_accept();
        dirty = 1;
        return;
    }

    if ((edge & BIT(mux_input_x)) && menu_action_x()) {
        play_sound(snd_confirm);
        dirty = 1;
        return;
    }

    if ((edge & BIT(mux_input_y)) && menu_action_y()) {
        dirty = 1;
        return;
    }

    if (edge & BIT(mux_input_b)) {
        play_sound(snd_back);
        menu_back();
        dirty = 1;
    }
}

int reader_page(void) {
    return page;
}

int reader_page_count(void) {
    return page_count;
}

int reader_reflowable(void) {
    return reflowable();
}

int reader_plain(void) {
    return plain();
}

const char *reader_title(void) {
    return document_title;
}

void reader_go_to(const int target) {
    show_page(target, 0);
}

int reader_has_toc(void) {
    return toc_loaded ? toc_count > 0 : doc_has_toc(doc);
}

const char *reader_chapter_title(const int at) {
    return chapter_title(at);
}

int reader_toc(const doc_toc_entry **entries) {
    ensure_toc();
    *entries = toc;
    return toc_count;
}

void reader_change_size(const int delta) {
    change_size(delta);
}

void reader_change_margin(const int delta) {
    change_margin(delta);
}

void reader_change_spacing(const int delta) {
    change_spacing(delta);
}

void reader_cycle_theme(const int delta) {
    cycle_theme(delta);
}

void reader_cycle_rotation(const int delta) {
    cycle_rotation(delta);
}

void reader_toggle_fit(void) {
    toggle_fit();
}

void reader_cycle_status(const int delta) {
    cycle_status(delta);
}

void reader_toggle_trim(void) {
    toggle_trim();
}

void reader_cycle_sharpness(const int delta) {
    cycle_sharpness(delta);
}

void reader_cycle_filter(const int delta) {
    cycle_filter(delta);
}

int reader_step_brightness(const int delta) {
    return step_value(&settings.brightness, delta, -100, 100);
}

int reader_step_contrast(const int delta) {
    return step_value(&settings.contrast, delta, 0, 200);
}

int reader_step_saturation(const int delta) {
    return step_value(&settings.saturation, delta, 0, 200);
}

int reader_step_hue(const int delta) {
    return step_value(&settings.hue_shift, delta, -180, 180);
}

int reader_step_gamma(const int delta) {
    return step_value(&settings.gamma, delta, 50, 200);
}

void reader_filter_preview(void) {
    load_filter_key(config.video.colour_filter);
    compose();
}

void reader_filter_restore(void) {
    if (strcmp(config.video.colour_filter, settings.filter) == 0) return;
    load_filter();
    compose();
}

void reader_filter_changed(void) {
    snprintf(settings.filter, sizeof(settings.filter), "%s", config.video.colour_filter);
    load_filter();
    compose();
}

int reader_settings_dirty(void) {
    return !reader_settings_same(&settings, &baseline_settings);
}

static void reapply_settings(void) {
    view_set_rotation(settings.rotation);
    view_set_transition(settings.transition);
    doc_set_font(doc, settings.font);
    doc_set_line_spacing(doc, reader_spacing_steps[settings.line_spacing]);
    load_filter();
    relayout_keep();
    reset_auto_turn(SDL_GetTicks());
}

int reader_settings_store(const reader_save_choice choice) {
    int okay = 1;
    switch (choice) {
        case reader_save_content:
            okay = reader_scope_save(&settings, reader_scope_content);
            break;
        case reader_save_directory:
            okay = reader_scope_save(&settings, reader_scope_directory);
            break;
        case reader_save_global:
            reader_settings_copy_scoped(&global_settings, &settings);
            okay = save_global();
            break;
        case reader_save_session:
            break;
        case reader_save_discard:
            reader_settings_copy_scoped(&settings, &baseline_settings);
            reapply_settings();
            return 1;
    }

    if (okay) baseline_settings = settings;
    return okay;
}

void reader_cycle_auto_turn(const int delta) {
    cycle_auto_turn(delta);
}

int reader_bookmark_count(void) {
    return bookmark_count;
}

int reader_bookmark_page(const int index) {
    if (index < 0 || index >= bookmark_count) return 0;
    return clamp_page(file_page(doc_position_page(doc, bookmarks[index])));
}

int reader_bookmark_find(const int at) {
    for (int i = 0; i < bookmark_count; i++)
        if (reader_bookmark_page(i) == at) return i;
    return -1;
}

static void save_thumbnail(const int bookmark, const int position) {
    if (!has_image) return;
    char path[PATH_MAX];
    if (reader_thumbnail_path(document_path, bookmark, position, path, sizeof(path))) view_save_png(path);
}

int reader_bookmark_thumbnail(const int index, char *path, const size_t size) {
    if (index < 0 || index >= bookmark_count) return 0;
    return reader_thumbnail_path(document_path, 1, bookmarks[index], path, size) && file_exist(path);
}

void reader_bookmark_remove(const int index) {
    if (index < 0 || index >= bookmark_count) return;

    char path[PATH_MAX];
    if (reader_thumbnail_path(document_path, 1, bookmarks[index], path, sizeof(path))) remove(path);

    memmove(&bookmarks[index], &bookmarks[index + 1], (size_t) (bookmark_count - index - 1) * sizeof(*bookmarks));
    bookmark_count--;
    reader_bookmarks_save(document_path, bookmarks, bookmark_count);
}

int reader_bookmark_toggle(void) {
    const int found = reader_bookmark_find(page);
    if (found >= 0) {
        reader_bookmark_remove(found);
        return 0;
    }
    if (bookmark_count >= READER_BOOKMARK_MAX) return -1;

    bookmarks[bookmark_count++] = doc_position(doc, file_page(page));
    reader_bookmarks_save(document_path, bookmarks, bookmark_count);
    save_thumbnail(1, bookmarks[bookmark_count - 1]);
    return 1;
}

void reader_set_font(const char *path) {
    snprintf(settings.font, sizeof(settings.font), "%s", path ? path : "");
    if (!reflowable()) {
        doc_set_font(doc, settings.font);
        return;
    }

    const int position = doc_position(doc, page);
    if (!doc_set_font(doc, settings.font)) {
        toast_message(lang.muxreader.open_failed, TOAST_MS);
        return;
    }

    layout();
    forget_toc();
    page = doc_position_page(doc, position);
    render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

int reader_reversed(void) {
    return reversed;
}

void reader_toggle_reversed(void) {
    if (reflowable() || page_count <= 0) return;

    const int at_start = page == 0;
    const int shown = file_page(page);
    reversed = !reversed;
    forget_toc();
    page = at_start ? 0 : file_page(shown);
    save_position();
    if (at_start) render();
    scroll_x = 0;
    scroll_y = 0;
    compose();
}

void reader_quit(void) {
    running = 0;
}

static void print_help(const char *program) {
    printf("Chutney %s\n\n", CHUTNEY_VERSION);
    printf("Usage: %s <document>\n\n", program);
    printf("Opens PDF, EPUB, CBZ, CBT and TXT documents.\n");
}

static void derive_title(void) {
    if (doc_title(doc, document_title, sizeof(document_title))) return;

    const char *base = strrchr(document_path, '/');
    snprintf(document_title, sizeof(document_title), "%s", base ? base + 1 : document_path);
    char *dot = strrchr(document_title, '.');
    if (dot && dot != document_title) *dot = '\0';
}

static void present(const int redraw) {
    const uint64_t before = display_present_serial();
    lv_task_handler();
    lv_refr_now(NULL);
    if (redraw && display_present_serial() == before) display_composite_frame();
}

static void fail_and_wait(const char *message) {
    toast_message(message, TOAST_MS * 2);
    const uint32_t until = SDL_GetTicks() + TOAST_MS * 2;
    while (!SDL_TICKS_PASSED(SDL_GetTicks(), until) && !terminate_requested) {
        notify_tick();
        present(1);
        SDL_Delay(33);
    }
}

int main(const int argc, char **argv) {
    setlocale(LC_CTYPE, "");

    if (argc != 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0) {
        print_help(argv[0]);
        return argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (strcmp(argv[1], "--version") == 0) {
        printf("Chutney %s\n", CHUTNEY_VERSION);
        return EXIT_SUCCESS;
    }

    snprintf(document_path, sizeof(document_path), "%s", argv[1]);

    load_device(&device);
    load_config(&config);
    init_module("muxreader");
    init_theme(1, 0);
    init_display();
    init_navigation_sound();

    reader_settings_load(&global_settings);
    settings = global_settings;
    reader_scope_open(document_path);
    reader_scope_apply(&settings);
    baseline_settings = settings;
    load_filter();
    view_set_rotation(settings.rotation);
    view_set_transition(settings.transition);

    SDL_Renderer *renderer = display_get_renderer();
    if (!renderer || !view_init(renderer, device.mux.width, device.mux.height)) {
        fprintf(stderr, "Chutney: cannot initialise the display\n");
        return EXIT_FAILURE;
    }

    init_ui_common_screen(&theme, &device, &lang, lang.muxreader.title);
    init_fonts();
    set_gradient_visible(0);
    lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_grid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    menu_init();
    reader_corners_init();
    reader_header_apply();
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_terminate;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    board_init(device.board.name);
    mux_input_open();

    display_set_video_background(view_draw);
    display_set_video_background_opaque(1);
    display_set_ui_hidden(0);

    compose();
    present(1);
    fade_in_screen();

    if (settings.font[0] && !file_exist(settings.font)) settings.font[0] = '\0';

    /* The layout cache keeps the page breaks of an EPUB so it opens without laying out every chapter */
    char accel[PATH_MAX];
    reader_layout_cache_path(document_path, settings.font, accel, sizeof(accel));
    const int accel_missing = !file_exist(accel);

    doc = doc_open_full(document_path, reader_spacing_steps[settings.line_spacing], settings.font, accel);
    if (doc) doc_set_cache(doc, CACHE_SLOTS);
    if (doc && reflowable()) {
        toast_message(lang.muxreader.loading, tst_wait_f);
        present(1);
    }

    if (doc) layout();
    if (doc && reflowable() && page_count > 0 && accel_missing) doc_save_accelerator(doc, accel);

    if (!doc || page_count <= 0) {
        LOG_ERROR(mux_module, "Chutney: unable to open '%s'", document_path);
        fail_and_wait(lang.muxreader.open_failed);
        mux_input_close();
        display_clear_video_background();
        doc_close(doc);
        view_free();
        sdl_cleanup_exit();
        TTF_Quit();
        return EXIT_FAILURE;
    }

    lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    derive_title();
    const int saved = reader_position_load(document_path, &reversed);
    if (reflowable()) reversed = 0;
    page = file_page(doc_position_page(doc, saved));
    saved_position = saved;
    saved_reversed = reversed;
    bookmark_count = reader_bookmarks_load(document_path, bookmarks, READER_BOOKMARK_MAX);
    render();
    compose();

    uint32_t datetime_deadline = 0;
    last_activity = SDL_GetTicks();
    reset_auto_turn(last_activity);

    while (running) {
        if (terminate_requested) running = 0;

        mux_input_poll();
        handle_input();
        wasabi_catalogue_tick();
        menu_tick();
        image_async_tick();
        publish_inhibit();

        const uint32_t now = SDL_GetTicks();
        notify_tick();
        if (SDL_TICKS_PASSED(now, datetime_deadline)) {
            datetime_task(NULL);
            datetime_deadline = now + 1000;
            if (!menu_active()) {
                if (settings.page_status && header_shows_battery()) battery_capacity_task(NULL);
                if (settings.page_status || settings.readtime) dirty = 1;
                if (settings.readtime) reader_corners_update();
            }
        }

        if (menu_active()) reset_auto_turn(now);
        if (settings.auto_turn && !view_animating() && SDL_TICKS_PASSED(now, auto_deadline)) {
            reset_auto_turn(now);
            advance_screen();
        }

        if (position_pending && !view_animating() && SDL_TICKS_PASSED(now, position_deadline)) save_position();

        if (view_tick()) dirty = 1;

        const int redraw = dirty;
        dirty = 0;
        present(redraw);

        /* Once the screen is settled the pages either side are drawn ahead so turning is instant */
        if (!redraw && !menu_active() && !view_animating() && page_count > 1
            && SDL_TICKS_PASSED(SDL_GetTicks(), last_activity + PREFETCH_IDLE)) {
            const doc_view view = current_view();
            const int ahead = clamp_page(page + travel);
            const int behind = clamp_page(page - travel);
            if (!(ahead != page && doc_prefetch(doc, file_page(ahead), &view)) && behind != page)
                doc_prefetch(doc, file_page(behind), &view);
        }

        SDL_Delay(redraw || menu_active() || view_animating() ? 16 : 33);
    }

    save_position();
    save_thumbnail(0, 0);
    save_on_exit();
    save_global();
    if (reflowable()) {
        reader_layout_cache_path(document_path, settings.font, accel, sizeof(accel));
        doc_save_accelerator(doc, accel);
    }
    running = 0;
    publish_inhibit();

    menu_close();
    forget_toc();
    mux_input_close();
    display_clear_video_background();
    doc_close(doc);
    view_free();
    sdl_cleanup_exit();
    TTF_Quit();
    return EXIT_SUCCESS;
}
