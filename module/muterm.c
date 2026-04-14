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

static int vt_acs_mode = 0;

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
        case 'q':
        case 'o':
        case 's':
            return 0x2500; /* ─ */
        case 'x':
            return 0x2502; /* │ */
        case 'l':
            return 0x250C; /* ┌ */
        case 'k':
            return 0x2510; /* ┐ */
        case 'm':
            return 0x2514; /* └ */
        case 'j':
            return 0x2518; /* ┘ */
        case 't':
            return 0x251C; /* ├ */
        case 'u':
            return 0x2524; /* ┤ */
        case 'w':
            return 0x252C; /* ┬ */
        case 'v':
            return 0x2534; /* ┴ */
        case 'n':
            return 0x253C; /* ┼ */
        default:
            return cp;
    }
}

static const Uint8 cube6[6] = {0, 95, 135, 175, 215, 255};

static inline Cell *CELL(int r, int c) {
    return &screen_buf[(size_t) r * (size_t) TERM_COLS + (size_t) c];
}

static inline int colour_equal(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

#define DEFAULT_SCROLLBACK 500

static Cell *scrollback = NULL;
static int sb_capacity = DEFAULT_SCROLLBACK;
static int sb_count = 0;
static int sb_head = 0;
static int scroll_offset = 0;

static void scrollback_init(void) {
    scrollback = calloc((size_t) sb_capacity * (size_t) TERM_COLS, sizeof(Cell));
    sb_count = 0;
    sb_head = 0;
    scroll_offset = 0;
}

static void scrollback_push(const Cell *row) {
    memcpy(&scrollback[(size_t) sb_head * (size_t) TERM_COLS], row,
           (size_t) TERM_COLS * sizeof(Cell));
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
    for (size_t i = 0; i < total; i++) reset_cell(&screen_buf[i]);
}

static void set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= TERM_ROWS) row = TERM_ROWS - 1;

    if (col < 0) col = 0;
    if (col >= TERM_COLS) col = TERM_COLS - 1;

    cursor_row = row;
    cursor_col = col;
}

static void scroll_up(void) {
    if (TERM_ROWS <= 1) return;

    scrollback_push(screen_buf);
    memmove(screen_buf, screen_buf + TERM_COLS, sizeof(Cell) * (size_t) TERM_COLS * (size_t) (TERM_ROWS - 1));

    for (int c = 0; c < TERM_COLS; c++) {
        reset_cell(CELL(TERM_ROWS - 1, c));
    }
}

static void scroll_down_one(void) {
    if (TERM_ROWS <= 1) return;

    if (cursor_row > 0) {
        cursor_row--;
        return;
    }

    memmove(screen_buf + TERM_COLS, screen_buf, sizeof(Cell) * (size_t) TERM_COLS * (size_t) (TERM_ROWS - 1));

    for (int c = 0; c < TERM_COLS; c++) {
        reset_cell(CELL(0, c));
    }
}

static void handle_esc(const char *seq) {
    if (!seq || !*seq) return;

    if (seq[0] == '(') {
        if (seq[1] == '0') {
            if (vt_acs_mode != 1) {
                vt_acs_mode = 1;
                fprintf(stderr, "[VT] ACS ON\n");
            }
        } else if (seq[1] == 'B') {
            if (vt_acs_mode != 0) {
                vt_acs_mode = 0;
                fprintf(stderr, "[VT] ACS OFF\n");
            }
        }
        return;
    }

    if (seq[0] == '7') {
        saved_row = cursor_row;
        saved_col = cursor_col;

        return;
    }

    if (seq[0] == '8') {
        set_cursor(saved_row, saved_col);

        return;
    }

    if (seq[0] == 'M') {
        scroll_down_one();

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

    if (ch == '\n') {
        cursor_col = 0;
        cursor_row++;
        return;
    }

    if (ch == '\r') {
        cursor_col = 0;
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

    if (vt_acs_mode) ch = vt_map_acs(ch);

    w = wcwidth_emu(ch);
    if (w <= 0) return;

    if (cursor_col >= TERM_COLS || cursor_col + w > TERM_COLS) {
        cursor_row++;
        cursor_col = 0;
    }

    if (cursor_row >= TERM_ROWS) {
        scroll_up();
        cursor_row = TERM_ROWS - 1;
    }

    Cell *c = CELL(cursor_row, cursor_col);

    c->codepoint = ch;
    c->width = (Uint8) w;
    c->fg = use_solid_fg ? solid_fg : current_fg;
    c->bg = current_bg;
    c->style = current_style;

    if (w == 2) {
        if (cursor_col + 1 < TERM_COLS) {
            Cell *c2 = CELL(cursor_row, cursor_col + 1);
            c2->codepoint = 0;
            c2->width = 0;
            c2->fg = c->fg;
            c2->bg = c->bg;
            c2->style = c->style;
        }
    }

    cursor_col += w;

    if (cursor_row >= TERM_ROWS) {
        scroll_up();
        cursor_row = TERM_ROWS - 1;
    }
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
                        current_fg = (SDL_Color) {
                                (Uint8) params[i + 2], (Uint8) params[i + 3],
                                (Uint8) params[i + 4], 255};
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
                for (int c = cursor_col; c < TERM_COLS; c++)
                    reset_cell(CELL(cursor_row, c));

                for (int r = cursor_row + 1; r < TERM_ROWS; r++)
                    for (int c = 0; c < TERM_COLS; c++)
                        reset_cell(CELL(r, c));

            } else if (mode == 1) {
                for (int r = 0; r < cursor_row; r++)
                    for (int c = 0; c < TERM_COLS; c++)
                        reset_cell(CELL(r, c));

                for (int c = 0; c <= cursor_col; c++)
                    reset_cell(CELL(cursor_row, c));

            } else if (mode == 2) {
                for (int r = 0; r < TERM_ROWS; r++)
                    for (int c = 0; c < TERM_COLS; c++)
                        reset_cell(CELL(r, c));

                set_cursor(0, 0);
            }
            break;
        }
        case 'K': {
            int mode = p0;
            int start = 0;
            int end = TERM_COLS;

            if (mode == 0) start = cursor_col;
            else if (mode == 1) end = cursor_col + 1;

            for (int c = start; c < end; c++) {
                reset_cell(CELL(cursor_row, c));
            }
            break;
        }
        case 'S': {
            int n = p0 ? p0 : 1;
            for (int i = 0; i < n; i++) scroll_up();
            break;
        }
        case 'T': {
            int n = p0 ? p0 : 1;
            for (int i = 0; i < n; i++) scroll_down_one();
            break;
        }
        case 'L': {
            int n = p0 ? p0 : 1;
            for (int i = 0; i < n; i++) {
                memmove(CELL(cursor_row + 1, 0), CELL(cursor_row, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (TERM_ROWS - cursor_row - 1));
                for (int c = 0; c < TERM_COLS; c++)
                    reset_cell(CELL(cursor_row, c));
            }
            break;
        }
        case 'M': {
            int n = p0 ? p0 : 1;
            for (int i = 0; i < n; i++) {
                if (cursor_row < TERM_ROWS - 1) {
                    memmove(CELL(cursor_row, 0), CELL(cursor_row + 1, 0), sizeof(Cell) * (size_t) TERM_COLS * (size_t) (TERM_ROWS - cursor_row - 1));
                }

                for (int c = 0; c < TERM_COLS; c++)
                    reset_cell(CELL(TERM_ROWS - 1, c));
            }
            break;
        }
        case 'P': {
            int n = p0 ? p0 : 1;

            if (cursor_col + n > TERM_COLS) n = TERM_COLS - cursor_col;
            memmove(CELL(cursor_row, cursor_col), CELL(cursor_row, cursor_col + n), sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n));

            for (int c = TERM_COLS - n; c < TERM_COLS; c++)
                reset_cell(CELL(cursor_row, c));
            break;
        }
        case '@': {
            int n = p0 ? p0 : 1;
            if (cursor_col + n > TERM_COLS) n = TERM_COLS - cursor_col;
            memmove(CELL(cursor_row, cursor_col + n), CELL(cursor_row, cursor_col), sizeof(Cell) * (size_t) (TERM_COLS - cursor_col - n));

            for (int c = cursor_col; c < cursor_col + n; c++)
                reset_cell(CELL(cursor_row, c));
            break;
        }
        case 'X': {
            int n = p0 ? p0 : 1;
            for (int c = cursor_col;
                 c < cursor_col + n && c < TERM_COLS;
                 c++)
                reset_cell(CELL(cursor_row, c));
            break;
        }
        case 'm':
            apply_sgr(params, count);
            break;
        case 's':
            saved_row = cursor_row;
            saved_col = cursor_col;
            break;
        case 'u':
            set_cursor(saved_row, saved_col);
            break;
        case 'h':
            if (is_private && p0 == 25) cursor_vis = 1;
            break;
        case 'l':
            if (is_private && p0 == 25) cursor_vis = 0;
            break;
        default:
            break;
    }
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
#define GLYPH_MAX_ENTRIES 4096u

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

static int is_vt_soft_char(Uint32 cp) {
    return is_vt_box_char(cp) || is_vt_block_char(cp) || is_vt_geom_char(cp) || cp == 0x00B7;
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

#define KEY_REPEAT_DELAY 500
#define KEY_REPEAT_RATE  80

static int osk_a_held = 0;
static Uint32 osk_a_press_t = 0;
static Uint32 osk_a_last_rep = 0;

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
    int slen = (int) strlen(seq);

    if (slen <= 0) return;

    if (osk_alt) pty_write("\x1B", 1);

    if (osk_ctrl && slen == 1) {
        char ch = seq[0];

        if (ch >= 'a' && ch <= 'z') {
            ch = (char) (ch - 'a' + 1);
        } else if (ch >= 'A' && ch <= 'Z') {
            ch = (char) (ch - 'A' + 1);
        }

        pty_write(&ch, 1);
    } else {
        pty_write(seq, slen);
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

                SDL_Rect cr = {
                        cursor_col * CELL_WIDTH,
                        cursor_row * CELL_HEIGHT,
                        CELL_WIDTH * w,
                        CELL_HEIGHT
                };

                SDL_SetRenderDrawColor(ren,
                                       default_fg.r,
                                       default_fg.g,
                                       default_fg.b,
                                       160);

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

    if (sym == SDLK_UP) {
        pty_write("\x1B[A", 3);
        return;
    }

    if (sym == SDLK_DOWN) {
        pty_write("\x1B[B", 3);
        return;
    }

    if (sym == SDLK_RIGHT) {
        pty_write("\x1B[C", 3);
        return;
    }

    if (sym == SDLK_LEFT) {
        pty_write("\x1B[D", 3);
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
            char c = '\x7F';
            pty_write(&c, 1);
            return;
        }
        case SDLK_TAB: {
            char c = '\t';
            pty_write(&c, 1);
            return;
        }
        case SDLK_ESCAPE: {
            char c = '\x1B';
            pty_write(&c, 1);
            return;
        }
        default:
            break;
    }

    (void) shift;
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
            execl("/bin/sh", "sh", "-l", "-c", argv[1], (char *) NULL);
        } else {
            execl("/bin/sh", "sh", "-l", (char *) NULL);
        }

        perror("exec");
        _exit(127);
    }

    close(slave_fd);
    *master_fd_out = master_fd;
    return pid;
}

int main(int argc, char *argv[]) {
    setlocale(LC_CTYPE, "");

    load_device(&device);
    load_config(&config);

    int term_width = 0, term_height = 0, font_size = 0;
    const char *font_path = NULL, *bg_path = NULL;
    char **cmd = NULL;

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0))
        print_help(argv[0]);

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((strcmp(a, "-w") == 0 || strcmp(a, "--width") == 0) && i + 1 < argc) term_width = safe_atoi(str_trim(argv[++i]));
        else if ((strcmp(a, "-h") == 0 || strcmp(a, "--height") == 0) && i + 1 < argc) term_height = safe_atoi(str_trim(argv[++i]));
        else if ((strcmp(a, "-s") == 0 || strcmp(a, "--size") == 0) && i + 1 < argc) font_size = safe_atoi(str_trim(argv[++i]));
        else if ((strcmp(a, "-f") == 0 || strcmp(a, "--font") == 0) && i + 1 < argc) font_path = str_trim(argv[++i]);
        else if ((strcmp(a, "-i") == 0 || strcmp(a, "--image") == 0) && i + 1 < argc) bg_path = str_trim(argv[++i]);
        else if ((strcmp(a, "-sb") == 0 || strcmp(a, "--scrollback") == 0) && i + 1 < argc) sb_capacity = safe_atoi(str_trim(argv[++i]));
        else if ((strcmp(a, "-bg") == 0 || strcmp(a, "--bgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_bg)) return 1;
            use_solid_bg = 1;
        } else if ((strcmp(a, "-fg") == 0 || strcmp(a, "--fgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_fg)) return 1;
            use_solid_fg = 1;
        } else if (strcmp(a, "-ro") == 0 || strcmp(a, "--readonly") == 0) {
            readonly_mode = 1;
        } else if (a[0] != '-') {
            cmd = &argv[i];
            break;
        }
    }

    if (term_width == 0) term_width = device.SCREEN.WIDTH;
    if (term_height == 0) term_height = device.SCREEN.HEIGHT;
    if (!font_size) font_size = (strcasecmp(device.BOARD.NAME, "tui-brick") == 0) ? 28 : 16;
    if (!font_path) font_path = OPT_PATH "share/font/muterm.ttf";
    if (sb_capacity < 1) sb_capacity = 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

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

    /* fonts */
    TTF_Font *base = TTF_OpenFont(font_path, font_size);
    if (!base) return 1;

    // Determine cell size from typical glyph
    TTF_SizeUTF8(base, "M", &CELL_WIDTH, &CELL_HEIGHT);
    if (CELL_WIDTH <= 0) CELL_WIDTH = 8;
    if (CELL_HEIGHT <= 0) CELL_HEIGHT = 16;

    TERM_COLS = term_width / CELL_WIDTH;
    TERM_ROWS = term_height / CELL_HEIGHT;

    if (TERM_COLS < 1) TERM_COLS = 1;
    if (TERM_ROWS < 1) TERM_ROWS = 1;

    // Create different styled font handles
    for (int i = 0; i < 4; i++) {
        fonts[i] = TTF_OpenFont(font_path, font_size);
        if (!fonts[i]) return 1;
        int st = TTF_STYLE_NORMAL;
        if (i & STYLE_BOLD) st |= TTF_STYLE_BOLD;
        if (i & STYLE_UNDERLINE) st |= TTF_STYLE_UNDERLINE;
        TTF_SetFontStyle(fonts[i], st);
    }
    TTF_CloseFont(base);

    screen_buf = (Cell *) calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    if (!screen_buf) return 1;

    scrollback_init();

    current_fg = default_fg;
    current_bg = default_bg;
    current_style = 0;
    clear_screen();
    screen_dirty = 1;

    osk_calc_metrics(term_width);
    osk_init_layers();

    int pty_fd = -1;
    pid_t child = spawn_pty_child(&pty_fd, argc, cmd ? cmd : argv);
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

    SDL_Window *win = SDL_CreateWindow("muterm",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       term_width, term_height, SDL_WINDOW_SHOWN);
    if (!win) return 1;

    SDL_StartTextInput(); /* enable text input events for USB keyboard */

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
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }

    int running = 1;
    int shell_dead = 0;
    int vis_rows = TERM_ROWS;

    Uint32 last_wait = 0;
    Uint32 last_frame = 0;

    const Uint32 frame_ms = 33;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_TEXTINPUT:
                    if (!readonly_mode && !osk_visible) {
                        pty_write(e.text.text, (int) strlen(e.text.text));
                        scroll_offset = 0;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE &&
                        !(SDL_GetModState() & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))) {
                        /* only quit if shell is dead; otherwise pass ESC to PTY */
                        if (shell_dead) {
                            running = 0;
                            break;
                        }
                    }

                    if (e.key.keysym.sym == SDLK_PAGEUP) {
                        scroll_offset += vis_rows / 2;
                        if (scroll_offset > sb_count) scroll_offset = sb_count;
                        screen_dirty = 1;
                        break;
                    }

                    if (e.key.keysym.sym == SDLK_PAGEDOWN) {
                        scroll_offset -= vis_rows / 2;
                        if (scroll_offset < 0) scroll_offset = 0;
                        screen_dirty = 1;
                        break;
                    }

                    handle_keyboard(&e.key);
                    screen_dirty = 1;
                    break;

                case SDL_JOYAXISMOTION: {
                    int axis = e.jaxis.axis;
                    int val = e.jaxis.value;
                    const int DEAD = 16000;

                    if (!osk_visible) break;

                    if (axis == 0) {
                        if (val < -DEAD && axis_x_state != -1) {
                            axis_x_state = -1;
                            fprintf(stderr, "[OSK] LEFT\n");
                            osk_move(0, -1);
                            screen_dirty = 1;
                        } else if (val > DEAD && axis_x_state != 1) {
                            axis_x_state = 1;
                            fprintf(stderr, "[OSK] RIGHT\n");
                            osk_move(0, 1);
                            screen_dirty = 1;
                        } else if (val >= -DEAD && val <= DEAD) {
                            axis_x_state = 0;
                        }
                    } else if (axis == 1) {
                        if (val < -DEAD && axis_y_state != -1) {
                            axis_y_state = -1;
                            fprintf(stderr, "[OSK] UP\n");
                            osk_move(-1, 0);
                            screen_dirty = 1;
                        } else if (val > DEAD && axis_y_state != 1) {
                            axis_y_state = 1;
                            fprintf(stderr, "[OSK] DOWN\n");
                            osk_move(1, 0);
                            screen_dirty = 1;
                        } else if (val >= -DEAD && val <= DEAD) {
                            axis_y_state = 0;
                        }
                    }

                    break;
                }

                case SDL_JOYHATMOTION: {
                    if (!osk_visible) break;

                    Uint8 hat = e.jhat.value;
                    fprintf(stderr, "[OSK] HAT=%d\n", hat);

                    if (hat & SDL_HAT_UP) {
                        osk_move(-1, 0);
                        fprintf(stderr, "[OSK] DPAD UP\n");
                    }
                    if (hat & SDL_HAT_DOWN) {
                        osk_move(1, 0);
                        fprintf(stderr, "[OSK] DPAD DOWN\n");
                    }
                    if (hat & SDL_HAT_LEFT) {
                        osk_move(0, -1);
                        fprintf(stderr, "[OSK] DPAD LEFT\n");
                    }
                    if (hat & SDL_HAT_RIGHT) {
                        osk_move(0, 1);
                        fprintf(stderr, "[OSK] DPAD RIGHT\n");
                    }

                    screen_dirty = 1;
                    break;
                }

                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_JOYBUTTONDOWN: {
                    int raw = -1;

                    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                        raw = e.cbutton.button;
                    } else {
                        raw = e.jbutton.button;
                    }

                    fprintf(stderr, "[OSK] DOWN RAW=%d\n", raw);

                    switch (raw) {
                        case 9: {
                            osk_state = (osk_state + 1) % OSK_NUM_STATES;
                            osk_visible = (osk_state != OSK_STATE_HIDDEN);

                            fprintf(stderr, "[OSK] STATE -> %d visible=%d\n", osk_state, osk_visible);

                            if (osk_state == OSK_STATE_BOTTOM_OPAQUE) {
                                vis_rows = (term_height - osk_height) / CELL_HEIGHT;
                            } else {
                                vis_rows = TERM_ROWS;
                            }

                            if (vis_rows < 1) vis_rows = 1;

                            scroll_offset = 0;
                            screen_dirty = 1;
                            break;
                        }
                        case 11:
                            fprintf(stderr, "[OSK] QUIT\n");
                            running = 0;
                            break;
                        case 3:
                            if (osk_visible) {
                                fprintf(stderr, "[OSK] PRESS KEY\n");
                                osk_press_key();

                                if (!osk_a_held) {
                                    osk_a_held = 1;
                                    osk_a_press_t = SDL_GetTicks();
                                    osk_a_last_rep = osk_a_press_t;
                                }

                                scroll_offset = 0;
                                screen_dirty = 1;
                            }
                            break;
                        case 4:
                            fprintf(stderr, "[OSK] BACKSPACE\n");
                            if (!readonly_mode) {
                                char bs = '\x7F';
                                pty_write(&bs, 1);
                            }
                            break;
                        case 5:
                            fprintf(stderr, "[OSK] SPACE\n");
                            if (!readonly_mode) {
                                pty_write(" ", 1);
                            }
                            break;
                        case 6:
                            break;
                        case 7:
                            if (osk_visible) {
                                fprintf(stderr, "[OSK] LAYER -1\n");
                                osk_switch_layer(-1);
                                screen_dirty = 1;
                            }
                            break;
                        case 8:
                            if (osk_visible) {
                                fprintf(stderr, "[OSK] LAYER +1\n");
                                osk_switch_layer(1);
                                screen_dirty = 1;
                            }
                            break;
                        case 10:
                            fprintf(stderr, "[OSK] ENTER\n");
                            if (!readonly_mode) {
                                pty_write("\r", 1);
                            }
                            break;
                        case 2:
                            fprintf(stderr, "[OSK] PAGE UP\n");
                            scroll_offset += vis_rows / 2;
                            if (scroll_offset > sb_count) scroll_offset = sb_count;
                            screen_dirty = 1;
                            break;
                        case 1:
                            fprintf(stderr, "[OSK] PAGE DOWN\n");
                            scroll_offset -= vis_rows / 2;
                            if (scroll_offset < 0) scroll_offset = 0;
                            screen_dirty = 1;
                            break;
                        default:
                            fprintf(stderr, "[OSK] UNHANDLED RAW=%d\n", raw);
                            break;
                    }

                    break;
                }
                case SDL_CONTROLLERBUTTONUP:
                case SDL_JOYBUTTONUP: {
                    int raw = -1;

                    if (e.type == SDL_CONTROLLERBUTTONUP) {
                        raw = e.cbutton.button;
                    } else {
                        raw = e.jbutton.button;
                    }

                    fprintf(stderr, "[OSK] UP RAW=%d\n", raw);
                    if (raw == 3) {
                        fprintf(stderr, "[OSK] A RELEASE\n");
                        osk_a_held = 0;
                    }

                    break;
                }
                case SDL_CONTROLLERDEVICEADDED:
                    if (!controller) controller = SDL_GameControllerOpen(e.cdevice.which);
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (controller) {
                        SDL_GameControllerClose(controller);
                        controller = NULL;
                        for (int i = 0; i < SDL_NumJoysticks(); i++) {
                            if (SDL_IsGameController(i)) {
                                controller = SDL_GameControllerOpen(i);
                                if (controller) break;
                            }
                        }
                    }
                    break;
                default:
                    fprintf(stderr, "[SDL] EVENT type=%d\n", e.type);
                    break;
            }
        }

        if (osk_visible && osk_a_held) {
            Uint32 now = SDL_GetTicks();
            if (now - osk_a_press_t >= KEY_REPEAT_DELAY && now - osk_a_last_rep >= KEY_REPEAT_RATE) {
                osk_press_key();
                osk_a_last_rep = now;
                screen_dirty = 1;
            }
        }

        if (!shell_dead) {
            struct pollfd pfd = {pty_fd, POLLIN, 0};
            poll(&pfd, 1, 0);

            if (pfd.revents & POLLIN) {
                char buf[4096];
                ssize_t n;
                while ((n = read(pty_fd, buf, sizeof(buf))) > 0) {
                    const char *p = buf, *end = buf + n;
                    while (p < end) {
                        if ((unsigned char) *p == 0x1B) {
                            p++;
                            if (p >= end) break;

                            if (*p == '[') {
                                const char *start = p;
                                p++;

                                while (p < end && !is_csi_final((unsigned char) *p)) p++;
                                if (p < end) p++;

                                size_t len = (size_t) (p - start);
                                char tmp[64];

                                if (len < sizeof(tmp)) {
                                    memcpy(tmp, start, len);
                                    tmp[len] = '\0';
                                    parse_csi(tmp);
                                }

                                continue;
                            }

                            {
                                char tmp[8] = {0};
                                size_t i = 0;

                                while (p < end && i < sizeof(tmp) - 1) {
                                    tmp[i++] = *p;

                                    if (i == 2) {
                                        p++;
                                        break;
                                    }

                                    p++;
                                }

                                tmp[i] = '\0';
                                handle_esc(tmp);
                            }

                            continue;
                        }

                        {
                            Uint32 cp = 0;
                            size_t consumed = utf8_decode_char(&cp, (const unsigned char *) p, (size_t) (end - p));

                            if (consumed == (size_t) -2) {
                                p = end;
                                break;
                            }

                            if (consumed == (size_t) -1) {
                                fprintf(stderr, "[UTF8] invalid byte 0x%02X\n", (unsigned char) *p);
                                put_char(0xFFFD);
                                screen_dirty = 1;
                                p++;

                                continue;
                            }

                            if (vt_acs_mode) {
                                if (cp >= 0x20 && cp <= 0x7E) {
                                    cp = vt_map_acs(cp);
                                }
                            }

                            put_char(cp);
                            screen_dirty = 1;

                            p += (ptrdiff_t) consumed;
                            continue;
                        }
                    }
                }

                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) if (errno == EIO) shell_dead = 1;
                scroll_offset = 0;
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

    free(screen_buf);
    free(scrollback);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
