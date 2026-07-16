#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../../common/display.h"
#include "../../common/fileio.h"
#include "../../common/input.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/text.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../state/manual.h"

#define MANUAL_FONT_FILE OPT_PATH "share/font/muterm.ttf"

#define MANUAL_FONT_MIN 10
#define MANUAL_FONT_MAX 32
#define MANUAL_FONT_DEF 18

#define MANUAL_PAGE_CHARS_MIN 200

#define MANUAL_SCROLLBAR_WIDTH 4
#define MANUAL_SCROLLBAR_GAP   4
#define MANUAL_SCROLLBAR_MIN_H 6

#define MANUAL_MAX_VISIBLE_ROWS 128

static int active = 0;
static uint64_t prev_nav_mask = 0;

static int prev_x = 0;
static int prev_y = 0;
static int prev_l2 = 0;
static int prev_r2 = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};
static nav_repeat_t rpt_l1 = {0};
static nav_repeat_t rpt_r1 = {0};

static lv_obj_t *ui_canvas_manual = NULL;
static uint8_t *canvas_buf = NULL;
static size_t canvas_buf_cap = 0;

static lv_obj_t *ui_bar_track = NULL;
static lv_obj_t *ui_bar_thumb = NULL;

static TTF_Font *manual_font = NULL;
static int manual_font_size = MANUAL_FONT_DEF;

static char *manual_text = NULL;
static size_t manual_text_len = 0;

static int wrap_enabled = 0;

static size_t *line_offset = NULL;
static size_t *line_length = NULL;

static int line_count = 0;
static int line_cap = 0;
static int current_line = 0;
static int current_col_px = 0;

static size_t current_offset = 0;
static size_t current_page_end = 0;

static uint64_t current_nav_mask(void) {
    uint64_t mask = nav_mask_standard();

    if (mux_input_pressed(mux_input_ls_up)) mask |= BIT(0);
    if (mux_input_pressed(mux_input_ls_down)) mask |= BIT(1);
    if (mux_input_pressed(mux_input_ls_left)) mask |= BIT(2);
    if (mux_input_pressed(mux_input_ls_right)) mask |= BIT(3);

    return mask;
}

static int ensure_font(const int size) {
    if (manual_font && manual_font_size == size) return 1;

    if (!TTF_WasInit() && TTF_Init() != 0) {
        LOG_ERROR(mux_module, "Manual: TTF_Init failed: %s", TTF_GetError());
        return 0;
    }

    TTF_Font *f = TTF_OpenFont(MANUAL_FONT_FILE, size);
    if (!f) {
        LOG_ERROR(mux_module, "Manual: failed to open '%s' at size %d: %s", MANUAL_FONT_FILE, size, TTF_GetError());
        return 0;
    }

    if (manual_font) TTF_CloseFont(manual_font);
    manual_font = f;
    manual_font_size = size;
    return 1;
}

static SDL_Color theme_text_color(void) {
    const uint32_t hex = theme.list_default.text;
    const SDL_Color col = {(hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, 255};
    return col;
}

static int content_wrap_width(void) {
    const int reserved = MANUAL_SCROLLBAR_WIDTH + MANUAL_SCROLLBAR_GAP;
    const int w = lv_obj_get_width(ui_pnl_content) - reserved;
    return w > 0 ? w : lv_obj_get_width(ui_pnl_content);
}

static void free_lines(void) {
    free(line_offset);
    free(line_length);
    line_offset = NULL;
    line_length = NULL;
    line_count = 0;
    line_cap = 0;
}

static int add_line(const size_t offset, const size_t length) {
    if (line_count >= line_cap) {
        const int new_cap = line_cap > 0 ? line_cap * 2 : 256;

        size_t *new_offset_arr = realloc(line_offset, (size_t) new_cap * sizeof(size_t));
        if (!new_offset_arr) return 0;
        line_offset = new_offset_arr;

        size_t *new_length_arr = realloc(line_length, (size_t) new_cap * sizeof(size_t));
        if (!new_length_arr) return 0;
        line_length = new_length_arr;

        line_cap = new_cap;
    }

    line_offset[line_count] = offset;
    line_length[line_count] = length;
    line_count++;
    return 1;
}

static void build_lines(void) {
    free_lines();
    if (!manual_text || manual_text_len == 0) return;

    size_t start = 0;
    for (size_t i = 0; i < manual_text_len; i++) {
        if (manual_text[i] == '\n') {
            if (!add_line(start, i - start)) return;
            start = i + 1;
        }
    }
    add_line(start, manual_text_len - start);
}

static int line_for_offset(const size_t offset) {
    for (int i = line_count - 1; i >= 0; i--) {
        if (line_offset[i] <= offset) return i;
    }

    return 0;
}

static void estimate_metrics(int *out_chars_per_line, int *out_page_chars) {
    const int content_w = content_wrap_width();
    const int content_h = lv_obj_get_height(ui_pnl_content);

    int char_w = 0;
    TTF_SizeUTF8(manual_font, "M", &char_w, NULL);
    if (char_w <= 0) char_w = manual_font_size / 2;

    const int line_h = TTF_FontLineSkip(manual_font);
    int cols = char_w > 0 ? content_w / char_w : 40;
    int rows = line_h > 0 ? content_h / line_h : 20;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (out_chars_per_line) *out_chars_per_line = cols;

    if (out_page_chars) {
        int page_chars = cols * rows;
        if (page_chars < MANUAL_PAGE_CHARS_MIN) page_chars = MANUAL_PAGE_CHARS_MIN;
        *out_page_chars = page_chars;
    }
}

static size_t advance_offset(const size_t from, const int budget) {
    if (from >= manual_text_len) return manual_text_len;

    const size_t end = from + (size_t) budget;
    if (end >= manual_text_len) return manual_text_len;

    size_t split = end;
    while (split > from && manual_text[split] != '\n' && manual_text[split] != ' ')
        split--;
    if (split == from) split = end;

    return split + 1;
}

static size_t retreat_offset(const size_t from, const int budget) {
    if (from == 0) return 0;

    const size_t target = (size_t) budget < from ? from - (size_t) budget : 0;
    if (target == 0) return 0;

    size_t split = target;
    while (split > 0 && manual_text[split] != '\n' && manual_text[split] != ' ')
        split--;

    return split > 0 ? split + 1 : 0;
}

static void blit_surface_swap_rb(
    const SDL_Surface *surf, uint8_t *dst, const int dst_stride, const int dst_x, const int dst_y, const int src_x0,
    const int copy_w, const int copy_h
) {
    for (int y = 0; y < copy_h; y++) {
        const uint8_t *src_row = (const uint8_t *) surf->pixels + (size_t) y * surf->pitch + (size_t) src_x0 * 4;
        uint8_t *dst_row = dst + (size_t) (dst_y + y) * (size_t) dst_stride + (size_t) dst_x * 4;

        for (int x = 0; x < copy_w; x++) {
            dst_row[x * 4 + 0] = src_row[x * 4 + 2];
            dst_row[x * 4 + 1] = src_row[x * 4 + 1];
            dst_row[x * 4 + 2] = src_row[x * 4 + 0];
            dst_row[x * 4 + 3] = src_row[x * 4 + 3];
        }
    }
}

static void update_scrollbar(void) {
    if (!ui_bar_thumb) return;

    const int content_h = lv_obj_get_height(ui_pnl_content);
    double frac_start = 0.0;
    double frac_span = 1.0;

    if (wrap_enabled) {
        if (manual_text_len == 0) return;

        const double doc_len = (double) manual_text_len;
        frac_start = (double) current_offset / doc_len;
        frac_span = (double) (current_page_end > current_offset ? current_page_end - current_offset : 1) / doc_len;
    } else {
        if (line_count == 0) return;

        const int line_h = manual_font ? TTF_FontLineSkip(manual_font) : 0;
        int rows = line_h > 0 ? content_h / line_h : 1;
        if (rows < 1) rows = 1;

        frac_start = (double) current_line / (double) line_count;
        frac_span = (double) rows / (double) line_count;
    }

    lv_coord_t thumb_h = (lv_coord_t) (frac_span * content_h);
    if (thumb_h < MANUAL_SCROLLBAR_MIN_H) thumb_h = MANUAL_SCROLLBAR_MIN_H;
    if (thumb_h > content_h) thumb_h = content_h;

    lv_coord_t thumb_y = (lv_coord_t) (frac_start * content_h);
    if (thumb_y + thumb_h > content_h) thumb_y = content_h - thumb_h;
    if (thumb_y < 0) thumb_y = 0;

    lv_obj_set_height(ui_bar_thumb, thumb_h);
    lv_obj_align(ui_bar_thumb, LV_ALIGN_TOP_RIGHT, 0, thumb_y);
}

static void render_wrap_view(void) {
    if (!ui_canvas_manual || !manual_font || !manual_text || manual_text_len == 0) return;

    int page_chars = 0;
    estimate_metrics(NULL, &page_chars);

    const int wrap_w = content_wrap_width();
    const SDL_Color col = theme_text_color();

    const size_t end = advance_offset(current_offset, page_chars);
    const size_t len = end > current_offset ? end - current_offset : 0;

    char *page_text = malloc(len + 1);
    if (!page_text) return;

    memcpy(page_text, manual_text + current_offset, len);
    page_text[len] = '\0';

    SDL_Surface *surf = sdl_render_text_wrapped_surface(manual_font, page_text, col, wrap_w, 0);
    free(page_text);
    if (!surf) return;

    current_page_end = end;

    const int w = surf->w > 0 ? surf->w : 1;
    const int h = surf->h > 0 ? surf->h : 1;
    const size_t needed = (size_t) w * (size_t) h * 4;

    if (needed > canvas_buf_cap) {
        free(canvas_buf);
        canvas_buf = malloc(needed);
        canvas_buf_cap = canvas_buf ? needed : 0;
    }

    if (!canvas_buf) {
        SDL_FreeSurface(surf);
        return;
    }

    blit_surface_swap_rb(surf, canvas_buf, w * 4, 0, 0, 0, w, h);
    SDL_FreeSurface(surf);

    lv_canvas_set_buffer(ui_canvas_manual, canvas_buf, w, h, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_size(ui_canvas_manual, w, h);
    canvas_invalidate_gpu_texture(ui_canvas_manual);

    update_scrollbar();
}

static void render_nowrap_view(void) {
    if (!ui_canvas_manual || !manual_font || line_count == 0) return;

    const int content_w = content_wrap_width();
    const int content_h = lv_obj_get_height(ui_pnl_content);
    const int line_h = TTF_FontLineSkip(manual_font);
    if (line_h <= 0 || content_w <= 0) return;

    int rows = content_h / line_h;
    if (rows < 1) rows = 1;
    if (rows > MANUAL_MAX_VISIBLE_ROWS) rows = MANUAL_MAX_VISIBLE_ROWS;

    if (current_line >= line_count) current_line = line_count - 1;
    if (current_line < 0) current_line = 0;

    const SDL_Color col = theme_text_color();

    SDL_Surface *line_surf[MANUAL_MAX_VISIBLE_ROWS] = {0};
    int max_w = 0;
    int actual_rows = 0;

    for (int row = 0; row < rows; row++) {
        const int line_idx = current_line + row;
        if (line_idx >= line_count) break;

        actual_rows = row + 1;

        const size_t off = line_offset[line_idx];
        const size_t len = line_length[line_idx];
        if (len == 0) continue;

        char *line_text = malloc(len + 1);
        if (!line_text) continue;

        memcpy(line_text, manual_text + off, len);
        line_text[len] = '\0';

        line_surf[row] = sdl_render_text_surface(manual_font, line_text, col, 0);
        free(line_text);

        if (line_surf[row] && line_surf[row]->w > max_w) max_w = line_surf[row]->w;
    }

    const int max_col_px = max_w > content_w ? max_w - content_w : 0;
    if (current_col_px > max_col_px) current_col_px = max_col_px;
    if (current_col_px < 0) current_col_px = 0;

    const int canvas_h = actual_rows > 0 ? actual_rows * line_h : line_h;
    const size_t needed = (size_t) content_w * (size_t) canvas_h * 4;

    if (needed > canvas_buf_cap) {
        free(canvas_buf);
        canvas_buf = malloc(needed);
        canvas_buf_cap = canvas_buf ? needed : 0;
    }

    if (!canvas_buf) {
        for (int row = 0; row < actual_rows; row++)
            SDL_FreeSurface(line_surf[row]);
        return;
    }

    memset(canvas_buf, 0, needed);

    for (int row = 0; row < actual_rows; row++) {
        SDL_Surface *ls = line_surf[row];
        if (!ls) continue;

        const int src_x0 = current_col_px < ls->w ? current_col_px : ls->w;
        int copy_w = ls->w - src_x0;
        if (copy_w > content_w) copy_w = content_w;
        const int copy_h = ls->h < line_h ? ls->h : line_h;

        if (copy_w > 0) blit_surface_swap_rb(ls, canvas_buf, content_w * 4, 0, row * line_h, src_x0, copy_w, copy_h);

        SDL_FreeSurface(ls);
    }

    lv_canvas_set_buffer(ui_canvas_manual, canvas_buf, content_w, canvas_h, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_size(ui_canvas_manual, content_w, canvas_h);
    canvas_invalidate_gpu_texture(ui_canvas_manual);

    update_scrollbar();
}

static void render_view(void) {
    if (wrap_enabled) {
        render_wrap_view();
    } else {
        render_nowrap_view();
    }
}

static void move_line(const int delta) {
    if (wrap_enabled) {
        int chars_per_line = 0;
        estimate_metrics(&chars_per_line, NULL);
        if (chars_per_line < 1) chars_per_line = 1;

        current_offset =
            delta > 0 ? advance_offset(current_offset, chars_per_line) : retreat_offset(current_offset, chars_per_line);
    } else {
        current_line += delta;
        if (current_line < 0) current_line = 0;
        if (current_line >= line_count) current_line = line_count > 0 ? line_count - 1 : 0;
    }

    render_view();
}

static void move_page(const int delta) {
    if (wrap_enabled) {
        int page_chars = 0;
        estimate_metrics(NULL, &page_chars);

        current_offset =
            delta > 0 ? advance_offset(current_offset, page_chars) : retreat_offset(current_offset, page_chars);
    } else {
        const int line_h = manual_font ? TTF_FontLineSkip(manual_font) : 0;
        const int content_h = lv_obj_get_height(ui_pnl_content);
        int rows = line_h > 0 ? content_h / line_h : 1;
        if (rows < 1) rows = 1;

        current_line += delta * rows;
        if (current_line < 0) current_line = 0;
        if (current_line >= line_count) current_line = line_count > 0 ? line_count - 1 : 0;
    }

    render_view();
}

static void move_col(const int delta) {
    if (wrap_enabled) return;

    int char_w = 0;
    if (manual_font) TTF_SizeUTF8(manual_font, "M", &char_w, NULL);
    if (char_w <= 0) char_w = manual_font_size / 2;
    if (char_w < 1) char_w = 1;

    current_col_px += delta * char_w;
    if (current_col_px < 0) current_col_px = 0;

    render_view();
}

static void jump_top(void) {
    if (wrap_enabled) {
        current_offset = 0;
    } else {
        current_line = 0;
        current_col_px = 0;
    }

    render_view();
}

static void toggle_wrap(void) {
    const size_t canonical_offset = wrap_enabled ? current_offset : line_count > 0 ? line_offset[current_line] : 0;

    wrap_enabled = !wrap_enabled;

    if (wrap_enabled) {
        current_offset = canonical_offset;
    } else {
        current_line = line_for_offset(canonical_offset);
        current_col_px = 0;
    }

    render_view();
}

static void change_font_size(const int delta) {
    if (!ensure_font(manual_font_size + delta)) return;

    render_view();
}

static void create_scrollbar(void) {
    ui_bar_track = lv_obj_create(ui_pnl_content);
    lv_obj_remove_style_all(ui_bar_track);
    lv_obj_set_size(ui_bar_track, MANUAL_SCROLLBAR_WIDTH, LV_PCT(100));
    lv_obj_align(ui_bar_track, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(ui_bar_track, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_track, 40, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_track, MANUAL_SCROLLBAR_WIDTH / 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(ui_bar_track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_bar_track, LV_OBJ_FLAG_IGNORE_LAYOUT);

    ui_bar_thumb = lv_obj_create(ui_pnl_content);
    lv_obj_remove_style_all(ui_bar_thumb);
    lv_obj_set_width(ui_bar_thumb, MANUAL_SCROLLBAR_WIDTH);
    lv_obj_set_style_bg_color(ui_bar_thumb, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_thumb, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_thumb, MANUAL_SCROLLBAR_WIDTH / 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(ui_bar_thumb, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_bar_thumb, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

static void build_txt_view(void) {
    lv_obj_clean(ui_pnl_content);

    ui_canvas_manual = lv_canvas_create(ui_pnl_content);
    lv_obj_clear_flag(ui_canvas_manual, LV_OBJ_FLAG_SCROLLABLE);

    create_scrollbar();

    wrap_enabled = manual_load_wrap_enabled();

    int loaded_size = manual_load_font_size(MANUAL_FONT_DEF);
    if (loaded_size < MANUAL_FONT_MIN) loaded_size = MANUAL_FONT_MIN;
    if (loaded_size > MANUAL_FONT_MAX) loaded_size = MANUAL_FONT_MAX;
    ensure_font(loaded_size);

    free(manual_text);
    manual_text = read_all_char_from(manual_get_path());
    manual_text_len = manual_text ? strlen(manual_text) : 0;

    build_lines();

    size_t start_offset = (size_t) manual_load_position();
    if (start_offset >= manual_text_len) start_offset = 0;

    if (wrap_enabled) {
        current_offset = start_offset;
    } else {
        current_line = line_for_offset(start_offset);
        current_col_px = 0;
    }

    render_view();
}

static void close_manual(void) {
    const size_t canonical_offset = wrap_enabled ? current_offset : line_count > 0 ? line_offset[current_line] : 0;
    manual_save_position((int) canonical_offset);
    manual_save_font_size(manual_font_size);
    manual_save_wrap_enabled(wrap_enabled);

    lv_obj_clean(ui_pnl_content);
    ui_canvas_manual = NULL;
    ui_bar_track = NULL;
    ui_bar_thumb = NULL;

    free(manual_text);
    manual_text = NULL;
    manual_text_len = 0;

    free_lines();

    current_offset = 0;
    current_page_end = 0;
    current_line = 0;
    current_col_px = 0;

    active = 0;

    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    pause_menu_show_nav_hints();
    pause_menu_toggle();
}

void manual_menu_init(void) {
}

void manual_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();
    prev_x = mux_input_pressed(mux_input_x);
    prev_y = mux_input_pressed(mux_input_y);
    prev_l2 = mux_input_pressed(mux_input_l2);
    prev_r2 = mux_input_pressed(mux_input_r2);

    build_txt_view();

    nav_show_a(0, "");
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.top, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.muxretro.manual_screen.wrap, 0},
                                  {NULL, NULL, 0}});

    pause_menu_fix_nav_order();
}

int manual_menu_is_active(void) {
    return active;
}

void manual_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (edge & BIT(5)) {
        play_sound(snd_back);
        close_manual();
        return;
    }

    const int x_now = mux_input_pressed(mux_input_x);
    const int x_edge = x_now && !prev_x;
    prev_x = x_now;

    const int y_now = mux_input_pressed(mux_input_y);
    const int y_edge = y_now && !prev_y;
    prev_y = y_now;

    const int l2_now = mux_input_pressed(mux_input_l2);
    const int l2_edge = l2_now && !prev_l2;
    prev_l2 = l2_now;

    const int r2_now = mux_input_pressed(mux_input_r2);
    const int r2_edge = r2_now && !prev_r2;
    prev_r2 = r2_now;

    if (x_edge) {
        play_sound(snd_option);
        jump_top();
        return;
    }

    if (y_edge) {
        play_sound(snd_option);
        toggle_wrap();
        return;
    }

    if (l2_edge && manual_font_size > MANUAL_FONT_MIN) {
        change_font_size(-1);
        return;
    }

    if (r2_edge && manual_font_size < MANUAL_FONT_MAX) {
        change_font_size(1);
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int do_down = nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int do_left = nav_repeat_step(&rpt_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int do_l1 = nav_repeat_step(&rpt_l1, edge & NAV_PAGE_UP_BIT, mask & NAV_PAGE_UP_BIT, 1, now);
    const int do_r1 = nav_repeat_step(&rpt_r1, edge & NAV_PAGE_DOWN_BIT, mask & NAV_PAGE_DOWN_BIT, 1, now);

    if (do_up) {
        move_line(-1);
    } else if (do_down) {
        move_line(1);
    } else if (do_left) {
        move_col(-1);
    } else if (do_right) {
        move_col(1);
    } else if (do_l1) {
        move_page(-1);
    } else if (do_r1) {
        move_page(1);
    }
}
