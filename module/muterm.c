#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"

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
};

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

static int cursor_keys_application = 0;
static int linefeed_mode = 0;

static int scroll_top = 0;
static int scroll_bottom = 0;

static int axis_x_state = 0;
static int axis_y_state = 0;

static int TERM_COLS = 0;
static int TERM_ROWS = 0;

static int CELL_WIDTH = 0;
static int CELL_HEIGHT = 0;

static int cursor_row = 0;
static int cursor_col = 0;
static int cursor_vis = 1;

static int saved_row = 0;
static int saved_col = 0;

static SDL_Color default_fg = {255, 255, 255, 255};
static SDL_Color default_bg = {0, 0, 0, 255};

static SDL_Color current_fg;
static SDL_Color current_bg;

static Uint8 current_style = 0;

static SDL_Color solid_bg = {0, 0, 0, 255};
static SDL_Color solid_fg = {0, 0, 0, 0};

static int use_solid_bg = 0;
static int use_solid_fg = 0;

static int readonly_mode = 0;
static int screen_dirty = 1;

static SDL_Color base_colours[8] = {
        {0,   0,   0,   255},
        {170, 0,   0,   255},
        {0,   170, 0,   255},
        {170, 85,  0,   255},
        {0,   0,   170, 255},
        {170, 0,   170, 255},
        {0,   170, 170, 255},
        {170, 170, 170, 255},
};

static SDL_Color bright_colours[8] = {
        {85,  85,  85,  255},
        {255, 85,  85,  255},
        {85,  255, 85,  255},
        {255, 255, 85,  255},
        {85,  85,  255, 255},
        {255, 85,  255, 255},
        {85,  255, 255, 255},
        {255, 255, 255, 255},
};

static Uint32 vt_map_acs(Uint32 cp) {
    switch (cp) {
        case '`':
            return 0x25C6; /* ◆ */
        case 'a':
            return 0x2592; /* ▒ */
        case 'b':
            return 0x2409; /* ␉ */
        case 'c':
            return 0x240C; /* ␌ */
        case 'd':
            return 0x240D; /* ␍ */
        case 'e':
            return 0x240A; /* ␊ */
        case 'f':
            return 0x00B0; /* ° */
        case 'g':
            return 0x00B1; /* ± */
        case 'h':
            return 0x2424; /* ␤ */
        case 'i':
            return 0x240B; /* ␋ */
        case 'j':
            return 0x2518; /* ┘ */
        case 'k':
            return 0x2510; /* ┐ */
        case 'l':
            return 0x250C; /* ┌ */
        case 'm':
            return 0x2514; /* └ */
        case 'n':
            return 0x253C; /* ┼ */
        case 'o':
            return 0x23BA; /* ⎺ */
        case 'p':
            return 0x23BB; /* ⎻ */
        case 'q':
            return 0x2500; /* ─ */
        case 'r':
            return 0x23BC; /* ⎼ */
        case 's':
            return 0x23BD; /* ⎽ */
        case 't':
            return 0x251C; /* ├ */
        case 'u':
            return 0x2524; /* ┤ */
        case 'v':
            return 0x2534; /* ┴ */
        case 'w':
            return 0x252C; /* ┬ */
        case 'x':
            return 0x2502; /* │ */
        case 'y':
            return 0x2264; /* ≤ */
        case 'z':
            return 0x2265; /* ≥ */
        case '{':
            return 0x03C0; /* π */
        case '|':
            return 0x2260; /* ≠ */
        case '}':
            return 0x00A3; /* £ */
        case '~':
            return 0x00B7; /* · */
        default:
            return cp;
    }
}

static inline int vt_charset_is_graphics(int which) {
    return which ? (vt_g1_charset == 1) : (vt_g0_charset == 1);
}

static inline int vt_gl_is_graphics(void) {
    return vt_charset_is_graphics(vt_gl_charset);
}

static inline Uint32 vt_apply_graphics_charset(Uint32 cp) {
    if (cp < 0x20 || cp > 0x7E) return cp;
    if (!vt_gl_is_graphics()) return cp;

    return vt_map_acs(cp);
}

static void vt_designate_charset(int which, char final) {
    int mode = 0;

    switch (final) {
        case '0':
            mode = 1;
            break;
        case 'B':
        default:
            mode = 0;
            break;
    }

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

static const Uint8 cube6[6] = {0, 95, 135, 175, 215, 255};

static inline Cell *CELL(int r, int c) {
    return &screen_buf[(size_t) r * (size_t) TERM_COLS + (size_t) c];
}

static inline int colour_equal(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

#define DEFAULT_SCROLLBACK 512

static Cell *scrollback = NULL;
static int sb_capacity = DEFAULT_SCROLLBACK;
static int sb_count = 0;
static int sb_head = 0;
static int scroll_offset = 0;

static void clear_cells(Cell *buf) {
    size_t total = (size_t) TERM_ROWS * (size_t) TERM_COLS;

    for (size_t i = 0; i < total; i++) {
        buf[i].codepoint = (Uint32) ' ';
        buf[i].width = 1;
        buf[i].fg = current_fg;
        buf[i].bg = current_bg;
        buf[i].style = 0;
    }
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
}

static void vt_leave_alt_screen(void) {
    if (!using_alt_screen) return;

    screen_buf = main_screen_buf;
    using_alt_screen = 0;

    cursor_row = saved_main_row;
    cursor_col = saved_main_col;
    scroll_offset = 0;
    screen_dirty = 1;
}

static void scrollback_init(void) {
    scrollback = calloc((size_t) sb_capacity * (size_t) TERM_COLS, sizeof(Cell));
    sb_count = 0;
    sb_head = 0;
    scroll_offset = 0;
}

static void scrollback_push(const Cell *row) {
    memcpy(&scrollback[(size_t) sb_head * (size_t) TERM_COLS], row, (size_t) TERM_COLS * sizeof(Cell));
    sb_head = (sb_head + 1) % sb_capacity;

    if (sb_count < sb_capacity) sb_count++;
}

static const Cell *scrollback_row(int idx) {
    if (idx < 0 || idx >= sb_count) return NULL;
    int pos = (sb_head - sb_count + idx + sb_capacity) % sb_capacity;

    return &scrollback[(size_t) pos * (size_t) TERM_COLS];
}

static inline void reset_cell(Cell *c) {
    c->codepoint = (Uint32) ' ';
    c->width = 1;
    c->fg = current_fg;
    c->bg = current_bg;
    c->style = 0;
}

static void clear_screen(void) {
    size_t total = (size_t) TERM_ROWS * (size_t) TERM_COLS;
    for (size_t i = 0; i < total; i++) {
        reset_cell(&screen_buf[i]);
    }
}

static void set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= TERM_ROWS) row = TERM_ROWS - 1;

    if (col < 0) col = 0;
    if (col >= TERM_COLS) col = TERM_COLS - 1;

    cursor_row = row;
    cursor_col = col;
}

static inline int row_in_scroll_region(int row) {
    return row >= scroll_top && row <= scroll_bottom;
}

static void clear_row_range(int row, int start_col, int end_col) {
    if (row < 0 || row >= TERM_ROWS) return;
    if (start_col < 0) start_col = 0;
    if (end_col >= TERM_COLS) end_col = TERM_COLS - 1;
    if (start_col > end_col) return;

    for (int c = start_col; c <= end_col; c++) {
        reset_cell(CELL(row, c));
    }
}

static void scroll_region_up(int top, int bottom, int lines, int allow_scrollback) {
    if (TERM_ROWS <= 1) return;
    if (top < 0) top = 0;
    if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
    if (top >= bottom || lines <= 0) return;

    int height = bottom - top + 1;
    if (lines > height) lines = height;

    if (allow_scrollback && !using_alt_screen && top == 0 && bottom == TERM_ROWS - 1) {
        for (int i = 0; i < lines; i++) {
            scrollback_push(CELL(top + i, 0));
        }
    }

    if (lines < height) memmove(CELL(top, 0), CELL(top + lines, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (height - lines));

    for (int r = bottom - lines + 1; r <= bottom; r++) {
        clear_row_range(r, 0, TERM_COLS - 1);
    }
}

static void scroll_region_down(int top, int bottom, int lines) {
    if (TERM_ROWS <= 1) return;
    if (top < 0) top = 0;
    if (bottom >= TERM_ROWS) bottom = TERM_ROWS - 1;
    if (top >= bottom || lines <= 0) return;

    int height = bottom - top + 1;
    if (lines > height) lines = height;

    if (lines < height) memmove(CELL(top + lines, 0), CELL(top, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (height - lines));

    for (int r = top; r < top + lines; r++) {
        clear_row_range(r, 0, TERM_COLS - 1);
    }
}

static void vt_index(void) {
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

static void vt_reset_state(int clear_buffers) {
    cursor_keys_application = 0;
    linefeed_mode = 0;

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

    if (clear_buffers) {
        if (main_screen_buf) clear_cells(main_screen_buf);
        if (alt_screen_buf) clear_cells(alt_screen_buf);
    }

    set_cursor(0, 0);
    scroll_offset = 0;
    screen_dirty = 1;
}

static void handle_esc(const char *seq) {
    if (!seq || !*seq) return;

    switch (seq[0]) {
        case '(':
            if (seq[1]) vt_designate_charset(0, seq[1]);
            return;
        case ')':
            if (seq[1]) vt_designate_charset(1, seq[1]);
            return;
        case '7':
            saved_row = cursor_row;
            saved_col = cursor_col;
            return;
        case '8':
            set_cursor(saved_row, saved_col);
            return;
        case 'D':
            vt_index();
            return;
        case 'E':
            cursor_col = 0;
            vt_index();
            return;
        case 'M':
            vt_reverse_index();
            return;
        case '=':
        case '>':
            return;
        case 'c':
            vt_reset_state(1);
            return;
        default:
            return;
    }
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

static int wcwidth_emu(Uint32 cp) {
    if (cp == 0 || cp < 32 || (cp >= 0x7F && cp < 0xA0) || (cp >= 0x0300 && cp <= 0x036F)) return 0;

    if (
            (cp >= 0x1100 && cp <= 0x115F) ||
            (cp >= 0x2329 && cp <= 0x232A) ||
            (cp >= 0x2E80 && cp <= 0xA4CF) ||
            (cp >= 0xAC00 && cp <= 0xD7A3) ||
            (cp >= 0xF900 && cp <= 0xFAFF) ||
            (cp >= 0xFE10 && cp <= 0xFE19) ||
            (cp >= 0xFE30 && cp <= 0xFE6F) ||
            (cp >= 0xFF00 && cp <= 0xFF60) ||
            (cp >= 0xFFE0 && cp <= 0xFFE6)
            ) {
        return 2;
    }

    return 1;
}

static void put_char(Uint32 ch) {
    int w;

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

    w = wcwidth_emu(ch);
    if (w <= 0) return;

    if (cursor_col >= TERM_COLS || cursor_col + w > TERM_COLS) {
        cursor_col = 0;
        vt_index();
    }

    if (cursor_row >= TERM_ROWS) cursor_row = TERM_ROWS - 1;

    {
        Cell *c = CELL(cursor_row, cursor_col);

        c->codepoint = ch;
        c->width = (Uint8) w;
        c->fg = use_solid_fg ? solid_fg : current_fg;
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
    }

    cursor_col += w;
}

static int utf8_encode(char out[8], Uint32 cp) {
    // Returns bytes written (excluding NUL), always NUL does terminate
    if (cp <= 0x7F) {
        out[0] = (char) cp;
        out[1] = '\0';
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (char) (0xC0 | ((cp >> 6) & 0x1F));
        out[1] = (char) (0x80 | (cp & 0x3F));
        out[2] = '\0';
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (char) (0xE0 | ((cp >> 12) & 0x0F));
        out[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char) (0x80 | (cp & 0x3F));
        out[3] = '\0';
        return 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (char) (0xF0 | ((cp >> 18) & 0x07));
        out[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char) (0x80 | (cp & 0x3F));
        out[4] = '\0';
        return 4;
    }

    out[0] = (char) 0xEF;
    out[1] = (char) 0xBF;
    out[2] = (char) 0xBD;
    out[3] = '\0';

    return 3;
}

static size_t utf8_decode_char(Uint32 *out, const unsigned char *s, size_t len) {
    Uint32 cp;
    unsigned char b0, b1, b2, b3;

    if (!out || !s || len == 0) return 0;

    b0 = s[0];

    if (b0 < 0x80) {
        *out = b0;
        return 1;
    }

    if ((b0 & 0xE0) == 0xC0) {
        if (len < 2) return (size_t) -2;
        b1 = s[1];
        if ((b1 & 0xC0) != 0x80) return (size_t) -1;

        cp = ((Uint32) (b0 & 0x1F) << 6) | ((Uint32) (b1 & 0x3F));
        if (cp < 0x80) return (size_t) -1;

        *out = cp;
        return 2;
    }

    if ((b0 & 0xF0) == 0xE0) {
        if (len < 3) return (size_t) -2;

        b1 = s[1];
        b2 = s[2];

        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return (size_t) -1;

        cp = ((Uint32) (b0 & 0x0F) << 12) | ((Uint32) (b1 & 0x3F) << 6) | ((Uint32) (b2 & 0x3F));

        if (cp < 0x800) return (size_t) -1;
        if (cp >= 0xD800 && cp <= 0xDFFF) return (size_t) -1;

        *out = cp;
        return 3;
    }

    if ((b0 & 0xF8) == 0xF0) {
        if (len < 4) return (size_t) -2;

        b1 = s[1];
        b2 = s[2];
        b3 = s[3];

        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return (size_t) -1;

        cp = ((Uint32) (b0 & 0x07) << 18) | ((Uint32) (b1 & 0x3F) << 12) | ((Uint32) (b2 & 0x3F) << 6) | ((Uint32) (b3 & 0x3F));
        if (cp < 0x10000 || cp > 0x10FFFF) return (size_t) -1;

        *out = cp;
        return 4;
    }

    return (size_t) -1;
}

static inline int is_csi_final(unsigned char ch) {
    return (ch >= 0x40 && ch <= 0x7E);
}

static inline int parse_int_fast(const char **p) {
    int v = 0;

    while (**p >= '0' && **p <= '9') {
        v = v * 10 + (**p - '0');
        (*p)++;
    }

    return v;
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
            case 4:
                current_style |= STYLE_UNDERLINE;
                break;
            case 7:
                current_style |= STYLE_REVERSE;
                break;
            case 22:
                current_style &= (Uint8) ~STYLE_BOLD;
                break;
            case 24:
                current_style &= (Uint8) ~STYLE_UNDERLINE;
                break;
            case 27:
                current_style &= (Uint8) ~STYLE_REVERSE;
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
                        current_fg = (SDL_Color) {(Uint8) params[i + 2], (Uint8) params[i + 3], (Uint8) params[i + 4], 255};
                        i += 4;
                    }
                } else if (p == 48) {
                    if (i + 1 < count && params[i + 1] == 5 && i + 2 < count) {
                        current_bg = colour_from_256(params[i + 2]);
                        i += 2;
                    } else if (i + 1 < count && params[i + 1] == 2 && i + 4 < count) {
                        current_bg = (SDL_Color) {(Uint8) params[i + 2], (Uint8) params[i + 3], (Uint8) params[i + 4], 255};
                        i += 4;
                    }
                }
                break;
        }
    }
}

static void parse_csi(const char *seq) {
    if (!seq || seq[0] != '[') return;

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
            if (count < 16) {
                params[count] = parse_int_fast(&p);
            } else {
                while (*p >= '0' && *p <= '9') p++;
            }
        } else if (*p == ';') {
            if (count < 15) count++;
            p++;
        } else {
            p++;
        }
    }

    count++;

    {
        char cmd = *p ? *p : '\0';
        int p0 = params[0];
        int p1 = (count > 1) ? params[1] : 0;

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
                int mode = p0;

                if (mode == 0) {
                    clear_row_range(cursor_row, cursor_col, TERM_COLS - 1);
                    for (int r = cursor_row + 1; r < TERM_ROWS; r++) clear_row_range(r, 0, TERM_COLS - 1);
                } else if (mode == 1) {
                    for (int r = 0; r < cursor_row; r++) clear_row_range(r, 0, TERM_COLS - 1);
                    clear_row_range(cursor_row, 0, cursor_col);
                } else if (mode == 2) {
                    clear_screen();
                    set_cursor(0, 0);
                }
                break;
            }
            case 'K': {
                int mode = p0;

                if (mode == 0) {
                    clear_row_range(cursor_row, cursor_col, TERM_COLS - 1);
                } else if (mode == 1) {
                    clear_row_range(cursor_row, 0, cursor_col);
                } else if (mode == 2) {
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
                    memmove(CELL(cursor_row, cursor_col), CELL(cursor_row, cursor_col + n), sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n));

                    for (int c = TERM_COLS - n; c < TERM_COLS; c++) {
                        reset_cell(CELL(cursor_row, c));
                    }
                }
                break;
            }
            case '@': {
                int n = p0 ? p0 : 1;
                if (n < 0) n = 0;
                if (cursor_col + n > TERM_COLS) n = TERM_COLS - cursor_col;

                if (n > 0) {
                    memmove(CELL(cursor_row, cursor_col + n), CELL(cursor_row, cursor_col), sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n));

                    for (int c = cursor_col; c < cursor_col + n; c++) {
                        reset_cell(CELL(cursor_row, c));
                    }
                }
                break;
            }
            case 'S': {
                int n = p0 ? p0 : 1;
                scroll_region_up(scroll_top, scroll_bottom, n, 1);
                break;
            }
            case 'T': {
                int n = p0 ? p0 : 1;
                scroll_region_down(scroll_top, scroll_bottom, n);
                break;
            }
            case 'X': {
                int n = p0 ? p0 : 1;
                for (int c = cursor_col; c < cursor_col + n && c < TERM_COLS; c++) {
                    reset_cell(CELL(cursor_row, c));
                }
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
                    if (is_private) {
                        switch (mode) {
                            case 1:
                                cursor_keys_application = enable;
                                break;
                            case 25:
                                cursor_vis = enable;
                                break;
                            case 47:
                            case 1047:
                            case 1049:
                                if (enable) vt_enter_alt_screen();
                                else vt_leave_alt_screen();
                                break;
                            default:
                                break;
                        }
                    } else {
                        if (mode == 20) linefeed_mode = enable;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    screen_dirty = 1;
}

static size_t vt_try_parse_escape(const unsigned char *buf, size_t len) {
    if (len == 0 || buf[0] != 0x1B || len == 1) return 0;

    if (buf[1] == '[') {
        size_t i = 2;
        while (i < len && !is_csi_final(buf[i])) i++;
        if (i >= len) return 0;

        {
            char tmp[64];
            size_t slen = i;
            if (slen >= sizeof(tmp)) slen = sizeof(tmp) - 1;

            memcpy(tmp, buf + 1, slen);
            tmp[slen] = '\0';
            parse_csi(tmp);
        }
        return i + 1;
    }

    if (buf[1] == '(' || buf[1] == ')') {
        if (len < 3) return 0;
        {
            char tmp[4];
            tmp[0] = (char) buf[1];
            tmp[1] = (char) buf[2];
            tmp[2] = '\0';

            handle_esc(tmp);
        }
        return 3;
    }

    {
        char tmp[2];
        tmp[0] = (char) buf[1];
        tmp[1] = '\0';

        handle_esc(tmp);
    }
    return 2;
}

static int vt_process_stream_bytes(const unsigned char *buf, size_t len) {
    size_t pos = 0;
    int saw_output = 0;

    while (pos < len) {
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

        {
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

static int vt_feed_pty_bytes(const char *buf, size_t len) {
    unsigned char merged[sizeof(esc_pending) + 4096];
    size_t total = 0;
    int saw_output;

    if (esc_pending_len > 0) {
        memcpy(merged, esc_pending, esc_pending_len);
        total += esc_pending_len;
    }

    if (len > sizeof(merged) - total) len = sizeof(merged) - total;

    memcpy(merged + total, buf, len);
    total += len;

    esc_pending_len = 0;
    saw_output = vt_process_stream_bytes(merged, total);

    return saw_output;
}

typedef struct GlyphEntry {
    Uint32 codepoint;
    SDL_Color fg;
    Uint8 style;
    SDL_Texture *tex;
    int w, h;
    struct GlyphEntry *next;
} GlyphEntry;

#define GLYPH_BUCKETS     1024u
#define GLYPH_MAX_ENTRIES 8192u

static GlyphEntry *glyph_table[GLYPH_BUCKETS];
static unsigned glyph_entries = 0;

// 4 fonts (normal, bold, underline, bold+underline)
static TTF_Font *fonts[4] = {NULL, NULL, NULL, NULL};

static Uint32 glyph_hash(Uint32 cp, SDL_Color fg, Uint8 style) {
    Uint32 h = cp * 2654435761u;

    h ^= ((Uint32) fg.r << 24) ^ ((Uint32) fg.g << 16) ^ ((Uint32) fg.b << 8) ^ (Uint32) fg.a;
    h ^= (Uint32) style * 97531u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;

    return h;
}

static void glyph_cache_clear(void) {
    for (unsigned i = 0; i < GLYPH_BUCKETS; i++) {
        GlyphEntry *e = glyph_table[i];

        while (e) {
            GlyphEntry *n = e->next;
            if (e->tex) SDL_DestroyTexture(e->tex);
            free(e);
            e = n;
        }

        glyph_table[i] = NULL;
    }

    glyph_entries = 0;
}

static int is_vt_box_char(Uint32 cp) {
    return cp >= 0x2500 && cp <= 0x257F;
}

static int is_vt_block_char(Uint32 cp) {
    return cp >= 0x2580 && cp <= 0x259F;
}

static int is_vt_geom_char(Uint32 cp) {
    return cp >= 0x25A0 && cp <= 0x25FF;
}

static int is_vt_scanline_char(Uint32 cp) {
    return cp >= 0x23BA && cp <= 0x23BD;
}

static int is_vt_control_picture(Uint32 cp) {
    switch (cp) {
        case 0x2409: /* ␉ */
        case 0x240A: /* ␊ */
        case 0x240B: /* ␋ */
        case 0x240C: /* ␌ */
        case 0x240D: /* ␍ */
        case 0x2424: /* ␤ */
            return 1;

        default:
            return 0;
    }
}

static int is_vt_soft_char(Uint32 cp) {
    return is_vt_box_char(cp) ||
           is_vt_block_char(cp) ||
           is_vt_geom_char(cp) ||
           is_vt_scanline_char(cp) ||
           is_vt_control_picture(cp) ||
           cp == 0x00B7;
}

static int vt_line_thickness(int cell_h, int heavy) {
    int t = cell_h / 8;
    if (t < 1) t = 1;
    if (heavy && t < 2) t = 2;
    return t;
}

static void fill_rect_px(SDL_Surface *surf, Uint32 col, int x, int y, int w, int h) {
    SDL_Rect r;

    if (w <= 0 || h <= 0) return;

    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;

    SDL_FillRect(surf, &r, col);
}

static void draw_hline_mid(SDL_Surface *surf, Uint32 col, int cw, int ch, int from_x, int to_x, int t) {
    int y = (ch - t) / 2;
    fill_rect_px(surf, col, from_x, y, to_x - from_x, t);
}

static void draw_vline_mid(SDL_Surface *surf, Uint32 col, int cw, int ch, int from_y, int to_y, int t) {
    int x = (cw - t) / 2;
    fill_rect_px(surf, col, x, from_y, t, to_y - from_y);
}

static void draw_hline_px(SDL_Surface *surf, Uint32 col, int x0, int x1, int y, int t) {
    fill_rect_px(surf, col, x0, y, x1 - x0, t);
}

static void draw_vline_px(SDL_Surface *surf, Uint32 col, int x, int y0, int y1, int t) {
    fill_rect_px(surf, col, x, y0, t, y1 - y0);
}

static void draw_frame_px(SDL_Surface *surf, Uint32 col, int x, int y, int w, int h, int t) {
    draw_hline_px(surf, col, x, x + w, y, t);
    draw_hline_px(surf, col, x, x + w, y + h - t, t);
    draw_vline_px(surf, col, x, y, y + h, t);
    draw_vline_px(surf, col, x + w - t, y, y + h, t);
}

static void draw_arrow_right_px(SDL_Surface *surf, Uint32 col, int x0, int x1, int y, int t, int head) {
    int shaft_end = x1 - head;

    if (shaft_end < x0) shaft_end = x0;
    draw_hline_px(surf, col, x0, shaft_end, y, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(surf, col, shaft_end + i, y - i, 1, (i * 2) + t);
    }
}

static void draw_arrow_left_px(SDL_Surface *surf, Uint32 col, int x0, int x1, int y, int t, int head) {
    int shaft_start = x0 + head;

    if (shaft_start > x1) shaft_start = x1;
    draw_hline_px(surf, col, shaft_start, x1, y, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(surf, col, x0 + i, y - (head - 1 - i), 1, (((head - 1 - i) * 2) + t));
    }
}

static void draw_arrow_down_px(SDL_Surface *surf, Uint32 col, int x, int y0, int y1, int t, int head) {
    int shaft_end = y1 - head;

    if (shaft_end < y0) shaft_end = y0;
    draw_vline_px(surf, col, x, y0, shaft_end, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(surf, col, x - i, shaft_end + i, (i * 2) + t, 1);
    }
}

static void draw_arrow_up_px(SDL_Surface *surf, Uint32 col, int x, int y0, int y1, int t, int head) {
    int shaft_start = y0 + head;

    if (shaft_start > y1) shaft_start = y1;
    draw_vline_px(surf, col, x, shaft_start, y1, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(surf, col, x - (head - 1 - i), y0 + i, (((head - 1 - i) * 2) + t), 1);
    }
}

static SDL_Surface *render_vt_soft_glyph(Uint32 cp, SDL_Color fg, int cell_w, int cell_h) {
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, cell_w, cell_h, 32, SDL_PIXELFORMAT_RGBA8888);
    Uint32 col;

    int cw = cell_w;
    int ch = cell_h;

    int cx = cw / 2;
    int cy = ch / 2;

    int light = vt_line_thickness(ch, 0);
    int heavy = vt_line_thickness(ch, 1);

    if (!surf) return NULL;

    SDL_FillRect(surf, NULL, 0);
    col = SDL_MapRGBA(surf->format, fg.r, fg.g, fg.b, 255);

    if (is_vt_box_char(cp)) {
        int l = 0, r = 0, u = 0, d = 0;
        int t = light;

        switch (cp) {
            case 0x2500:
                l = 1;
                r = 1;
                t = light;
                break;
            case 0x2501:
                l = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2502:
                u = 1;
                d = 1;
                t = light;
                break;
            case 0x2503:
                u = 1;
                d = 1;
                t = heavy;
                break;
            case 0x250C:
            case 0x250D:
            case 0x250E:
            case 0x250F:
                r = 1;
                d = 1;
                break;
            case 0x2510:
            case 0x2511:
            case 0x2512:
            case 0x2513:
                l = 1;
                d = 1;
                break;
            case 0x2514:
            case 0x2515:
            case 0x2516:
            case 0x2517:
                r = 1;
                u = 1;
                break;
            case 0x2518:
            case 0x2519:
            case 0x251A:
            case 0x251B:
                l = 1;
                u = 1;
                break;
            case 0x251C:
            case 0x251D:
            case 0x251E:
            case 0x251F:
            case 0x2520:
            case 0x2521:
            case 0x2522:
            case 0x2523:
                u = 1;
                d = 1;
                r = 1;
                break;
            case 0x2524:
            case 0x2525:
            case 0x2526:
            case 0x2527:
            case 0x2528:
            case 0x2529:
            case 0x252A:
            case 0x252B:
                u = 1;
                d = 1;
                l = 1;
                break;
            case 0x252C:
            case 0x252D:
            case 0x252E:
            case 0x252F:
            case 0x2530:
            case 0x2531:
            case 0x2532:
            case 0x2533:
                l = 1;
                r = 1;
                d = 1;
                break;
            case 0x2534:
            case 0x2535:
            case 0x2536:
            case 0x2537:
            case 0x2538:
            case 0x2539:
            case 0x253A:
            case 0x253B:
                l = 1;
                r = 1;
                u = 1;
                break;
            case 0x253C:
            case 0x253D:
            case 0x253E:
            case 0x253F:
            case 0x2540:
            case 0x2541:
            case 0x2542:
            case 0x2543:
            case 0x2544:
            case 0x2545:
            case 0x2546:
            case 0x2547:
            case 0x2548:
            case 0x2549:
            case 0x254A:
            case 0x254B:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                break;
            case 0x254C:
            case 0x254D:
            case 0x254E:
            case 0x254F:
                l = 1;
                r = 1;
                break;
            case 0x2550:
                l = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2551:
                u = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2554:
                r = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2557:
                l = 1;
                d = 1;
                t = heavy;
                break;
            case 0x255A:
                r = 1;
                u = 1;
                t = heavy;
                break;
            case 0x255D:
                l = 1;
                u = 1;
                t = heavy;
                break;
            case 0x2560:
                u = 1;
                d = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2563:
                u = 1;
                d = 1;
                l = 1;
                t = heavy;
                break;
            case 0x2566:
                l = 1;
                r = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2569:
                l = 1;
                r = 1;
                u = 1;
                t = heavy;
                break;
            case 0x256C:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                t = heavy;
                break;
            default:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                break;
        }

        if (l) draw_hline_mid(surf, col, cw, ch, 0, cx, t);
        if (r) draw_hline_mid(surf, col, cw, ch, cx, cw, t);
        if (u) draw_vline_mid(surf, col, cw, ch, 0, cy, t);
        if (d) draw_vline_mid(surf, col, cw, ch, cy, ch, t);

        return surf;
    }

    if (is_vt_block_char(cp)) {
        switch (cp) {
            case 0x2580: /* ▀ upper half */
                fill_rect_px(surf, col, 0, 0, cw, ch / 2);
                break;
            case 0x2584: /* ▄ lower half */
                fill_rect_px(surf, col, 0, ch / 2, cw, ch - (ch / 2));
                break;
            case 0x2588: /* █ full */
                fill_rect_px(surf, col, 0, 0, cw, ch);
                break;
            case 0x258C: /* ▌ left half */
                fill_rect_px(surf, col, 0, 0, cw / 2, ch);
                break;
            case 0x2590: /* ▐ right half */
                fill_rect_px(surf, col, cw / 2, 0, cw - (cw / 2), ch);
                break;
            case 0x2591: /* ░ light shade ~25% */
            {
                Uint32 *px = (Uint32 *) surf->pixels;
                int pitch4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++)
                    for (int x = 0; x < cw; x++)
                        if (((x + y * 2) & 3) == 0)
                            px[y * pitch4 + x] = col;
                break;
            }
            case 0x2592: /* ▒ medium shade ~50% */
            {
                Uint32 *px = (Uint32 *) surf->pixels;
                int pitch4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++)
                    for (int x = 0; x < cw; x++)
                        if (((x + y) & 1) == 0)
                            px[y * pitch4 + x] = col;
                break;
            }
            case 0x2593: /* ▓ dark shade ~75% */
            {
                Uint32 *px = (Uint32 *) surf->pixels;
                int pitch4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++)
                    for (int x = 0; x < cw; x++)
                        if (((x + y) & 1) != 0 || ((x & 1) == 0 && (y & 1) == 0))
                            px[y * pitch4 + x] = col;
                break;
            }
            case 0x2596: /* ▖ lower-left quadrant */
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - (ch / 2));
                break;
            case 0x2597: /* ▗ lower-right quadrant */
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - (cw / 2), ch - (ch / 2));
                break;
            case 0x2598: /* ▘ upper-left quadrant */
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                break;
            case 0x259D: /* ▝ upper-right quadrant */
                fill_rect_px(surf, col, cw / 2, 0, cw - (cw / 2), ch / 2);
                break;
            default:
                fill_rect_px(surf, col, 0, 0, cw, ch);
                break;
        }

        return surf;
    }

    if (is_vt_geom_char(cp)) {
        switch (cp) {
            case 0x25A0: /* ■ */
                fill_rect_px(surf, col, cw / 4, ch / 4, cw / 2, ch / 2);
                return surf;
            case 0x25CF: /* ● */
            {
                int x, y;
                Uint32 *px = (Uint32 *) surf->pixels;
                int radius = (cw < ch ? cw : ch) / 4;
                int ox = cw / 2;
                int oy = ch / 2;
                for (y = 0; y < ch; y++)
                    for (x = 0; x < cw; x++) {
                        int dx = x - ox, dy = y - oy;
                        if (dx * dx + dy * dy <= radius * radius)
                            px[y * surf->pitch / 4 + x] = col;
                    }
                return surf;
            }
            default:
                fill_rect_px(surf, col, cw / 4, ch / 4, cw / 2, ch / 2);
                return surf;
        }
    }

    if (is_vt_scanline_char(cp)) {
        int y;

        switch (cp) {
            case 0x23BA:
                y = ch / 6;
                break; /* ⎺ */
            case 0x23BB:
                y = (2 * ch) / 6;
                break; /* ⎻ */
            case 0x23BC:
                y = (4 * ch) / 6;
                break; /* ⎼ */
            case 0x23BD:
                y = (5 * ch) / 6;
                break; /* ⎽ */
            default:
                y = ch / 2;
                break;
        }

        if (y < 0) y = 0;
        if (y > ch - light) y = ch - light;

        draw_hline_px(surf, col, 0, cw, y, light);
        return surf;
    }

    if (is_vt_control_picture(cp)) {
        int t = light;
        int head = ch / 6;
        int left = cw / 6;
        int right = cw - (cw / 6);
        int top = ch / 6;
        int bottom = ch - (ch / 6);
        int mid_x = (cw - t) / 2;
        int mid_y = (ch - t) / 2;

        if (head < 2) head = 2;
        if (right <= left) right = left + t + head + 1;
        if (bottom <= top) bottom = top + t + head + 1;

        switch (cp) {
            case 0x2409: /* ␉ */
                draw_arrow_right_px(surf, col, left, right - t - 1, mid_y, t, head);
                draw_vline_px(surf, col, right - t, top, bottom, t);
                return surf;
            case 0x240A: /* ␊ */
                draw_arrow_down_px(surf, col, mid_x, top, bottom, t, head);
                return surf;
            case 0x240B: /* ␋ */
                draw_arrow_up_px(surf, col, mid_x, top, bottom, t, head);
                draw_arrow_down_px(surf, col, mid_x, top, bottom, t, head);
                return surf;
            case 0x240C: /* ␌ */
                draw_frame_px(surf, col, left, top, right - left, bottom - top, t);
                draw_hline_px(surf, col, left + t, right - t, bottom - (2 * t), t);
                return surf;
            case 0x240D: /* ␍ */
                draw_arrow_left_px(surf, col, left, right, mid_y, t, head);
                return surf;
            case 0x2424: /* ␤ */
                draw_vline_px(surf, col, right - t, top, mid_y + t, t);
                draw_arrow_left_px(surf, col, left, right, mid_y, t, head);
                return surf;
            default:
                break;
        }
    }

    if (cp == 0x00B7) {
        fill_rect_px(surf, col, cw / 2, ch / 2, 1, 1);
        return surf;
    }

    SDL_FreeSurface(surf);
    return NULL;
}

static GlyphEntry *glyph_cache_get(SDL_Renderer *ren, Uint32 cp, SDL_Color fg, Uint8 style) {
    GlyphEntry *ne;
    GlyphEntry *e;

    SDL_Surface *surf = NULL;
    TTF_Font *font;

    char utf8[8];
    Uint32 h;

    unsigned b;

    style &= 3;
    h = glyph_hash(cp, fg, style);
    b = (unsigned) (h % GLYPH_BUCKETS);

    for (e = glyph_table[b]; e; e = e->next) {
        if (e->codepoint == cp && e->style == style && colour_equal(e->fg, fg)) return e;
    }

    if (glyph_entries >= GLYPH_MAX_ENTRIES) {
        glyph_cache_clear();
        b = (unsigned) (h % GLYPH_BUCKETS);
    }

    ne = (GlyphEntry *) calloc(1, sizeof(*ne));
    if (!ne) return NULL;

    ne->codepoint = cp;
    ne->fg = fg;
    ne->style = style;

    if (is_vt_soft_char(cp)) {
        surf = render_vt_soft_glyph(cp, fg, CELL_WIDTH, CELL_HEIGHT);
    } else {
        (void) utf8_encode(utf8, cp);
        font = fonts[style];
        if (!font) {
            free(ne);
            return NULL;
        }
        surf = TTF_RenderUTF8_Blended(font, utf8, fg);
    }

    if (!surf) {
        free(ne);
        return NULL;
    }

    ne->tex = SDL_CreateTextureFromSurface(ren, surf);
    ne->w = surf->w;
    ne->h = surf->h;
    SDL_FreeSurface(surf);

    if (!ne->tex) {
        free(ne);
        return NULL;
    }

    ne->next = glyph_table[b];
    glyph_table[b] = ne;
    glyph_entries++;

    return ne;
}

typedef struct {
    const char *label;
    const char *send;
    int width;
} OskKey;

typedef struct {
    const OskKey *keys;
    int count;
} OskRow;

static const OskKey OSK_EMPTY = {NULL, NULL, 0};

#define OSK_LAYERS   3
#define OSK_ROWS     5
#define OSK_MAX_COLS 12
#define OSK_KEY_PAD  2
#define OSK_MARGIN   4

static const OskKey row_lower_0[] = {
        {"1", "1", 1},
        {"2", "2", 1},
        {"3", "3", 1},
        {"4", "4", 1},
        {"5", "5", 1},
        {"6", "6", 1},
        {"7", "7", 1},
        {"8", "8", 1},
        {"9", "9", 1},
        {"0", "0", 1},
        {"-", "-", 1},
        {"=", "=", 1}
};

static const OskKey row_lower_1[] = {
        {"q", "q", 1},
        {"w", "w", 1},
        {"e", "e", 1},
        {"r", "r", 1},
        {"t", "t", 1},
        {"y", "y", 1},
        {"u", "u", 1},
        {"i", "i", 1},
        {"o", "o", 1},
        {"p", "p", 1},
        {"[", "[", 1},
        {"]", "]", 1}
};

static const OskKey row_lower_2[] = {
        {"a",  "a",  1},
        {"s",  "s",  1},
        {"d",  "d",  1},
        {"f",  "f",  1},
        {"g",  "g",  1},
        {"h",  "h",  1},
        {"j",  "j",  1},
        {"k",  "k",  1},
        {"l",  "l",  1},
        {";",  ";",  1},
        {"'",  "'",  1},
        {"\\", "\\", 1}
};

static const OskKey row_lower_3[] = {
        {"z", "z", 1},
        {"x", "x", 1},
        {"c", "c", 1},
        {"v", "v", 1},
        {"b", "b", 1},
        {"n", "n", 1},
        {"m", "m", 1},
        {",", ",", 1},
        {".", ".", 1},
        {"/", "/", 1},
        {"$", "$", 1},
        {"`", "`", 1}
};

static const OskKey row_lower_4[] = {
        {"Tab",   "\t",     1},
        {"Esc",   "\x1B",   1},
        {"Ctrl",  "",       1},
        {"Alt",   "",       1},
        {"Up",    "\x1B[A", 2},
        {"Down",  "\x1B[B", 2},
        {"Left",  "\x1B[D", 2},
        {"Right", "\x1B[C", 2}
};

static const OskKey row_upper_0[] = {
        {"!", "!", 1},
        {"@", "@", 1},
        {"#", "#", 1},
        {"$", "$", 1},
        {"%", "%", 1},
        {"^", "^", 1},
        {"&", "&", 1},
        {"*", "*", 1},
        {"(", "(", 1},
        {")", ")", 1},
        {"_", "_", 1},
        {"+", "+", 1}
};

static const OskKey row_upper_1[] = {
        {"Q", "Q", 1},
        {"W", "W", 1},
        {"E", "E", 1},
        {"R", "R", 1},
        {"T", "T", 1},
        {"Y", "Y", 1},
        {"U", "U", 1},
        {"I", "I", 1},
        {"O", "O", 1},
        {"P", "P", 1},
        {"{", "{", 1},
        {"}", "}", 1}
};

static const OskKey row_upper_2[] = {
        {"A",  "A",  1},
        {"S",  "S",  1},
        {"D",  "D",  1},
        {"F",  "F",  1},
        {"G",  "G",  1},
        {"H",  "H",  1},
        {"J",  "J",  1},
        {"K",  "K",  1},
        {"L",  "L",  1},
        {":",  ":",  1},
        {"\"", "\"", 1},
        {"|",  "|",  1}
};

static const OskKey row_upper_3[] = {
        {"Z", "Z", 1},
        {"X", "X", 1},
        {"C", "C", 1},
        {"V", "V", 1},
        {"B", "B", 1},
        {"N", "N", 1},
        {"M", "M", 1},
        {"<", "<", 1},
        {">", ">", 1},
        {"?", "?", 1},
        {"~", "~", 1},
        {"`", "`", 1}
};

static const OskKey row_upper_4[] = {
        {"Tab",   "\t",     1},
        {"Esc",   "\x1B",   1},
        {"Ctrl",  "",       1},
        {"Alt",   "",       1},
        {"Up",    "\x1B[A", 2},
        {"Down",  "\x1B[B", 2},
        {"Left",  "\x1B[D", 2},
        {"Right", "\x1B[C", 2}
};

static const OskKey row_ctrl_0[] = {
        {"F1",  "\x1BOP",   1},
        {"F2",  "\x1BOQ",   1},
        {"F3",  "\x1BOR",   1},
        {"F4",  "\x1BOS",   1},
        {"F5",  "\x1B[15~", 1},
        {"F6",  "\x1B[17~", 1},
        {"F7",  "\x1B[18~", 1},
        {"F8",  "\x1B[19~", 1},
        {"F9",  "\x1B[20~", 1},
        {"F10", "\x1B[21~", 1},
        {"F11", "\x1B[23~", 1},
        {"F12", "\x1B[24~", 1}
};

static const OskKey row_ctrl_1[] = {
        {"C-a", "\x01", 1},
        {"C-b", "\x02", 1},
        {"C-c", "\x03", 1},
        {"C-d", "\x04", 1},
        {"C-e", "\x05", 1},
        {"C-f", "\x06", 1},
        {"C-g", "\x07", 1},
        {"C-h", "\x08", 1},
        {"C-i", "\x09", 1},
        {"C-j", "\x0A", 1},
        {"C-k", "\x0B", 1},
        {"C-l", "\x0C", 1}
};

static const OskKey row_ctrl_2[] = {
        {"C-m", "\x0D", 1},
        {"C-n", "\x0E", 1},
        {"C-o", "\x0F", 1},
        {"C-p", "\x10", 1},
        {"C-q", "\x11", 1},
        {"C-r", "\x12", 1},
        {"C-s", "\x13", 1},
        {"C-t", "\x14", 1},
        {"C-u", "\x15", 1},
        {"C-v", "\x16", 1},
        {"C-w", "\x17", 1},
        {"C-x", "\x18", 1}
};

static const OskKey row_ctrl_3[] = {
        {"C-y",  "\x19", 1},
        {"C-z",  "\x1A", 1},
        {"C-[",  "\x1B", 1},
        {"C-\\", "\x1C", 1},
        {"C-]",  "\x1D", 1},
        {"C-^",  "\x1E", 1},
        {"C-_",  "\x1F", 1}
};

static const OskKey row_ctrl_4[] = {
        {"Tab",   "\t",     1},
        {"Esc",   "\x1B",   1},
        {"Ctrl",  "",       1},
        {"Alt",   "",       1},
        {"Up",    "\x1B[A", 2},
        {"Down",  "\x1B[B", 2},
        {"Left",  "\x1B[D", 2},
        {"Right", "\x1B[C", 2}
};

static const OskRow layer_lower_rows[OSK_ROWS] = {
        {row_lower_0, sizeof(row_lower_0) / sizeof(OskKey)},
        {row_lower_1, sizeof(row_lower_1) / sizeof(OskKey)},
        {row_lower_2, sizeof(row_lower_2) / sizeof(OskKey)},
        {row_lower_3, sizeof(row_lower_3) / sizeof(OskKey)},
        {row_lower_4, sizeof(row_lower_4) / sizeof(OskKey)},
};

static const OskRow layer_upper_rows[OSK_ROWS] = {
        {row_upper_0, sizeof(row_upper_0) / sizeof(OskKey)},
        {row_upper_1, sizeof(row_upper_1) / sizeof(OskKey)},
        {row_upper_2, sizeof(row_upper_2) / sizeof(OskKey)},
        {row_upper_3, sizeof(row_upper_3) / sizeof(OskKey)},
        {row_upper_4, sizeof(row_upper_4) / sizeof(OskKey)},
};

static const OskRow layer_ctrl_rows[OSK_ROWS] = {
        {row_ctrl_0, sizeof(row_ctrl_0) / sizeof(OskKey)},
        {row_ctrl_1, sizeof(row_ctrl_1) / sizeof(OskKey)},
        {row_ctrl_2, sizeof(row_ctrl_2) / sizeof(OskKey)},
        {row_ctrl_3, sizeof(row_ctrl_3) / sizeof(OskKey)},
        {row_ctrl_4, sizeof(row_ctrl_4) / sizeof(OskKey)},
};

static OskKey layer_lower[OSK_ROWS][OSK_MAX_COLS];
static OskKey layer_upper[OSK_ROWS][OSK_MAX_COLS];
static OskKey layer_ctrl[OSK_ROWS][OSK_MAX_COLS];

static OskKey (*osk_layers[OSK_LAYERS])[OSK_MAX_COLS] = {
        layer_lower, layer_upper, layer_ctrl
};

static const char *layer_names[OSK_LAYERS] = {"abc", "ABC", "Ctrl"};

#define OSK_STATE_HIDDEN        0
#define OSK_STATE_BOTTOM_OPAQUE 1
#define OSK_STATE_BOTTOM_TRANS  2
#define OSK_STATE_TOP_OPAQUE    3
#define OSK_STATE_TOP_TRANS     4
#define OSK_NUM_STATES          5

static int osk_state = OSK_STATE_HIDDEN;

static int osk_visible = 0;
static int osk_layer = 0;
static int osk_sel_row = 0;
static int osk_sel_col = 0;
static int osk_ctrl = 0;
static int osk_alt = 0;

#define KEY_REPEAT_DELAY 350
#define KEY_REPEAT_RATE  70

typedef enum {
    INPUT_ACT_NONE = 0,
    INPUT_ACT_OSK_TOGGLE,
    INPUT_ACT_QUIT,
    INPUT_ACT_PRESS,
    INPUT_ACT_BACKSPACE,
    INPUT_ACT_SPACE,
    INPUT_ACT_LAYER_PREV,
    INPUT_ACT_LAYER_NEXT,
    INPUT_ACT_ENTER,
    INPUT_ACT_PAGE_UP,
    INPUT_ACT_PAGE_DOWN
} input_action_t;

static int osk_hold_active = 0;
static input_action_t osk_hold_action = INPUT_ACT_NONE;
static Uint32 osk_hold_press_t = 0;
static Uint32 osk_hold_last_rep = 0;

static int osk_key_w = 0;
static int osk_key_h = 0;
static int osk_height = 0;

static void osk_build_layer(const OskRow *rows, OskKey out[OSK_ROWS][OSK_MAX_COLS]) {
    for (int r = 0; r < OSK_ROWS; r++) {
        int c = 0;

        if (rows[r].keys) {
            for (int i = 0; i < rows[r].count && c < OSK_MAX_COLS; i++) {
                out[r][c++] = rows[r].keys[i];
            }
        }

        while (c < OSK_MAX_COLS) {
            out[r][c++] = OSK_EMPTY;
        }
    }
}

static void osk_init_layers(void) {
    osk_build_layer(layer_lower_rows, layer_lower);
    osk_build_layer(layer_upper_rows, layer_upper);
    osk_build_layer(layer_ctrl_rows, layer_ctrl);
}

static void osk_calc_metrics(int screen_w) {
    osk_key_w = (screen_w - 2 * OSK_MARGIN) / OSK_MAX_COLS - OSK_KEY_PAD;
    osk_key_h = CELL_HEIGHT + 6;
    osk_height = OSK_ROWS * (osk_key_h + OSK_KEY_PAD) + OSK_MARGIN * 2 + osk_key_h;

    fprintf(stderr, "[OSK] METRICS key_w=%d key_h=%d height=%d screen_w=%d\n",
            osk_key_w, osk_key_h, osk_height, screen_w);
}

static int osk_row_len(int layer, int row) {
    const OskKey *r = osk_layers[layer][row];
    int n = 0;

    for (int i = 0; i < OSK_MAX_COLS; i++) {
        if (!r[i].label) break;
        n++;
    }

    return n;
}

static int pty_fd_global = -1;

static void pty_write(const char *data, int len) {
    if (readonly_mode) return;
    if (pty_fd_global >= 0 && len > 0) {
        ssize_t r = write(pty_fd_global, data, (size_t) len);
        (void) r;
    }
}

static void osk_press_key(void) {
    const OskKey *key = &osk_layers[osk_layer][osk_sel_row][osk_sel_col];
    if (!key->label) return;

    if (strcmp(key->label, "Ctrl") == 0) {
        osk_ctrl = !osk_ctrl;
        return;
    }

    if (strcmp(key->label, "Alt") == 0) {
        osk_alt = !osk_alt;
        return;
    }

    const char *seq = key->send;
    char dyn_seq[8];

    if (strcmp(key->label, "Up") == 0 || strcmp(key->label, "Down") == 0 || strcmp(key->label, "Left") == 0 || strcmp(key->label, "Right") == 0) {
        char code = 0;

        switch (key->label[0]) {
            case 'U':
                code = 'A';
                break;
            case 'D':
                code = 'B';
                break;
            case 'R':
                code = 'C';
                break;
            case 'L':
                code = 'D';
                break;
        }

        if (cursor_keys_application) {
            snprintf(dyn_seq, sizeof(dyn_seq), "\x1BO%c", code);
        } else {
            snprintf(dyn_seq, sizeof(dyn_seq), "\x1B[%c", code);
        }

        seq = dyn_seq;
    }

    int str_len = (int) strlen(seq);
    if (str_len <= 0) return;

    if (osk_alt) pty_write("\x1B", 1);

    if (osk_ctrl && str_len == 1) {
        char ch = seq[0];

        if (ch >= 'a' && ch <= 'z') {
            ch = (char) (ch - 'a' + 1);
        } else if (ch >= 'A' && ch <= 'Z') {
            ch = (char) (ch - 'A' + 1);
        }

        pty_write(&ch, 1);
    } else {
        pty_write(seq, str_len);
    }

    osk_ctrl = 0;
    osk_alt = 0;
}

static void osk_move(int drow, int dcol) {
    int orig = osk_sel_row;
    osk_sel_row += drow;

    for (int t = 0; t < OSK_ROWS; t++) {
        if (osk_sel_row < 0) osk_sel_row = OSK_ROWS - 1;
        if (osk_sel_row >= OSK_ROWS) osk_sel_row = 0;
        if (osk_row_len(osk_layer, osk_sel_row) > 0) break;
        osk_sel_row += (drow != 0) ? drow : 1;
        if (osk_sel_row < 0) osk_sel_row = OSK_ROWS - 1;
        if (osk_sel_row >= OSK_ROWS) osk_sel_row = 0;
    }

    int rlen = osk_row_len(osk_layer, osk_sel_row);
    if (rlen == 0) {
        osk_sel_row = orig;
        rlen = osk_row_len(osk_layer, osk_sel_row);
    }

    osk_sel_col += dcol;
    if (osk_sel_col < 0) osk_sel_col = rlen - 1;
    if (osk_sel_col >= rlen) osk_sel_col = 0;
}

static void osk_switch_layer(int delta) {
    osk_layer = (osk_layer + delta + OSK_LAYERS) % OSK_LAYERS;
    int rlen = osk_row_len(osk_layer, osk_sel_row);

    if (rlen == 0) {
        for (int r = 0; r < OSK_ROWS; r++) {
            if (osk_row_len(osk_layer, r) > 0) {
                osk_sel_row = r;
                rlen = osk_row_len(osk_layer, r);
                break;
            }
        }
    }

    if (osk_sel_col >= rlen) osk_sel_col = rlen > 0 ? rlen - 1 : 0;
    if (osk_sel_col < 0) osk_sel_col = 0;
}

static void render_osk(SDL_Renderer *ren, int screen_w, int screen_h) {
    if (!osk_visible) return;

    if (osk_height <= 0) {
        fprintf(stderr, "[OSK] ERROR: osk_height=0 (metrics not initialised)\n");
        return;
    }

    int osk_at_top = (osk_state == OSK_STATE_TOP_OPAQUE || osk_state == OSK_STATE_TOP_TRANS);
    int osk_transparent = (osk_state == OSK_STATE_BOTTOM_TRANS || osk_state == OSK_STATE_TOP_TRANS);

    Uint8 backdrop_alpha = osk_transparent ? 115 : 230;
    Uint8 widget_alpha = osk_transparent ? 128 : 255;

    int osk_y0 = osk_at_top ? 0 : (screen_h - osk_height);

    SDL_Color accent = {255, 200, 0, 255};
    SDL_Color key_dark = {40, 40, 40, 255};
    SDL_Color key_ctrl = {200, 100, 60, 255};
    SDL_Color key_sel = {255, 200, 0, 255};
    SDL_Color lbl_norm = {255, 200, 0, 255};
    SDL_Color lbl_sel = {0, 0, 0, 255};

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_Rect backdrop = {0, osk_y0, screen_w, osk_height};
    SDL_SetRenderDrawColor(ren, 18, 18, 18, backdrop_alpha);
    SDL_RenderFillRect(ren, &backdrop);

    {
        char info[64];
        char mod_buf[32] = {0};

        if (osk_ctrl) strcat(mod_buf, " [CTRL]");
        if (osk_alt) strcat(mod_buf, " [ALT]");

        snprintf(info, sizeof(info), " Layer: %s  %s", layer_names[osk_layer], mod_buf);
        SDL_Surface *s = TTF_RenderUTF8_Blended(fonts[0], info, accent);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_SetTextureAlphaMod(t, widget_alpha);
            SDL_Rect dst = {OSK_MARGIN, osk_y0 + 2, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &dst);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    int keys_y0 = osk_y0 + osk_key_h + OSK_MARGIN;

    for (int row = 0; row < OSK_ROWS; row++) {
        int rlen = osk_row_len(osk_layer, row);
        if (rlen == 0) continue;

        int total_units = 0;
        for (int c = 0; c < rlen; c++) total_units += osk_layers[osk_layer][row][c].width;

        int avail_w = screen_w - 2 * OSK_MARGIN - (rlen - 1) * OSK_KEY_PAD;
        int unit_px = (total_units > 0) ? avail_w / total_units : osk_key_w;

        int total_w = 0;
        for (int c = 0; c < rlen; c++) total_w += osk_layers[osk_layer][row][c].width * unit_px;
        total_w += (rlen - 1) * OSK_KEY_PAD;
        int x = (screen_w - total_w) / 2;
        int y = keys_y0 + row * (osk_key_h + OSK_KEY_PAD);

        for (int col = 0; col < rlen; col++) {
            const OskKey *key = &osk_layers[osk_layer][row][col];
            int kw = key->width * unit_px;
            SDL_Rect kr = {x, y, kw, osk_key_h};
            int sel = (row == osk_sel_row && col == osk_sel_col);

            int is_ctrl = strcmp(key->label, "Ctrl") == 0;
            int is_alt = strcmp(key->label, "Alt") == 0;
            SDL_Color fill = sel ? key_sel : ((is_ctrl && osk_ctrl) || (is_alt && osk_alt)) ? key_ctrl : key_dark;

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, fill.r, fill.g, fill.b, widget_alpha);
            SDL_RenderFillRect(ren, &kr);
            SDL_SetRenderDrawColor(ren, 90, 90, 110, widget_alpha);
            SDL_RenderDrawRect(ren, &kr);

            SDL_Color lc = sel ? lbl_sel : lbl_norm;
            SDL_Surface *ls = TTF_RenderUTF8_Blended(fonts[0], key->label, lc);
            if (ls) {
                SDL_Texture *lt = SDL_CreateTextureFromSurface(ren, ls);
                SDL_SetTextureAlphaMod(lt, widget_alpha);
                SDL_Rect ld = {x + (kw - ls->w) / 2, y + (osk_key_h - ls->h) / 2, ls->w, ls->h};
                SDL_RenderCopy(ren, lt, NULL, &ld);
                SDL_DestroyTexture(lt);
                SDL_FreeSurface(ls);
            }
            x += kw + OSK_KEY_PAD;
        }
    }
}

static void render_screen_to_target(SDL_Renderer *ren, SDL_Texture *target,
                                    SDL_Texture *bg_tex, int screen_w, int screen_h,
                                    int vis_rows) {
    SDL_SetRenderTarget(ren, target);

    if (bg_tex) {
        SDL_RenderCopy(ren, bg_tex, NULL, NULL);
    } else if (use_solid_bg) {
        SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
        SDL_RenderClear(ren);
    } else {
        SDL_SetRenderDrawColor(ren, default_bg.r, default_bg.g, default_bg.b, 255);
        SDL_RenderClear(ren);
    }

#define RENDER_CELL(cell_, px_, py_) do { \
        if ((cell_)->width == 0) break; \
        SDL_Color _fg = use_solid_fg ? solid_fg : (cell_)->fg; \
        SDL_Color _bg = (cell_)->bg; \
        if ((cell_)->style & STYLE_REVERSE) { SDL_Color _t = _fg; _fg = _bg; _bg = _t; } \
        int _w = ((cell_)->width > 0 ? (cell_)->width : 1); \
        int _px_w = CELL_WIDTH * _w; \
        \
        if (_bg.r != default_bg.r || _bg.g != default_bg.g || _bg.b != default_bg.b) { \
            SDL_Rect _br = {(px_), (py_), _px_w, CELL_HEIGHT}; \
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE); \
            SDL_SetRenderDrawColor(ren, _bg.r, _bg.g, _bg.b, 255); \
            SDL_RenderFillRect(ren, &_br); \
        } \
        \
        if ((cell_)->codepoint != (Uint32)' ') { \
            GlyphEntry *_g = glyph_cache_get(ren, (cell_)->codepoint, _fg, (Uint8)((cell_)->style & 3)); \
            if (_g) { \
                SDL_Rect _d = {(px_), (py_), _px_w, CELL_HEIGHT}; \
                SDL_RenderCopy(ren, _g->tex, NULL, &_d); \
            } \
        } \
    } while(0)

    if (scroll_offset > 0) {
        int sb_show = scroll_offset < sb_count ? scroll_offset : sb_count;
        if (sb_show > vis_rows) sb_show = vis_rows;
        int grid_show = vis_rows - sb_show;

        for (int vr = 0; vr < sb_show; vr++) {
            const Cell *row = scrollback_row(sb_count - sb_show + vr);
            if (!row) continue;

            for (int c = 0; c < TERM_COLS; c++) {
                const Cell *cell = &row[c];
                RENDER_CELL(cell, c * CELL_WIDTH, vr * CELL_HEIGHT);
                if (cell->width == 2) c++;
            }
        }

        for (int vr = 0; vr < grid_show; vr++) {
            if (vr >= TERM_ROWS) break;

            for (int c = 0; c < TERM_COLS; c++) {
                Cell *cell = CELL(vr, c);
                RENDER_CELL(cell, c * CELL_WIDTH, (sb_show + vr) * CELL_HEIGHT);
                if (cell->width == 2) c++;
            }
        }

    } else {
        for (int r = 0; r < vis_rows; r++) {
            for (int c = 0; c < TERM_COLS; c++) {
                Cell *cell = CELL(r, c);
                RENDER_CELL(cell, c * CELL_WIDTH, r * CELL_HEIGHT);
                if (cell->width == 2) c++;
            }
        }

        if (cursor_vis && !readonly_mode && cursor_row < vis_rows) {
            Uint32 ticks = SDL_GetTicks();

            if ((ticks % 1000) < 500) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

                int w = 1;
                Cell *c = CELL(cursor_row, cursor_col);
                if (c->width == 2) w = 2;

                SDL_Rect cr = {cursor_col * CELL_WIDTH, cursor_row * CELL_HEIGHT, CELL_WIDTH * w, CELL_HEIGHT};
                SDL_SetRenderDrawColor(ren, default_fg.r, default_fg.g, default_fg.b, 160);
                SDL_RenderFillRect(ren, &cr);
            }
        }
    }

#undef RENDER_CELL

    int top_right_y = 4;

    if (scroll_offset > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "^ %d lines", scroll_offset);

        SDL_Color ic = (SDL_Color) {255, 200, 0, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(fonts[0], buf, ic);

        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {screen_w - s->w - 8, top_right_y, s->w, s->h};

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            SDL_Rect bg = {dst.x - 4, dst.y - 2, dst.w + 8, dst.h + 4};
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_RenderFillRect(ren, &bg);

            SDL_RenderCopy(ren, t, NULL, &dst);

            top_right_y += s->h + 4;

            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    if (readonly_mode) {
        SDL_Color rc = (SDL_Color) {255, 100, 60, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(fonts[0], "[RO]", rc);

        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect dst = {screen_w - s->w - 8, top_right_y, s->w, s->h};

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            SDL_Rect bg = {dst.x - 4, dst.y - 2, dst.w + 8, dst.h + 4};
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_RenderFillRect(ren, &bg);

            SDL_RenderCopy(ren, t, NULL, &dst);

            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }

    SDL_SetRenderTarget(ren, NULL);
}

static void handle_keyboard(SDL_KeyboardEvent *key) {
    if (readonly_mode) return;

    SDL_Keycode sym = key->keysym.sym;
    SDL_Keymod mods = SDL_GetModState();
    int ctrl = (mods & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
    int shift = (mods & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0;
    int alt = (mods & (KMOD_LALT | KMOD_RALT)) != 0;

    if (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_RIGHT || sym == SDLK_LEFT) {
        char seq[4] = {'\x1B', cursor_keys_application ? 'O' : '[', 'A', '\0'};

        if (sym == SDLK_UP) seq[2] = 'A';
        else if (sym == SDLK_DOWN) seq[2] = 'B';
        else if (sym == SDLK_RIGHT) seq[2] = 'C';
        else seq[2] = 'D';

        pty_write(seq, 3);
        return;
    }

    if (sym == SDLK_HOME) {
        pty_write("\x1B[H", 3);
        return;
    }

    if (sym == SDLK_END) {
        pty_write("\x1B[F", 3);
        return;
    }

    if (sym == SDLK_PAGEUP) {
        pty_write("\x1B[5~", 4);
        return;
    }

    if (sym == SDLK_PAGEDOWN) {
        pty_write("\x1B[6~", 4);
        return;
    }

    if (sym == SDLK_INSERT) {
        pty_write("\x1B[2~", 4);
        return;
    }

    if (sym == SDLK_DELETE) {
        pty_write("\x1B[3~", 4);
        return;
    }

    static const char *fkeys[] = {
            "\x1BOP", "\x1BOQ", "\x1BOR", "\x1BOS",
            "\x1B[15~", "\x1B[17~", "\x1B[18~", "\x1B[19~", "\x1B[20~", "\x1B[21~",
            "\x1B[23~", "\x1B[24~"
    };

    if (sym >= SDLK_F1 && sym <= SDLK_F12) {
        int fi = sym - SDLK_F1;
        pty_write(fkeys[fi], (int) strlen(fkeys[fi]));
        return;
    }

    if (ctrl) {
        if (sym >= SDLK_a && sym <= SDLK_z) {
            char cc = (char) (sym - SDLK_a + 1);
            pty_write(&cc, 1);
            return;
        }

        if (sym == SDLK_SPACE) {
            char cc = 0;
            pty_write(&cc, 1);
            return;
        }

        if (sym == SDLK_BACKSLASH) {
            char cc = 0x1C;
            pty_write(&cc, 1);
            return;
        }

        if (sym == SDLK_RIGHTBRACKET) {
            char cc = 0x1D;
            pty_write(&cc, 1);
            return;
        }
    }

    if (alt) {
        char esc = '\x1B';
        pty_write(&esc, 1);
    }

    switch (sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER: {
            char c = '\r';
            pty_write(&c, 1);
            return;
        }
        case SDLK_BACKSPACE: {
            char c = '';
            pty_write(&c, 1);
            return;
        }
        case SDLK_TAB:
            if (shift) {
                pty_write("\x1B[Z", 3);
            } else {
                char c = '\t';
                pty_write(&c, 1);
            }
            return;
        case SDLK_ESCAPE: {
            char c = '\x1B';
            pty_write(&c, 1);
            return;
        }
        default:
            break;
    }
}

static void print_help(const char *name) {
    printf("MustardOS Virtual Terminal (muterm)\n\n");
    printf("Usage:\n\t%s [options] <command>\n\n", name);

    printf("Options:\n");
    printf("\t-w, --width <px>         Screen width  (default: device width)\n");
    printf("\t-h, --height <px>        Screen height (default: device height)\n");
    printf("\t-s, --size <pt>          Font size     (default: 16, 28 on tui-brick)\n");
    printf("\t-f, --font <path.ttf>    Font path     (default: " OPT_PATH "share/font/muterm.ttf)\n");
    printf("\t-i, --image <file>       Background image (PNG)\n");
    printf("\t-sb,--scrollback <n>     Scrollback lines (default: %d)\n", DEFAULT_SCROLLBACK);
    printf("\t-bg,--bgcolour RRGGBB    Solid background colour\n");
    printf("\t-fg,--fgcolour RRGGBB    Solid foreground colour (overrides ANSI)\n");
    printf("\t-ro,--readonly           View-only: display PTY output, no input\n\n");

    printf("Global controls:\n");
    printf("\t Select       Cycle OSK (bottom → bottom 50%% → top → top 50%% → hide)\n");
    printf("\t Menu         Quit\n\n");

    printf("When OSK is active:\n");
    printf("\t A            Select key\n");
    printf("\t B            Backspace\n");
    printf("\t Y            Space\n");
    printf("\t Start        Enter\n");
    printf("\t L1 / R1      Switch OSK layer\n");
    printf("\t D-pad        Navigate OSK\n");
    printf("\t Left Stick   Navigate OSK\n\n");
    printf("\t Vol+         Page up\n");
    printf("\t Vol-         Page down\n\n");

    printf("When OSK is hidden:\n");
    printf("\t D-pad Up     Scroll up\n");
    printf("\t D-pad Down   Scroll down\n");
    printf("\t D-pad Left   Cursor left\n");
    printf("\t D-pad Right  Cursor right\n");

    printf("Keyboard shortcuts:\n");
    printf("\t Enter        Return / execute\n");
    printf("\t Backspace    Delete character\n");
    printf("\t PgUp/PgDn    Scroll history\n\n");

    exit(0);
}

static int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (!hex || strlen(hex) != 6) return 0;
    char r_s[3] = {hex[0], hex[1], '\0'}, g_s[3] = {hex[2], hex[3], '\0'}, b_s[3] = {hex[4], hex[5], '\0'};
    char *e;
    errno = 0;
    unsigned long r = strtoul(r_s, &e, 16);
    if (errno || *e || r > 255) return 0;
    unsigned long g = strtoul(g_s, &e, 16);
    if (errno || *e || g > 255) return 0;
    unsigned long b = strtoul(b_s, &e, 16);
    if (errno || *e || b > 255) return 0;
    out->r = (Uint8) r;
    out->g = (Uint8) g;
    out->b = (Uint8) b;
    out->a = 255;
    return 1;
}

static volatile sig_atomic_t child_exited = 0;

static void sigchld_handler(int s) {
    (void) s;
    child_exited = 1;
}

static pid_t spawn_pty_child(int *master_fd_out, int argc, char **argv) {
    int master_fd = -1;
    int slave_fd = -1;
    pid_t pid;

    struct winsize ws = {
            .ws_row = TERM_ROWS,
            .ws_col = TERM_COLS,
            .ws_xpixel = 0,
            .ws_ypixel = 0
    };

    if (openpty(&master_fd, &slave_fd, NULL, NULL, &ws) < 0) {
        perror("openpty");
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(master_fd);
        close(slave_fd);
        return -1;
    }

    if (pid == 0) {
        setsid();

        if (ioctl(slave_fd, TIOCSCTTY, 0) < 0) {
            perror("TIOCSCTTY");
        }

        if (dup2(slave_fd, STDIN_FILENO) < 0 ||
            dup2(slave_fd, STDOUT_FILENO) < 0 ||
            dup2(slave_fd, STDERR_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }

        close(master_fd);
        close(slave_fd);

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        if (!getenv("HOME")) {
            setenv("HOME", "/root", 1);
        }

        if (argc > 1 && argv[1] && argv[1][0]) {
            execvp(argv[1], &argv[1]);
            perror("execvp");
            _exit(127);
        }

        const char *shell = getenv("SHELL");
        if (!shell || !*shell) shell = "/bin/sh";

        execlp(shell, shell, "-l", (char *) NULL);
        perror("execlp");
        _exit(127);
    }

    close(slave_fd);
    *master_fd_out = master_fd;
    return pid;
}

static void osk_hold_start(input_action_t action) {
    if (!osk_hold_active) {
        osk_hold_active = 1;
        osk_hold_action = action;
        osk_hold_press_t = SDL_GetTicks();
        osk_hold_last_rep = osk_hold_press_t;
    }
}

static void osk_hold_end(void) {
    osk_hold_active = 0;
    osk_hold_action = INPUT_ACT_NONE;
}

static void osk_reset_axis_state(void) {
    axis_x_state = 0;
    axis_y_state = 0;
}

static int event_from_active_controller(const SDL_Event *e, SDL_GameController *controller) {
    SDL_Joystick *joy;
    SDL_JoystickID which;

    if (!controller || !e) return 0;

    joy = SDL_GameControllerGetJoystick(controller);
    if (!joy) return 0;

    which = SDL_JoystickInstanceID(joy);

    switch (e->type) {
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            return e->cbutton.which == which;
        case SDL_JOYAXISMOTION:
            return e->jaxis.which == which;
        case SDL_JOYHATMOTION:
            return e->jhat.which == which;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            return e->jbutton.which == which;
        default:
            return 0;
    }
}

static input_action_t map_controller_button_down(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_BACK:
            return INPUT_ACT_OSK_TOGGLE;
        case SDL_CONTROLLER_BUTTON_GUIDE:
            return INPUT_ACT_QUIT;
        case SDL_CONTROLLER_BUTTON_A:
            return INPUT_ACT_PRESS;
        case SDL_CONTROLLER_BUTTON_B:
            return INPUT_ACT_BACKSPACE;
        case SDL_CONTROLLER_BUTTON_Y:
            return INPUT_ACT_SPACE;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return INPUT_ACT_LAYER_PREV;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return INPUT_ACT_LAYER_NEXT;
        case SDL_CONTROLLER_BUTTON_START:
            return INPUT_ACT_ENTER;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            return INPUT_ACT_PRESS;
        default:
            return INPUT_ACT_NONE;
    }
}

static void handle_controller_dpad(Uint8 button) {
    if (!osk_visible) return;

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            fprintf(stderr, "[OSK] DPAD UP\n");
            osk_move(-1, 0);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            fprintf(stderr, "[OSK] DPAD DOWN\n");
            osk_move(1, 0);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            fprintf(stderr, "[OSK] DPAD LEFT\n");
            osk_move(0, -1);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            fprintf(stderr, "[OSK] DPAD RIGHT\n");
            osk_move(0, 1);
            break;
        default:
            return;
    }

    screen_dirty = 1;
}

static void handle_controller_axis(const SDL_Event *e) {
    const int dead = 16000;
    int axis = e->caxis.axis;
    int val = e->caxis.value;

    if (!osk_visible) return;

    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
        if (val < -dead && axis_x_state != -1) {
            axis_x_state = -1;
            fprintf(stderr, "[OSK] LEFT (STICK)\n");
            osk_move(0, -1);
            screen_dirty = 1;
        } else if (val > dead && axis_x_state != 1) {
            axis_x_state = 1;
            fprintf(stderr, "[OSK] RIGHT (STICK)\n");
            osk_move(0, 1);
            screen_dirty = 1;
        } else if (val >= -dead && val <= dead) {
            axis_x_state = 0;
        }
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
        if (val < -dead && axis_y_state != -1) {
            axis_y_state = -1;
            fprintf(stderr, "[OSK] UP (STICK)\n");
            osk_move(-1, 0);
            screen_dirty = 1;
        } else if (val > dead && axis_y_state != 1) {
            axis_y_state = 1;
            fprintf(stderr, "[OSK] DOWN (STICK)\n");
            osk_move(1, 0);
            screen_dirty = 1;
        } else if (val >= -dead && val <= dead) {
            axis_y_state = 0;
        }
    }
}

static input_action_t map_joystick_button_down(int raw) {
    switch (raw) {
        case 9:
            return INPUT_ACT_OSK_TOGGLE;
        case 11:
            return INPUT_ACT_QUIT;
        case 3:
            return INPUT_ACT_PRESS;
        case 4:
            return INPUT_ACT_BACKSPACE;
        case 5:
            return INPUT_ACT_SPACE;
        case 7:
            return INPUT_ACT_LAYER_PREV;
        case 8:
            return INPUT_ACT_LAYER_NEXT;
        case 10:
            return INPUT_ACT_ENTER;
        case 2:
            return INPUT_ACT_PAGE_UP;
        case 1:
            return INPUT_ACT_PAGE_DOWN;
        default:
            return INPUT_ACT_NONE;
    }
}

static void apply_input_action(
        input_action_t action,
        int *running,
        int *vis_rows,
        int term_height
) {
    switch (action) {
        case INPUT_ACT_NONE:
            break;
        case INPUT_ACT_OSK_TOGGLE:
            osk_state = (osk_state + 1) % OSK_NUM_STATES;
            osk_visible = (osk_state != OSK_STATE_HIDDEN);

            fprintf(stderr, "[OSK] STATE -> %d visible=%d\n", osk_state, osk_visible);

            if (osk_state == OSK_STATE_BOTTOM_OPAQUE) {
                *vis_rows = (term_height - osk_height) / CELL_HEIGHT;
            } else {
                *vis_rows = TERM_ROWS;
            }

            if (*vis_rows < 1) *vis_rows = 1;

            scroll_offset = 0;
            osk_reset_axis_state();
            screen_dirty = 1;
            break;
        case INPUT_ACT_QUIT:
            fprintf(stderr, "[OSK] QUIT\n");
            *running = 0;
            break;
        case INPUT_ACT_PRESS:
            if (!osk_visible) break;

            fprintf(stderr, "[OSK] PRESS KEY\n");
            osk_press_key();

            osk_hold_start(INPUT_ACT_PRESS);

            scroll_offset = 0;
            screen_dirty = 1;
            break;
        case INPUT_ACT_BACKSPACE:
            fprintf(stderr, "[OSK] BACKSPACE\n");

            if (!readonly_mode) {
                char bs = '\x7F';
                pty_write(&bs, 1);
            }

            osk_hold_start(INPUT_ACT_BACKSPACE);
            break;
        case INPUT_ACT_SPACE:
            fprintf(stderr, "[OSK] SPACE\n");

            if (!readonly_mode) {
                char sp = ' ';
                pty_write(&sp, 1);
            }

            osk_hold_start(INPUT_ACT_SPACE);
            break;
        case INPUT_ACT_LAYER_PREV:
            if (!osk_visible) break;

            fprintf(stderr, "[OSK] LAYER -1\n");
            osk_switch_layer(-1);
            screen_dirty = 1;
            break;
        case INPUT_ACT_LAYER_NEXT:
            if (!osk_visible) break;

            fprintf(stderr, "[OSK] LAYER +1\n");
            osk_switch_layer(1);
            screen_dirty = 1;
            break;
        case INPUT_ACT_ENTER:
            fprintf(stderr, "[OSK] ENTER\n");
            if (!readonly_mode) {
                pty_write("\r", 1);
            }
            break;
        case INPUT_ACT_PAGE_UP:
            fprintf(stderr, "[OSK] PAGE UP\n");
            scroll_offset += *vis_rows / 2;
            if (scroll_offset > sb_count) scroll_offset = sb_count;
            screen_dirty = 1;
            break;
        case INPUT_ACT_PAGE_DOWN:
            fprintf(stderr, "[OSK] PAGE DOWN\n");
            scroll_offset -= *vis_rows / 2;
            if (scroll_offset < 0) scroll_offset = 0;
            screen_dirty = 1;
            break;
    }
}

static void handle_joy_axis_event(const SDL_Event *e) {
    const int dead = 16000;
    int axis = e->jaxis.axis;
    int val = e->jaxis.value;

    if (!osk_visible) return;

    if (axis == 0) {
        if (val < -dead && axis_x_state != -1) {
            axis_x_state = -1;
            fprintf(stderr, "[OSK] LEFT\n");
            osk_move(0, -1);
            screen_dirty = 1;
        } else if (val > dead && axis_x_state != 1) {
            axis_x_state = 1;
            fprintf(stderr, "[OSK] RIGHT\n");
            osk_move(0, 1);
            screen_dirty = 1;
        } else if (val >= -dead && val <= dead) {
            axis_x_state = 0;
        }
    } else if (axis == 1) {
        if (val < -dead && axis_y_state != -1) {
            axis_y_state = -1;
            fprintf(stderr, "[OSK] UP\n");
            osk_move(-1, 0);
            screen_dirty = 1;
        } else if (val > dead && axis_y_state != 1) {
            axis_y_state = 1;
            fprintf(stderr, "[OSK] DOWN\n");
            osk_move(1, 0);
            screen_dirty = 1;
        } else if (val >= -dead && val <= dead) {
            axis_y_state = 0;
        }
    }
}

static void handle_joy_hat_event(const SDL_Event *e) {
    Uint8 hat = e->jhat.value;

    if (!osk_visible) return;

    fprintf(stderr, "[OSK] HAT=%d\n", hat);

    if (hat & SDL_HAT_UP) {
        osk_move(-1, 0);
        fprintf(stderr, "[OSK] DPAD UP\n");
    } else if (hat & SDL_HAT_DOWN) {
        osk_move(1, 0);
        fprintf(stderr, "[OSK] DPAD DOWN\n");
    } else if (hat & SDL_HAT_LEFT) {
        osk_move(0, -1);
        fprintf(stderr, "[OSK] DPAD LEFT\n");
    } else if (hat & SDL_HAT_RIGHT) {
        osk_move(0, 1);
        fprintf(stderr, "[OSK] DPAD RIGHT\n");
    }

    screen_dirty = 1;
}

static void handle_button_down_event(const SDL_Event *e, SDL_GameController *controller, int *running, int *vis_rows, int term_height) {
    input_action_t action;

    if (e->type == SDL_CONTROLLERBUTTONDOWN) {
        handle_controller_dpad(e->cbutton.button);
        action = map_controller_button_down(e->cbutton.button);
        apply_input_action(action, running, vis_rows, term_height);
        return;
    }

    if (e->type == SDL_JOYBUTTONDOWN) {
        int raw = (int) e->jbutton.button;
        if (raw == 1 || raw == 2) {
            action = map_joystick_button_down(raw);
            apply_input_action(action, running, vis_rows, term_height);
            return;
        }
    }

    if (controller) return;

    action = map_joystick_button_down(e->jbutton.button);
    if (action == INPUT_ACT_NONE) return;

    apply_input_action(action, running, vis_rows, term_height);
}

static void handle_button_up_event(const SDL_Event *e, SDL_GameController *controller) {
    if (e->type == SDL_CONTROLLERBUTTONUP) {
        fprintf(stderr, "[OSK] CTRL UP BTN=%u\n", (unsigned) e->cbutton.button);

        if (e->cbutton.button == SDL_CONTROLLER_BUTTON_A ||
            e->cbutton.button == SDL_CONTROLLER_BUTTON_B ||
            e->cbutton.button == SDL_CONTROLLER_BUTTON_Y ||
            e->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK) {
            fprintf(stderr, "[OSK] PRESS RELEASE\n");
            osk_hold_end();
        }

        return;
    }

    if (controller) return;

    if (e->type == SDL_JOYBUTTONUP) {
        if (e->jbutton.button == 3) {
            fprintf(stderr, "[OSK] JOY PRESS RELEASE\n");
            osk_hold_end();
        }
    }
}

static void reopen_controller(SDL_GameController **controller) {
    if (*controller) {
        SDL_GameControllerClose(*controller);
        *controller = NULL;
    }

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            *controller = SDL_GameControllerOpen(i);
            if (*controller) {
                fprintf(stderr, "[OSK] Opened GameController %d: %s\n", i, SDL_GameControllerName(*controller));
                break;
            }
        }
    }
}

static void handle_sdl_event(const SDL_Event *e, SDL_GameController **controller, int *running,
                             int shell_dead, int *vis_rows, int term_width, int term_height) {
    switch (e->type) {
        case SDL_QUIT:
            *running = 0;
            return;
        case SDL_TEXTINPUT:
            if (!readonly_mode && !osk_visible) {
                pty_write(e->text.text, (int) strlen(e->text.text));
                scroll_offset = 0;
            }
            return;
        case SDL_KEYDOWN:
            if (e->key.keysym.sym == SDLK_ESCAPE &&
                !(SDL_GetModState() & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))) {
                if (shell_dead) {
                    *running = 0;
                    return;
                }
            }

            if (e->key.keysym.sym == SDLK_PAGEUP) {
                scroll_offset += *vis_rows / 2;
                if (scroll_offset > sb_count) scroll_offset = sb_count;
                screen_dirty = 1;
                return;
            }

            if (e->key.keysym.sym == SDLK_PAGEDOWN) {
                scroll_offset -= *vis_rows / 2;
                if (scroll_offset < 0) scroll_offset = 0;
                screen_dirty = 1;
                return;
            }

            handle_keyboard((SDL_KeyboardEvent *) &e->key);
            screen_dirty = 1;
            return;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_JOYBUTTONDOWN:
            handle_button_down_event(e, *controller, running, vis_rows, term_height);
            return;
        case SDL_CONTROLLERBUTTONUP:
        case SDL_JOYBUTTONUP:
            handle_button_up_event(e, *controller);
            return;
        case SDL_JOYAXISMOTION:
            if (*controller && event_from_active_controller(e, *controller)) return;
            handle_joy_axis_event(e);
            return;
        case SDL_CONTROLLERAXISMOTION:
            handle_controller_axis(e);
            return;
        case SDL_JOYHATMOTION:
            if (*controller) return;
            handle_joy_hat_event(e);
            return;
        case SDL_CONTROLLERDEVICEADDED:
            if (!*controller) reopen_controller(controller);
            return;
        case SDL_CONTROLLERDEVICEREMOVED:
            reopen_controller(controller);
            return;
        default:
            fprintf(stderr, "[SDL] EVENT type=%d\n", e->type);
            return;
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_CTYPE, "");

    load_device(&device);
    load_config(&config);

    int term_width = 0;
    int term_height = 0;
    int font_size = 0;

    const char *font_path = NULL;
    const char *bg_path = NULL;

    char **child_argv = NULL;

    int child_argc = 1;
    int cmd_index = 0;

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)) print_help(argv[0]);

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--") == 0) {
            cmd_index = i + 1;
            break;
        } else if ((strcmp(a, "-w") == 0 || strcmp(a, "--width") == 0) && i + 1 < argc) {
            term_width = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(a, "-h") == 0 || strcmp(a, "--height") == 0) && i + 1 < argc) {
            term_height = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(a, "-s") == 0 || strcmp(a, "--size") == 0) && i + 1 < argc) {
            font_size = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(a, "-f") == 0 || strcmp(a, "--font") == 0) && i + 1 < argc) {
            font_path = str_trim(argv[++i]);
        } else if ((strcmp(a, "-i") == 0 || strcmp(a, "--image") == 0) && i + 1 < argc) {
            bg_path = str_trim(argv[++i]);
        } else if ((strcmp(a, "-sb") == 0 || strcmp(a, "--scrollback") == 0) && i + 1 < argc) {
            sb_capacity = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(a, "-bg") == 0 || strcmp(a, "--bgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_bg)) return 1;
            use_solid_bg = 1;
        } else if ((strcmp(a, "-fg") == 0 || strcmp(a, "--fgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_fg)) return 1;
            use_solid_fg = 1;
        } else if (strcmp(a, "-ro") == 0 || strcmp(a, "--readonly") == 0) {
            readonly_mode = 1;
        } else if (a[0] != '-') {
            cmd_index = i;
            break;
        }
    }

    if (cmd_index > 0 && cmd_index < argc) {
        child_argv = &argv[cmd_index - 1];
        child_argc = argc - cmd_index + 1;
    } else {
        child_argv = argv;
        child_argc = 1;
    }

    if (term_width == 0) term_width = device.SCREEN.WIDTH;
    if (term_height == 0) term_height = device.SCREEN.HEIGHT;
    if (!font_size) font_size = (strcasecmp(device.BOARD.NAME, "tui-brick") == 0) ? 28 : 16;
    if (!font_path) font_path = OPT_PATH "share/font/muterm.ttf";
    if (sb_capacity < 1) sb_capacity = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) return 1;

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    fprintf(stderr, "[OSK] SDL subsystems initialised\n");

    int num_joy = SDL_NumJoysticks();
    fprintf(stderr, "[OSK] Joysticks detected: %d\n", num_joy);

    for (int i = 0; i < num_joy; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) fprintf(stderr, "[OSK] Opened GameController %d: %s\n", i, SDL_GameControllerName(gc));
        } else {
            SDL_Joystick *joy = SDL_JoystickOpen(i);
            if (joy) fprintf(stderr, "[OSK] Opened Joystick %d: %s\n", i, SDL_JoystickName(joy));
        }
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) return 1;
    if (TTF_Init() != 0) return 1;

    TTF_Font *base = TTF_OpenFont(font_path, font_size);
    if (!base) return 1;

    TTF_SizeUTF8(base, "M", &CELL_WIDTH, &CELL_HEIGHT);
    if (CELL_WIDTH <= 0) CELL_WIDTH = 8;
    if (CELL_HEIGHT <= 0) CELL_HEIGHT = 16;

    TERM_COLS = term_width / CELL_WIDTH;
    TERM_ROWS = term_height / CELL_HEIGHT;

    if (TERM_COLS < 1) TERM_COLS = 1;
    if (TERM_ROWS < 1) TERM_ROWS = 1;

    for (int i = 0; i < 4; i++) {
        fonts[i] = TTF_OpenFont(font_path, font_size);
        if (!fonts[i]) return 1;
        int st = TTF_STYLE_NORMAL;
        if (i & STYLE_BOLD) st |= TTF_STYLE_BOLD;
        if (i & STYLE_UNDERLINE) st |= TTF_STYLE_UNDERLINE;
        TTF_SetFontStyle(fonts[i], st);
    }
    TTF_CloseFont(base);

    main_screen_buf = calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    alt_screen_buf = calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    scrollback_init();

    if (!main_screen_buf || !alt_screen_buf || !scrollback) return 1;

    screen_buf = main_screen_buf;
    using_alt_screen = 0;

    current_fg = default_fg;
    current_bg = default_bg;
    current_style = 0;

    vt_reset_state(1);

    osk_calc_metrics(term_width);
    osk_init_layers();

    int pty_fd = -1;
    pid_t child = spawn_pty_child(&pty_fd, child_argc, child_argv);
    if (child < 0 || pty_fd < 0) return 1;

    pty_fd_global = pty_fd;
    int fl = fcntl(pty_fd, F_GETFL);
    if (fl >= 0) fcntl(pty_fd, F_SETFL, fl | O_NONBLOCK);

    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigchld_handler;
        sa.sa_flags = SA_NOCLDSTOP;
        sigaction(SIGCHLD, &sa, NULL);
    }

    SDL_Window *win = SDL_CreateWindow("Mustard Terminal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       term_width, term_height, SDL_WINDOW_SHOWN);
    if (!win) return 1;

    SDL_StartTextInput();

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) return 1;
    }

    SDL_Texture *render_target = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                   TERM_COLS * CELL_WIDTH, TERM_ROWS * CELL_HEIGHT);
    if (!render_target) return 1;

    SDL_Texture *bg_texture = NULL;
    if (bg_path && access(bg_path, R_OK) == 0) {
        SDL_Surface *bgs = IMG_Load(bg_path);
        if (bgs) {
            bg_texture = SDL_CreateTextureFromSurface(ren, bgs);
            SDL_FreeSurface(bgs);
        }
    }

    SDL_GameController *controller = NULL;
    reopen_controller(&controller);

    int running = 1;
    int shell_dead = 0;
    int vis_rows = TERM_ROWS;

    Uint32 last_wait = 0;
    Uint32 last_frame = 0;

    const Uint32 frame_ms = 33;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) handle_sdl_event(&e, &controller, &running, shell_dead, &vis_rows, term_width, term_height);

        if (osk_hold_active) {
            Uint32 now = SDL_GetTicks();
            const Uint32 initial_delay = KEY_REPEAT_DELAY;
            const Uint32 repeat_rate = KEY_REPEAT_RATE;

            if (now - osk_hold_press_t >= initial_delay) {
                if (now - osk_hold_last_rep >= repeat_rate) {

                    switch (osk_hold_action) {
                        case INPUT_ACT_PRESS:
                            osk_press_key();
                            break;
                        case INPUT_ACT_BACKSPACE:
                            if (!readonly_mode) {
                                char bs = '\x7F';
                                pty_write(&bs, 1);
                            }
                            break;
                        case INPUT_ACT_SPACE:
                            if (!readonly_mode) {
                                char sp = ' ';
                                pty_write(&sp, 1);
                            }
                            break;
                        default:
                            break;
                    }

                    osk_hold_last_rep = now;
                }
            }
        }

        if (!shell_dead) {
            struct pollfd pfd = {pty_fd, POLLIN, 0};
            poll(&pfd, 1, 0);

            if (pfd.revents & POLLIN) {
                char buf[4096];
                ssize_t n;
                int saw_output = 0;

                while ((n = read(pty_fd, buf, sizeof(buf))) > 0) {
                    if (vt_feed_pty_bytes(buf, (size_t) n)) {
                        saw_output = 1;
                    }
                }

                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    if (errno == EIO) shell_dead = 1;
                }

                if (saw_output) scroll_offset = 0;
            }
        }

        {
            Uint32 now = SDL_GetTicks();
            if (child_exited && !shell_dead) {
                int status;
                if (waitpid(child, &status, WNOHANG) > 0) shell_dead = 1;
            }

            if (!child_exited && now - last_wait >= 250) {
                last_wait = now;
                int status;
                if (waitpid(child, &status, WNOHANG) > 0) shell_dead = 1;
            }
        }

        if (shell_dead) running = 0;

        Uint32 now = SDL_GetTicks();
        if (now - last_frame < frame_ms) {
            SDL_Delay(1);
            continue;
        }

        last_frame = now;

        int need_blink = cursor_vis && !readonly_mode && !scroll_offset;
        if (screen_dirty || osk_visible || need_blink) {
            render_screen_to_target(ren, render_target, bg_texture, term_width, term_height, vis_rows);
            screen_dirty = 0;
        }

        float zoom = device.SCREEN.ZOOM;

        float sw = (float) (TERM_COLS * CELL_WIDTH) * zoom;
        float sh = (float) (TERM_ROWS * CELL_HEIGHT) * zoom;

        float underscan = (config.SETTINGS.HDMI.SCAN == 1) ? 16.0f : 0.0f;

        SDL_FRect dest = {
                ((float) term_width - sw) / 2.0f + underscan,
                ((float) term_height - sh) / 2.0f + underscan,
                sw - underscan * 2.0f, sh - underscan * 2.0f
        };

        double angle = 0;
        switch (device.SCREEN.ROTATE) {
            case 1:
                angle = 90;
                break;
            case 2:
                angle = 180;
                break;
            case 3:
                angle = 270;
                break;
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopyExF(ren, render_target, NULL, &dest, angle, NULL, SDL_FLIP_NONE);

        if (osk_visible) render_osk(ren, term_width, term_height);

        SDL_RenderPresent(ren);
    }

    SDL_StopTextInput();

    if (pty_fd >= 0) close(pty_fd);
    if (!shell_dead) waitpid(child, NULL, WNOHANG);

    SDL_DestroyTexture(render_target);
    if (bg_texture) SDL_DestroyTexture(bg_texture);

    glyph_cache_clear();

    for (int i = 0; i < 4; i++) {
        if (fonts[i]) TTF_CloseFont(fonts[i]);
    }

    if (controller) SDL_GameControllerClose(controller);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    if (main_screen_buf) free(main_screen_buf);
    if (alt_screen_buf) free(alt_screen_buf);
    if (scrollback) free(scrollback);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
