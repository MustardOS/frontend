#pragma once

#include <stddef.h>
#include <SDL2/SDL.h>

typedef struct {
    Uint32 codepoint;
    Uint8 width;
    SDL_Color fg;
    SDL_Color bg;
    Uint8 style;
} Cell;

enum {
    STYLE_BOLD = 1 << 0,
    STYLE_UNDERLINE = 1 << 1,
    STYLE_REVERSE = 1 << 2,
    STYLE_ITALIC = 1 << 3,
    STYLE_DIM = 1 << 4,
    STYLE_STRIKE = 1 << 5,
};

typedef void (*vt_title_cb_t)(const char *title, void *userdata);

int vt_init(int cols, int rows, int scrollback_capacity);

void vt_resize(int new_cols, int new_rows);

void vt_reset(void);

void vt_free(void);

int vt_cols(void);

int vt_rows(void);

int vt_cursor_row(void);

int vt_cursor_col(void);

int vt_cursor_visible(void);

int vt_cursor_keys_app(void);

int vt_scrollback_count(void);

const Cell *vt_scrollback_row(int idx);

Cell *vt_cell(int row, int col);

int vt_scroll_offset(void);

void vt_scroll_set(int offset);

void vt_scroll_adjust(int delta, int max_visible_rows);

int vt_is_dirty(void);

void vt_clear_dirty(void);

int vt_row_is_dirty(int row);

void vt_clear_row_dirty(int row);

void vt_mark_all_rows_dirty(void);

void vt_mark_cursor_row_dirty(void);

int vt_using_alt_screen(void);

int vt_feed(const char *buf, size_t len);

void vt_set_title_callback(vt_title_cb_t cb, void *userdata);

int vt_scrollback_save(const char *path);

int vt_scrollback_load(const char *path);
