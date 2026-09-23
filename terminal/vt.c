#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vt.h"
#include "sixel.h"

#define SB_MAGIC 0x4D545342u

static Cell *screen_buf = NULL;
static Cell *main_screen_buf = NULL;
static Cell *alt_screen_buf = NULL;

static int using_alt_screen = 0;

static int saved_main_row = 0;
static int saved_main_col = 0;

static int vt_g0_charset = 0;
static int vt_g1_charset = 0;
static int vt_gl_charset = 0;

static unsigned char esc_pending[128];
static size_t esc_pending_len = 0;

static unsigned char *dcs_buf = NULL;
static size_t dcs_len = 0;
static size_t dcs_cap = 0;
static int dcs_active = 0;
static int dcs_cell_x = 0;
static int dcs_cell_y = 0;

static int cursor_keys_application = 0;
static int linefeed_mode = 0;
static int autowrap_mode = 1;

static int scroll_top = 0;
static int scroll_bottom = 0;

static int TERM_COLS = 0;
static int TERM_ROWS = 0;

static int cursor_row = 0;
static int cursor_col = 0;
static int cursor_vis = 1;

static int prev_cursor_row = 0;
static int prev_cursor_col = 0;

static int saved_row = 0;
static int saved_col = 0;

static SDL_Color default_fg = {255, 255, 255, 255};
static SDL_Color default_bg = {0, 0, 0, 255};
static SDL_Color current_fg;
static SDL_Color current_bg;
static Uint8 current_style = 0;

static int screen_dirty = 1;

static Uint8 *row_dirty = NULL;

static Cell *scrollback = NULL;
static int sb_capacity = 512;
static int sb_count = 0;
static int sb_head = 0;
static int scroll_offset = 0;

static vt_title_cb_t g_title_cb = NULL;
static void *g_title_userdata = NULL;

static const SDL_Color base_colours[8] = {
    {0, 0, 0, 255},   {170, 0, 0, 255},   {0, 170, 0, 255},   {170, 85, 0, 255},
    {0, 0, 170, 255}, {170, 0, 170, 255}, {0, 170, 170, 255}, {170, 170, 170, 255},
};

static const SDL_Color bright_colours[8] = {
    {85, 85, 85, 255},  {255, 85, 85, 255},  {85, 255, 85, 255},  {255, 255, 85, 255},
    {85, 85, 255, 255}, {255, 85, 255, 255}, {85, 255, 255, 255}, {255, 255, 255, 255},
};

static const Uint8 cube6[6] = {0, 95, 135, 175, 215, 255};

static inline Cell *CELL(int r, int c) {
    return &screen_buf[(size_t) r * (size_t) TERM_COLS + (size_t) c];
}

static inline int row_in_scroll_region(int row) {
    return row >= scroll_top && row <= scroll_bottom;
}

static Uint32 vt_map_acs(Uint32 cp) {
    switch (cp) {
        case '`':
            return 0x25C6;
        case 'a':
            return 0x2592;
        case 'b':
            return 0x2409;
        case 'c':
            return 0x240C;
        case 'd':
            return 0x240D;
        case 'e':
            return 0x240A;
        case 'f':
            return 0x00B0;
        case 'g':
            return 0x00B1;
        case 'h':
            return 0x2424;
        case 'i':
            return 0x240B;
        case 'j':
            return 0x2518;
        case 'k':
            return 0x2510;
        case 'l':
            return 0x250C;
        case 'm':
            return 0x2514;
        case 'n':
            return 0x253C;
        case 'o':
            return 0x23BA;
        case 'p':
            return 0x23BB;
        case 'q':
            return 0x2500;
        case 'r':
            return 0x23BC;
        case 's':
            return 0x23BD;
        case 't':
            return 0x251C;
        case 'u':
            return 0x2524;
        case 'v':
            return 0x2534;
        case 'w':
            return 0x252C;
        case 'x':
            return 0x2502;
        case 'y':
            return 0x2264;
        case 'z':
            return 0x2265;
        case '{':
            return 0x03C0;
        case '|':
            return 0x2260;
        case '}':
            return 0x00A3;
        case '~':
            return 0x00B7;
        default:
            return cp;
    }
}

static inline int vt_gl_is_graphics(void) {
    return vt_gl_charset ? (vt_g1_charset == 1) : (vt_g0_charset == 1);
}

static inline Uint32 vt_apply_graphics_charset(Uint32 cp) {
    if (cp < 0x20 || cp > 0x7E || !vt_gl_is_graphics()) return cp;
    return vt_map_acs(cp);
}

static void vt_designate_charset(int which, char final) {
    int mode = (final == '0') ? 1 : 0;
    if (which == 0) {
        vt_g0_charset = mode;
    } else {
        vt_g1_charset = mode;
    }
}

static void vt_shift_in(void) {
    vt_gl_charset = 0;
}

static void vt_shift_out(void) {
    vt_gl_charset = 1;
}

static inline void reset_cell(Cell *c) {
    c->codepoint = (Uint32) ' ';
    c->width = 1;
    c->fg = current_fg;
    c->bg = current_bg;
    c->style = 0;
}

static inline void mark_row_dirty(int r) {
    if (row_dirty && r >= 0 && r < TERM_ROWS) row_dirty[r] = 1;
}

static void clear_cells(Cell *buf) {
    size_t total = (size_t) TERM_ROWS * (size_t) TERM_COLS;

    for (size_t i = 0; i < total; i++) {
        buf[i].codepoint = (Uint32) ' ';
        buf[i].width = 1;
        buf[i].fg = current_fg;
        buf[i].bg = current_bg;
        buf[i].style = 0;
    }

    if (row_dirty) memset(row_dirty, 1, (size_t) TERM_ROWS);
}

static void clear_screen(void) {
    size_t total = (size_t) TERM_ROWS * (size_t) TERM_COLS;
    for (size_t i = 0; i < total; i++)
        reset_cell(&screen_buf[i]);

    if (row_dirty) memset(row_dirty, 1, (size_t) TERM_ROWS);

    sixel_free();
}

static void clear_row_range(int row, int start_col, int end_col) {
    if (row < 0 || row >= TERM_ROWS) return;
    if (start_col < 0) start_col = 0;
    if (end_col >= TERM_COLS) end_col = TERM_COLS - 1;
    if (start_col > end_col) return;

    for (int c = start_col; c <= end_col; c++)
        reset_cell(CELL(row, c));

    mark_row_dirty(row);
}

static void set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= TERM_ROWS) row = TERM_ROWS - 1;

    if (col < 0) col = 0;
    if (col >= TERM_COLS) col = TERM_COLS - 1;

    if (row != cursor_row || col != cursor_col) {
        mark_row_dirty(cursor_row);
        mark_row_dirty(row);

        screen_dirty = 1;

        prev_cursor_row = cursor_row;
        prev_cursor_col = cursor_col;
    }

    cursor_row = row;
    cursor_col = col;
}

static void scrollback_push(const Cell *row) {
    memcpy(&scrollback[(size_t) sb_head * (size_t) TERM_COLS], row, (size_t) TERM_COLS * sizeof(Cell));
    sb_head = (sb_head + 1) % sb_capacity;

    if (sb_count < sb_capacity) sb_count++;
}

static void scroll_region_up(int top, int bottom, int lines, int allow_scrollback) {
    if (TERM_ROWS <= 1) return;
    if (top < 0) top = 0;
    if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
    if (top >= bottom || lines <= 0) return;

    int height = bottom - top + 1;
    if (lines > height) lines = height;

    if (allow_scrollback && !using_alt_screen && top == 0 && bottom == TERM_ROWS - 1) {
        for (int i = 0; i < lines; i++)
            scrollback_push(CELL(top + i, 0));
        sixel_scroll(lines);
    }

    if (lines < height)
        memmove(CELL(top, 0), CELL(top + lines, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (height - lines));

    for (int r = bottom - lines + 1; r <= bottom; r++) {
        clear_row_range(r, 0, TERM_COLS - 1);
    }

    if (row_dirty)
        for (int r = top; r <= bottom; r++)
            row_dirty[r] = 1;
}

static void scroll_region_down(int top, int bottom, int lines) {
    if (TERM_ROWS <= 1) return;
    if (top < 0) top = 0;
    if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
    if (top >= bottom || lines <= 0) return;

    int height = bottom - top + 1;
    if (lines > height) lines = height;

    if (lines < height)
        memmove(CELL(top + lines, 0), CELL(top, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (height - lines));

    for (int r = top; r < top + lines; r++) {
        clear_row_range(r, 0, TERM_COLS - 1);
    }

    if (row_dirty)
        for (int r = top; r <= bottom; r++)
            row_dirty[r] = 1;
}

static void vt_index(void) {
    mark_row_dirty(cursor_row);

    if (row_in_scroll_region(cursor_row)) {
        if (cursor_row == scroll_bottom) {
            scroll_region_up(scroll_top, scroll_bottom, 1, 1);
        } else {
            cursor_row++;
        }
        return;
    }

    if (cursor_row < TERM_ROWS - 1) {
        cursor_row++;
    } else if (scroll_top == 0 && scroll_bottom == TERM_ROWS - 1) {
        scroll_region_up(0, TERM_ROWS - 1, 1, 1);
    }
}

static void vt_reverse_index(void) {
    mark_row_dirty(cursor_row);

    if (row_in_scroll_region(cursor_row)) {
        if (cursor_row == scroll_top) {
            scroll_region_down(scroll_top, scroll_bottom, 1);
        } else {
            cursor_row--;
        }
        return;
    }

    if (cursor_row > 0) {
        cursor_row--;
    } else if (scroll_top == 0 && scroll_bottom == TERM_ROWS - 1) {
        scroll_region_down(0, TERM_ROWS - 1, 1);
    }
}

static void vt_newline(void) {
    if (linefeed_mode) cursor_col = 0;
    vt_index();
}

static void vt_reset_state(void) {
    cursor_keys_application = 0;
    linefeed_mode = 0;
    autowrap_mode = 1;

    scroll_top = 0;
    scroll_bottom = TERM_ROWS - 1;

    cursor_vis = 1;

    saved_row = 0;
    saved_col = 0;

    saved_main_row = 0;
    saved_main_col = 0;

    current_fg = default_fg;
    current_bg = default_bg;

    current_style = 0;

    vt_g0_charset = 0;
    vt_g1_charset = 0;
    vt_gl_charset = 0;

    screen_buf = main_screen_buf;
    using_alt_screen = 0;

    if (main_screen_buf) clear_cells(main_screen_buf);
    if (alt_screen_buf) clear_cells(alt_screen_buf);

    set_cursor(0, 0);
    scroll_offset = 0;
    screen_dirty = 1;

    dcs_len = 0;
    dcs_active = 0;

    sixel_free();
}

void vt_reset(void) {
    vt_reset_state();
    sb_count = 0;
    sb_head = 0;
    esc_pending_len = 0;
    if (scrollback) memset(scrollback, 0, (size_t) sb_capacity * (size_t) TERM_COLS * sizeof(Cell));
    if (row_dirty) memset(row_dirty, 1, (size_t) TERM_ROWS);
}

static void vt_enter_alt_screen(void) {
    if (using_alt_screen) return;

    saved_main_row = cursor_row;
    saved_main_col = cursor_col;

    screen_buf = alt_screen_buf;
    using_alt_screen = 1;

    clear_cells(screen_buf);

    cursor_row = 0;
    cursor_col = 0;

    scroll_offset = 0;
    screen_dirty = 1;

    sixel_free();
}

static void vt_leave_alt_screen(void) {
    if (!using_alt_screen) return;
    screen_buf = main_screen_buf;

    using_alt_screen = 0;
    cursor_row = saved_main_row;
    cursor_col = saved_main_col;

    scroll_offset = 0;
    screen_dirty = 1;

    if (row_dirty) memset(row_dirty, 1, (size_t) TERM_ROWS);

    sixel_free();
}

static SDL_Color colour_from_256(int idx) {
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;

    if (idx < 8) return base_colours[idx];
    if (idx < 16) return bright_colours[idx - 8];

    if (idx < 232) {
        int v = idx - 16;
        int ri = v / 36;
        int gi = (v % 36) / 6;
        int bi = v % 6;
        return (SDL_Color) {cube6[ri], cube6[gi], cube6[bi], 255};
    }

    Uint8 g = (Uint8) (8 + 10 * (idx - 232));
    return (SDL_Color) {g, g, g, 255};
}

static void apply_sgr(int *params, int count) {
    if (count == 0) {
        current_fg = default_fg;
        current_bg = default_bg;
        current_style = 0;
        return;
    }

    for (int i = 0; i < count; i++) {
        int p = params[i];
        switch (p) {
            case 0:
                current_fg = default_fg;
                current_bg = default_bg;
                current_style = 0;
                break;
            case 1:
                current_style |= STYLE_BOLD;
                break;
            case 2:
                current_style |= STYLE_DIM;
                break;
            case 3:
                current_style |= STYLE_ITALIC;
                break;
            case 4:
                current_style |= STYLE_UNDERLINE;
                break;
            case 7:
                current_style |= STYLE_REVERSE;
                break;
            case 9:
                current_style |= STYLE_STRIKE;
                break;
            case 22:
                current_style &= (Uint8) ~(STYLE_BOLD | STYLE_DIM);
                break;
            case 23:
                current_style &= (Uint8) ~STYLE_ITALIC;
                break;
            case 24:
                current_style &= (Uint8) ~STYLE_UNDERLINE;
                break;
            case 27:
                current_style &= (Uint8) ~STYLE_REVERSE;
                break;
            case 29:
                current_style &= (Uint8) ~STYLE_STRIKE;
                break;
            case 39:
                current_fg = default_fg;
                break;
            case 49:
                current_bg = default_bg;
                break;
            default:
                if (p >= 30 && p <= 37) {
                    current_fg = (current_style & STYLE_BOLD) ? bright_colours[p - 30] : base_colours[p - 30];
                } else if (p >= 40 && p <= 47) {
                    current_bg = base_colours[p - 40];
                } else if (p >= 90 && p <= 97) {
                    current_fg = bright_colours[p - 90];
                } else if (p >= 100 && p <= 107) {
                    current_bg = bright_colours[p - 100];
                } else if (p == 38) {
                    if (i + 1 < count && params[i + 1] == 5 && i + 2 < count) {
                        current_fg = colour_from_256(params[i + 2]);
                        i += 2;
                    } else if (i + 1 < count && params[i + 1] == 2 && i + 4 < count) {
                        current_fg =
                            (SDL_Color) {(Uint8) params[i + 2], (Uint8) params[i + 3], (Uint8) params[i + 4], 255};
                        i += 4;
                    }
                } else if (p == 48) {
                    if (i + 1 < count && params[i + 1] == 5 && i + 2 < count) {
                        current_bg = colour_from_256(params[i + 2]);
                        i += 2;
                    } else if (i + 1 < count && params[i + 1] == 2 && i + 4 < count) {
                        current_bg =
                            (SDL_Color) {(Uint8) params[i + 2], (Uint8) params[i + 3], (Uint8) params[i + 4], 255};
                        i += 4;
                    }
                }
                break;
        }
    }
}

static void vt_handle_csi_private(int *params, int count, int set) {
    for (int i = 0; i < count; i++) {
        int mode = params[i];

        switch (mode) {
            case 1:
                cursor_keys_application = set;
                break;
            case 7:
                autowrap_mode = set;
                break;
            case 25:
                cursor_vis = set;
                break;
            case 47:
            case 1047:
                if (set)
                    vt_enter_alt_screen();
                else
                    vt_leave_alt_screen();
                break;
            case 1048:
                if (set) {
                    saved_row = cursor_row;
                    saved_col = cursor_col;
                } else {
                    set_cursor(saved_row, saved_col);
                }
                break;
            case 1049:
                if (set) {
                    saved_main_row = cursor_row;
                    saved_main_col = cursor_col;
                    vt_enter_alt_screen();
                } else {
                    vt_leave_alt_screen();
                    set_cursor(saved_main_row, saved_main_col);
                }
                break;
            case 2004:
            case 2026:
            default:
                break;
        }
    }
}

static inline int is_csi_final(unsigned char ch) {
    return ch >= 0x40 && ch <= 0x7E;
}

static inline int parse_int_fast(const char **p) {
    int v = 0;

    while (**p >= '0' && **p <= '9') {
        v = v * 10 + (**p - '0');
        (*p)++;
    }

    return v;
}

static void parse_csi(const char *seq) {
    if (seq[0] != '[') return;

    int params[16] = {0};
    int count = 0;

    int is_private = 0;
    const char *p = seq + 1;

    if (*p == '?') {
        is_private = 1;
        p++;
    }

    while (*p && !is_csi_final((unsigned char) *p)) {
        if (*p >= '0' && *p <= '9') {
            if (count < 16)
                params[count] = parse_int_fast(&p);
            else {
                while (*p >= '0' && *p <= '9')
                    p++;
            }
        } else if (*p == ';') {
            if (count < 15) count++;
            p++;
        } else {
            p++;
        }
    }

    count++;

    char cmd = *p ? *p : '\0';
    int p0 = params[0];
    int p1 = (count > 1) ? params[1] : 0;

    if (is_private && (cmd == 'h' || cmd == 'l')) {
        vt_handle_csi_private(params, count, cmd == 'h');
        screen_dirty = 1;
        return;
    }

    switch (cmd) {
        case 'A':
            set_cursor(cursor_row - (p0 ? p0 : 1), cursor_col);
            break;
        case 'B':
            set_cursor(cursor_row + (p0 ? p0 : 1), cursor_col);
            break;
        case 'C':
            set_cursor(cursor_row, cursor_col + (p0 ? p0 : 1));
            break;
        case 'D':
            set_cursor(cursor_row, cursor_col - (p0 ? p0 : 1));
            break;
        case 'E':
            set_cursor(cursor_row + (p0 ? p0 : 1), 0);
            break;
        case 'F':
            set_cursor(cursor_row - (p0 ? p0 : 1), 0);
            break;
        case 'G':
            set_cursor(cursor_row, (p0 ? p0 : 1) - 1);
            break;
        case 'd':
            set_cursor((p0 ? p0 : 1) - 1, cursor_col);
            break;
        case 'H':
        case 'f':
            set_cursor((p0 ? p0 : 1) - 1, (p1 ? p1 : 1) - 1);
            break;
        case 'J': {
            if (p0 == 0) {
                clear_row_range(cursor_row, cursor_col, TERM_COLS - 1);
                for (int r = cursor_row + 1; r < TERM_ROWS; r++)
                    clear_row_range(r, 0, TERM_COLS - 1);
            } else if (p0 == 1) {
                for (int r = 0; r < cursor_row; r++)
                    clear_row_range(r, 0, TERM_COLS - 1);
                clear_row_range(cursor_row, 0, cursor_col);
            } else if (p0 == 2) {
                clear_screen();
                set_cursor(0, 0);
            }
            break;
        }
        case 'K': {
            if (p0 == 0) {
                clear_row_range(cursor_row, cursor_col, TERM_COLS - 1);
            } else if (p0 == 1) {
                clear_row_range(cursor_row, 0, cursor_col);
            } else if (p0 == 2) {
                clear_row_range(cursor_row, 0, TERM_COLS - 1);
            }
            break;
        }
        case 'L': {
            int n = p0 ? p0 : 1;
            if (row_in_scroll_region(cursor_row)) scroll_region_down(cursor_row, scroll_bottom, n);
            break;
        }
        case 'M': {
            int n = p0 ? p0 : 1;
            if (row_in_scroll_region(cursor_row)) scroll_region_up(cursor_row, scroll_bottom, n, 0);
            break;
        }
        case 'P': {
            int n = p0 ? p0 : 1;
            if (n < 0) n = 0;
            if (cursor_col + n > TERM_COLS) n = TERM_COLS - cursor_col;
            if (n > 0) {
                memmove(
                    CELL(cursor_row, cursor_col), CELL(cursor_row, cursor_col + n),
                    sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n)
                );
                for (int c = TERM_COLS - n; c < TERM_COLS; c++)
                    reset_cell(CELL(cursor_row, c));

                mark_row_dirty(cursor_row);
            }
            break;
        }
        case '@': {
            int n = p0 ? p0 : 1;
            if (n < 0) n = 0;
            if (cursor_col + n > TERM_COLS) n = TERM_COLS - cursor_col;
            if (n > 0) {
                memmove(
                    CELL(cursor_row, cursor_col + n), CELL(cursor_row, cursor_col),
                    sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n)
                );
                for (int c = cursor_col; c < cursor_col + n; c++)
                    reset_cell(CELL(cursor_row, c));

                mark_row_dirty(cursor_row);
            }
            break;
        }
        case 'S':
            scroll_region_up(scroll_top, scroll_bottom, p0 ? p0 : 1, 1);
            break;
        case 'T':
            scroll_region_down(scroll_top, scroll_bottom, p0 ? p0 : 1);
            break;
        case 'X': {
            int n = p0 ? p0 : 1;
            for (int c = cursor_col; c < cursor_col + n && c < TERM_COLS; c++)
                reset_cell(CELL(cursor_row, c));

            mark_row_dirty(cursor_row);
            break;
        }
        case 'm':
            apply_sgr(params, count);
            break;
        case 'r': {
            int top = (p0 ? p0 : 1) - 1;
            int bottom = (p1 ? p1 : TERM_ROWS) - 1;
            if (top < 0) top = 0;
            if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
            if (top >= bottom) {
                scroll_top = 0;
                scroll_bottom = TERM_ROWS - 1;
            } else {
                scroll_top = top;
                scroll_bottom = bottom;
            }
            set_cursor(0, 0);
            break;
        }
        case 's':
            saved_row = cursor_row;
            saved_col = cursor_col;
            break;
        case 'u':
            set_cursor(saved_row, saved_col);
            break;
        case 'h':
        case 'l': {
            int enable = (cmd == 'h');
            for (int i = 0; i < count; i++) {
                int mode = params[i];
                if (!is_private && mode == 20) linefeed_mode = enable;
            }
            break;
        }
        default:
            break;
    }

    screen_dirty = 1;
}

static void dcs_append(const unsigned char *data, size_t len) {
    if (dcs_len + len > dcs_cap) {
        size_t need = dcs_len + len;
        size_t new_cap = ((need + 65535) / 65536) * 65536;
        unsigned char *nb = realloc(dcs_buf, new_cap);

        if (!nb) return;

        dcs_buf = nb;
        dcs_cap = new_cap;
    }

    memcpy(dcs_buf + dcs_len, data, len);
    dcs_len += len;
}

static void dcs_dispatch(void) {
    if (dcs_buf && dcs_len > 0) {
        /* Find the 'q' that introduces a sixel stream and decode. */
        const char *q = memchr((char *) dcs_buf, 'q', dcs_len);
        if (q) {
            sixel_decode(q + 1, dcs_len - (size_t) (q + 1 - (char *) dcs_buf), dcs_cell_x, dcs_cell_y);

            int sx, sy, sw, sh;
            if (sixel_pixels(&sx, &sy, &sw, &sh)) {
                int img_cell_w = (sw + sixel_cell_w() - 1) / sixel_cell_w();
                int cell_h = sixel_cell_h();
                int inline_glyph = (sh <= cell_h + 5);

                int new_col = sx + img_cell_w;
                if (new_col >= TERM_COLS) new_col = TERM_COLS - 1;

                if (inline_glyph) {
                    set_cursor(sy, new_col);
                } else {
                    int img_cell_h = (sh + cell_h - 1) / cell_h;
                    int new_row = sy + img_cell_h;

                    if (new_row >= TERM_ROWS) {
                        int overflow = new_row - (TERM_ROWS - 1);
                        scroll_region_up(scroll_top, scroll_bottom, overflow, 1);
                        new_row = TERM_ROWS - 1;
                    }

                    set_cursor(new_row, 0);
                }
            }
        }
    }

    dcs_len = 0;
    dcs_active = 0;
}

static void handle_osc(const char *seq) {
    int ps = 0;
    const char *p = seq;

    while (*p >= '0' && *p <= '9')
        ps = ps * 10 + (*p++ - '0');

    if (*p == ';') p++;
    if ((ps == 0 || ps == 1 || ps == 2) && g_title_cb) g_title_cb(p, g_title_userdata);
}

static void handle_esc(const char *seq) {
    if (!*seq) return;
    switch (seq[0]) {
        case '(':
            if (seq[1]) vt_designate_charset(0, seq[1]);
            break;
        case ')':
            if (seq[1]) vt_designate_charset(1, seq[1]);
            break;
        case '7':
            saved_row = cursor_row;
            saved_col = cursor_col;
            break;
        case '8':
            set_cursor(saved_row, saved_col);
            break;
        case 'D':
            vt_index();
            break;
        case 'E':
            cursor_col = 0;
            vt_index();
            break;
        case 'M':
            vt_reverse_index();
            break;
        case '=':
        case '>':
            break;
        case 'c':
            vt_reset_state();
            break;
        default:
            break;
    }
}

static size_t vt_try_parse_escape(const unsigned char *buf, size_t len) {
    if (len == 0 || buf[0] != 0x1B || len == 1) return 0;

    if (buf[1] == '[') {
        size_t i = 2;
        while (i < len && !is_csi_final(buf[i]))
            i++;
        if (i >= len) return 0;

        char tmp[256];
        size_t slen = i < sizeof(tmp) ? i : sizeof(tmp) - 1;
        memcpy(tmp, buf + 1, slen);
        tmp[slen] = '\0';
        parse_csi(tmp);

        return i + 1;
    }

    if (buf[1] == ']') {
        size_t i = 2;
        while (i < len) {
            if (buf[i] == 0x07) {
                char tmp[256];
                size_t slen = i - 2;

                if (slen >= sizeof(tmp)) slen = sizeof(tmp) - 1;

                memcpy(tmp, buf + 2, slen);
                tmp[slen] = '\0';
                handle_osc(tmp);

                return i + 1;
            }
            if (buf[i] == 0x1B && i + 1 < len && buf[i + 1] == '\\') {
                char tmp[256];
                size_t slen = i - 2;

                if (slen >= sizeof(tmp)) slen = sizeof(tmp) - 1;

                memcpy(tmp, buf + 2, slen);
                tmp[slen] = '\0';
                handle_osc(tmp);

                return i + 2;
            }
            i++;
        }
        return 0;
    }

    if (buf[1] == 'P') {
        dcs_active = 1;
        dcs_len = 0;
        dcs_cell_x = cursor_col;
        dcs_cell_y = cursor_row;
        return 2;
    }

    if (buf[1] == '(' || buf[1] == ')') {
        if (len < 3) return 0;

        char tmp[4] = {(char) buf[1], (char) buf[2], '\0', '\0'};
        handle_esc(tmp);

        return 3;
    }

    char tmp[2] = {(char) buf[1], '\0'};
    handle_esc(tmp);

    return 2;
}

static int wcwidth_emu(Uint32 cp) {
    if (cp == 0 || cp < 32 || (cp >= 0x7F && cp < 0xA0) || (cp >= 0x0300 && cp <= 0x036F)) return 0;

    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2329 && cp <= 0x232A) || (cp >= 0x2E80 && cp <= 0xA4CF)
        || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19)
        || (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6))
        return 2;

    return 1;
}

static size_t utf8_decode_char(Uint32 *out, const unsigned char *s, size_t len) {
    if (!s || len == 0) return 0;
    unsigned char b0 = s[0];

    if (b0 < 0x80) {
        *out = b0;
        return 1;
    }

    if ((b0 & 0xE0) == 0xC0) {
        if (len < 2) return (size_t) -2;

        unsigned char b1 = s[1];
        if ((b1 & 0xC0) != 0x80) return (size_t) -1;

        Uint32 cp = ((Uint32) (b0 & 0x1F) << 6) | (Uint32) (b1 & 0x3F);
        if (cp < 0x80) return (size_t) -1;

        *out = cp;

        return 2;
    }

    if ((b0 & 0xF0) == 0xE0) {
        if (len < 3) return (size_t) -2;

        unsigned char b1 = s[1], b2 = s[2];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return (size_t) -1;

        Uint32 cp = ((Uint32) (b0 & 0x0F) << 12) | ((Uint32) (b1 & 0x3F) << 6) | (Uint32) (b2 & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return (size_t) -1;

        *out = cp;

        return 3;
    }

    if ((b0 & 0xF8) == 0xF0) {
        if (len < 4) return (size_t) -2;

        unsigned char b1 = s[1], b2 = s[2], b3 = s[3];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return (size_t) -1;

        Uint32 cp = ((Uint32) (b0 & 0x07) << 18) | ((Uint32) (b1 & 0x3F) << 12) | ((Uint32) (b2 & 0x3F) << 6)
                    | (Uint32) (b3 & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) return (size_t) -1;

        *out = cp;

        return 4;
    }

    return (size_t) -1;
}

static void put_char(Uint32 ch) {
    if (ch == '\n' || ch == '\v' || ch == '\f') {
        vt_newline();
        return;
    }

    if (ch == '\r') {
        cursor_col = 0;
        screen_dirty = 1;
        return;
    }

    if (ch == '\b') {
        if (cursor_col > 0) cursor_col--;
        return;
    }

    if (ch == '\t') {
        int next = ((cursor_col / 8) + 1) * 8;
        cursor_col = (next >= TERM_COLS) ? TERM_COLS - 1 : next;
        return;
    }

    ch = vt_apply_graphics_charset(ch);

    int w = wcwidth_emu(ch);
    if (w <= 0) return;

    if (cursor_col >= TERM_COLS || cursor_col + w > TERM_COLS) {
        if (!autowrap_mode) {
            cursor_col = TERM_COLS - w;
            if (cursor_col < 0) cursor_col = 0;
        } else {
            mark_row_dirty(cursor_row);
            cursor_col = 0;
            vt_index();
        }
    }

    if (cursor_row >= TERM_ROWS) cursor_row = TERM_ROWS - 1;

    Cell *c = CELL(cursor_row, cursor_col);

    c->codepoint = ch;
    c->width = (Uint8) w;
    c->fg = current_fg;
    c->bg = current_bg;
    c->style = current_style;

    if (w == 2 && cursor_col + 1 < TERM_COLS) {
        Cell *c2 = CELL(cursor_row, cursor_col + 1);
        c2->codepoint = 0;
        c2->width = 0;
        c2->fg = c->fg;
        c2->bg = c->bg;
        c2->style = c->style;
    }

    mark_row_dirty(cursor_row);
    cursor_col += w;
}

static int vt_process_stream_bytes(const unsigned char *buf, size_t len) {
    size_t pos = 0;
    int saw_output = 0;

    while (pos < len) {
        if (dcs_active) {
            size_t start = pos;
            while (pos < len) {
                if (buf[pos] == 0x1B && pos + 1 < len && buf[pos + 1] == '\\') {
                    dcs_append(buf + start, pos - start);
                    dcs_dispatch();
                    pos += 2;
                    saw_output = 1;
                    goto dcs_done;
                }
                pos++;
            }

            dcs_append(buf + start, pos - start);
            break;

        dcs_done:;
            continue;
        }

        unsigned char ch = buf[pos];

        if (ch == 0x0E) {
            vt_shift_out();
            screen_dirty = 1;
            pos++;
            continue;
        }

        if (ch == 0x0F) {
            vt_shift_in();
            screen_dirty = 1;
            pos++;
            continue;
        }

        if (ch == 0x1B) {
            size_t consumed = vt_try_parse_escape(buf + pos, len - pos);
            if (consumed == 0) break;
            pos += consumed;
            saw_output = 1;
            continue;
        }

        Uint32 cp = 0;
        size_t consumed = utf8_decode_char(&cp, buf + pos, len - pos);

        if (consumed == (size_t) -2) break;

        if (consumed == (size_t) -1) {
            put_char(0xFFFD);
            screen_dirty = 1;
            saw_output = 1;
            pos++;
            continue;
        }

        put_char(cp);
        screen_dirty = 1;

        saw_output = 1;
        pos += consumed;
    }

    if (pos < len) {
        size_t rem = len - pos;
        if (rem > sizeof(esc_pending)) rem = sizeof(esc_pending);

        memcpy(esc_pending, buf + pos, rem);
        esc_pending_len = rem;
    } else {
        esc_pending_len = 0;
    }

    if (saw_output) screen_dirty = 1;

    return saw_output;
}

int vt_init(int cols, int rows, int scrollback_capacity) {
    TERM_COLS = (cols > 0) ? cols : 80;
    TERM_ROWS = (rows > 0) ? rows : 24;

    sb_capacity = (scrollback_capacity > 0) ? scrollback_capacity : 512;

    main_screen_buf = calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    alt_screen_buf = calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    scrollback = calloc((size_t) sb_capacity * (size_t) TERM_COLS, sizeof(Cell));
    row_dirty = calloc((size_t) TERM_ROWS, sizeof(Uint8));

    if (!main_screen_buf || !alt_screen_buf || !scrollback || !row_dirty) {
        free(main_screen_buf);
        main_screen_buf = NULL;

        free(alt_screen_buf);
        alt_screen_buf = NULL;

        free(scrollback);
        scrollback = NULL;

        free(row_dirty);
        row_dirty = NULL;

        return -1;
    }

    sb_count = 0;
    sb_head = 0;

    screen_buf = main_screen_buf;
    using_alt_screen = 0;

    current_fg = default_fg;
    current_bg = default_bg;

    current_style = 0;

    vt_reset_state();

    return 0;
}

void vt_resize(int new_cols, int new_rows) {
    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (new_cols == TERM_COLS && new_rows == TERM_ROWS) return;

    Cell *new_main = calloc((size_t) new_rows * (size_t) new_cols, sizeof(Cell));
    Cell *new_alt = calloc((size_t) new_rows * (size_t) new_cols, sizeof(Cell));
    Cell *new_sb = calloc((size_t) sb_capacity * (size_t) new_cols, sizeof(Cell));
    Uint8 *new_dirty = calloc((size_t) new_rows, sizeof(Uint8));

    if (!new_main || !new_alt || !new_sb || !new_dirty) {
        free(new_main);
        free(new_alt);
        free(new_sb);
        free(new_dirty);
        fprintf(stderr, "[VT] resize alloc failed, keeping old dimensions\n");
        return;
    }

    SDL_Color blank_fg = current_fg;
    SDL_Color blank_bg = current_bg;

    size_t new_total = (size_t) new_rows * (size_t) new_cols;
    for (size_t i = 0; i < new_total; i++) {
        new_main[i].codepoint = (Uint32) ' ';
        new_main[i].width = 1;

        new_main[i].fg = blank_fg;
        new_main[i].bg = blank_bg;

        new_alt[i].codepoint = (Uint32) ' ';
        new_alt[i].width = 1;

        new_alt[i].fg = blank_fg;
        new_alt[i].bg = blank_bg;
    }

    int copy_rows = TERM_ROWS < new_rows ? TERM_ROWS : new_rows;
    int copy_cols = TERM_COLS < new_cols ? TERM_COLS : new_cols;

    for (int r = 0; r < copy_rows; r++) {
        for (int c = 0; c < copy_cols; c++) {
            new_main[(size_t) r * (size_t) new_cols + (size_t) c] =
                main_screen_buf[(size_t) r * (size_t) TERM_COLS + (size_t) c];
        }
    }

    int new_sb_count = 0;
    int new_sb_head = 0;

    size_t new_sb_cols_total = (size_t) sb_capacity * (size_t) new_cols;
    for (size_t i = 0; i < new_sb_cols_total; i++) {
        new_sb[i].codepoint = (Uint32) ' ';
        new_sb[i].width = 1;
        new_sb[i].fg = blank_fg;
        new_sb[i].bg = blank_bg;
    }

    int copy_sb_cols = TERM_COLS < new_cols ? TERM_COLS : new_cols;
    for (int i = 0; i < sb_count && i < sb_capacity; i++) {
        const Cell *src_row = vt_scrollback_row(i);
        if (!src_row) continue;

        Cell *dst_row = &new_sb[(size_t) new_sb_head * (size_t) new_cols];

        for (int c = 0; c < copy_sb_cols; c++)
            dst_row[c] = src_row[c];
        new_sb_head = (new_sb_head + 1) % sb_capacity;

        new_sb_count++;
    }

    free(main_screen_buf);
    free(alt_screen_buf);

    free(scrollback);
    free(row_dirty);

    main_screen_buf = new_main;
    alt_screen_buf = new_alt;

    scrollback = new_sb;
    row_dirty = new_dirty;

    sb_count = new_sb_count;
    sb_head = new_sb_head;

    TERM_COLS = new_cols;
    TERM_ROWS = new_rows;

    screen_buf = using_alt_screen ? alt_screen_buf : main_screen_buf;

    if (cursor_row >= TERM_ROWS) cursor_row = TERM_ROWS - 1;
    if (cursor_col >= TERM_COLS) cursor_col = TERM_COLS - 1;

    if (saved_row >= TERM_ROWS) saved_row = TERM_ROWS - 1;
    if (saved_col >= TERM_COLS) saved_col = TERM_COLS - 1;

    if (saved_main_row >= TERM_ROWS) saved_main_row = TERM_ROWS - 1;
    if (saved_main_col >= TERM_COLS) saved_main_col = TERM_COLS - 1;

    scroll_top = 0;
    scroll_bottom = TERM_ROWS - 1;
    scroll_offset = 0;

    memset(row_dirty, 1, (size_t) TERM_ROWS);
    screen_dirty = 1;

    fprintf(stderr, "[VT] resize → %d cols × %d rows\n", TERM_COLS, TERM_ROWS);
}

void vt_free(void) {
    free(main_screen_buf);
    main_screen_buf = NULL;

    free(alt_screen_buf);
    alt_screen_buf = NULL;

    free(scrollback);
    scrollback = NULL;

    free(row_dirty);
    row_dirty = NULL;

    free(dcs_buf);
    dcs_buf = NULL;
    dcs_cap = 0;
    dcs_len = 0;
    dcs_active = 0;

    sixel_free();

    screen_buf = NULL;
}

int vt_cols(void) {
    return TERM_COLS;
}

int vt_rows(void) {
    return TERM_ROWS;
}

int vt_cursor_row(void) {
    return cursor_row;
}

int vt_cursor_col(void) {
    return cursor_col;
}

int vt_cursor_visible(void) {
    return cursor_vis;
}

int vt_cursor_keys_app(void) {
    return cursor_keys_application;
}

int vt_scrollback_count(void) {
    return sb_count;
}

int vt_using_alt_screen(void) {
    return using_alt_screen;
}

int vt_is_dirty(void) {
    return screen_dirty;
}

void vt_clear_dirty(void) {
    screen_dirty = 0;
}

int vt_row_is_dirty(int row) {
    if (!row_dirty || row < 0 || row >= TERM_ROWS) return 1;
    return row_dirty[row];
}

void vt_clear_row_dirty(int row) {
    if (row_dirty && row >= 0 && row < TERM_ROWS) row_dirty[row] = 0;
}

void vt_mark_all_rows_dirty(void) {
    if (row_dirty) memset(row_dirty, 1, (size_t) TERM_ROWS);
    screen_dirty = 1;
}

void vt_mark_cursor_row_dirty(void) {
    mark_row_dirty(cursor_row);
    screen_dirty = 1;
}

int vt_scroll_offset(void) {
    return scroll_offset;
}

void vt_scroll_set(int offset) {
    if (offset < 0) offset = 0;
    if (offset > sb_count) offset = sb_count;

    if (offset == scroll_offset) return;

    scroll_offset = offset;
    vt_mark_all_rows_dirty();
}

void vt_scroll_adjust(int delta, int max_visible_rows) {
    (void) max_visible_rows;
    int new_off = scroll_offset + delta;

    if (new_off > sb_count) new_off = sb_count;
    if (new_off < 0) new_off = 0;

    if (new_off == scroll_offset) return;

    scroll_offset = new_off;
    vt_mark_all_rows_dirty();
}

const Cell *vt_scrollback_row(int idx) {
    if (idx < 0 || idx >= sb_count) return NULL;

    int pos = (sb_head - sb_count + idx + sb_capacity) % sb_capacity;
    return &scrollback[(size_t) pos * (size_t) TERM_COLS];
}

Cell *vt_cell(int row, int col) {
    if (row < 0) row = 0;
    if (row >= TERM_ROWS) row = TERM_ROWS - 1;

    if (col < 0) col = 0;
    if (col >= TERM_COLS) col = TERM_COLS - 1;

    return CELL(row, col);
}

int vt_feed(const char *buf, size_t len) {
    if (esc_pending_len > 0) {
        unsigned char merged[sizeof(esc_pending) + 8];
        size_t prefix = esc_pending_len;

        memcpy(merged, esc_pending, prefix);
        esc_pending_len = 0;

        size_t adj = len < sizeof(merged) - prefix ? len : sizeof(merged) - prefix;
        memcpy(merged + prefix, buf, adj);

        size_t merged_total = prefix + adj;
        size_t pos = 0;

        while (pos < merged_total) {
            unsigned char ch = merged[pos];

            if (ch == 0x0E) {
                vt_shift_out();
                screen_dirty = 1;
                pos++;
                continue;
            }

            if (ch == 0x0F) {
                vt_shift_in();
                screen_dirty = 1;
                pos++;
                continue;
            }

            if (ch == 0x1B) {
                size_t c = vt_try_parse_escape(merged + pos, merged_total - pos);
                if (c == 0) break;
                pos += c;
                screen_dirty = 1;
                continue;
            }

            Uint32 cp = 0;
            size_t c = utf8_decode_char(&cp, merged + pos, merged_total - pos);
            if (c == (size_t) -2) break;

            if (c == (size_t) -1) {
                put_char(0xFFFD);
                screen_dirty = 1;
                pos++;
                continue;
            }

            put_char(cp);
            screen_dirty = 1;
            pos += c;
        }

        if (pos < merged_total) {
            size_t rem = merged_total - pos;
            if (rem > sizeof(esc_pending)) rem = sizeof(esc_pending);

            memcpy(esc_pending, merged + pos, rem);
            esc_pending_len = rem;

            if (adj < len) return vt_process_stream_bytes((const unsigned char *) buf + adj, len - adj) | screen_dirty;

            return screen_dirty;
        }

        size_t consumed = (pos > prefix) ? pos - prefix : 0;
        buf += consumed;
        len = (len > consumed) ? len - consumed : 0;
    }

    esc_pending_len = 0;
    return vt_process_stream_bytes((const unsigned char *) buf, len);
}

void vt_set_title_callback(vt_title_cb_t cb, void *userdata) {
    g_title_cb = cb;
    g_title_userdata = userdata;
}

int vt_scrollback_save(const char *path) {
    if (!path || !*path || sb_count == 0) return 0;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    Uint32 magic = SB_MAGIC;
    Uint32 cols = (Uint32) TERM_COLS;
    Uint32 count = (Uint32) sb_count;

    fwrite(&magic, 4, 1, f);
    fwrite(&cols, 4, 1, f);
    fwrite(&count, 4, 1, f);

    for (int i = 0; i < sb_count; i++) {
        const Cell *row = vt_scrollback_row(i);
        if (!row) continue;

        for (int c = 0; c < TERM_COLS; c++) {
            Uint32 cp = row[c].codepoint;
            Uint8 width = row[c].width;

            Uint8 fg[4] = {row[c].fg.r, row[c].fg.g, row[c].fg.b, row[c].fg.a};
            Uint8 bg[4] = {row[c].bg.r, row[c].bg.g, row[c].bg.b, row[c].bg.a};

            Uint8 style = row[c].style;

            fwrite(&cp, 4, 1, f);
            fwrite(&width, 1, 1, f);

            fwrite(fg, 4, 1, f);
            fwrite(bg, 4, 1, f);

            fwrite(&style, 1, 1, f);
        }
    }

    fclose(f);
    fprintf(stderr, "[VT] scrollback saved: %d rows -> %s\n", sb_count, path);
    return 0;
}

int vt_scrollback_load(const char *path) {
    if (!path || !*path) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    Uint32 magic = 0, cols = 0, count = 0;
    if (fread(&magic, 4, 1, f) != 1 || magic != SB_MAGIC || fread(&cols, 4, 1, f) != 1 || fread(&count, 4, 1, f) != 1) {
        fclose(f);
        return 0;
    }

    if ((int) cols != TERM_COLS || (int) count > sb_capacity) {
        fclose(f);
        return 0;
    }

    sb_count = 0;
    sb_head = 0;

    Cell *row_buf = calloc((size_t) TERM_COLS, sizeof(Cell));
    if (!row_buf) {
        fclose(f);
        return -1;
    }

    for (Uint32 r = 0; r < count; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            Uint32 cp = 0;
            Uint8 width = 0;

            Uint8 fg[4] = {0};
            Uint8 bg[4] = {0};

            Uint8 style = 0;

            if (fread(&cp, 4, 1, f) != 1) goto done;
            if (fread(&width, 1, 1, f) != 1) goto done;

            if (fread(fg, 4, 1, f) != 1) goto done;
            if (fread(bg, 4, 1, f) != 1) goto done;

            if (fread(&style, 1, 1, f) != 1) goto done;

            row_buf[c].codepoint = cp;
            row_buf[c].width = width;

            row_buf[c].fg = (SDL_Color) {fg[0], fg[1], fg[2], fg[3]};
            row_buf[c].bg = (SDL_Color) {bg[0], bg[1], bg[2], bg[3]};

            row_buf[c].style = style;
        }
        scrollback_push(row_buf);
    }

done:
    free(row_buf);
    fclose(f);
    fprintf(stderr, "[VT] scrollback loaded: %d rows from %s\n", sb_count, path);

    return 0;
}
