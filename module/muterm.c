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
#include <uchar.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"

typedef struct {
    Uint32 codepoint;
    SDL_Color fg;
    Uint8 style;
} Cell;

enum {
    STYLE_BOLD = 1 << 0,
    STYLE_UNDERLINE = 1 << 1
};

static Cell *screen_buf = NULL;

static int TERM_COLS = 0;
static int TERM_ROWS = 0;
static int CELL_WIDTH = 0;
static int CELL_HEIGHT = 0;

static int cursor_row = 0;
static int cursor_col = 0;

static Uint8 current_style = 0;

static SDL_Color current_fg = {255, 255, 255, 255};
static SDL_Color solid_bg = {0, 0, 0, 255};
static SDL_Color solid_fg = {0, 0, 0, 0};

static int use_solid_bg = 0;
static int use_solid_fg = 0;

static SDL_Color base_colours[8] = {
        {0,   0,   0,   255},
        {170, 0,   0,   255},
        {0,   170, 0,   255},
        {170, 85,  0,   255},
        {0,   0,   170, 255},
        {170, 0,   170, 255},
        {0,   170, 170, 255},
        {170, 170, 170, 255}
};

static SDL_Color bright_colours[8] = {
        {85,  85,  85,  255},
        {255, 85,  85,  255},
        {85,  255, 85,  255},
        {255, 255, 85,  255},
        {85,  85,  255, 255},
        {255, 85,  255, 255},
        {85,  255, 255, 255},
        {255, 255, 255, 255}
};

static inline Cell *CELL(int r, int c) {
    return &screen_buf[(size_t) r * (size_t) TERM_COLS + (size_t) c];
}

static inline int colour_equal(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void print_help(const char *name) {
    printf("MustardOS Virtual Terminal\n");

    printf("\nUsage:\n");
    printf("\t%s [options] <command>\n", name);

    printf("\nOptions:\n");
    printf("\t-w, --width <pixels>\tOverride screen width\n");
    printf("\t\t\t\tDefault: device screen width\n");

    printf("\t-h, --height <pixels>\tOverride screen height\n");
    printf("\t\t\t\tDefault: device screen height\n");

    printf("\t-s, --size <points>\tSet font size\n");
    printf("\t\t\t\tDefault: 16 (or 28 on tui-brick)\n");

    printf("\t-f, --font <path.ttf>\tPath to TTF font file\n");
    printf("\t\t\t\tDefault: " OPT_PATH "/share/font/muterm.ttf\n");

    printf("\t-i, --image <image>\tPath to PNG background image\n");
    printf("\t\t\t\tDefault: none\n");

    printf("\t-bg, --bgcolour RRGGBB\tSolid background colour\n");
    printf("\t\t\t\tDefault: 000000\n");

    printf("\t-fg, --fgcolour RRGGBB\tSolid foreground colour (overrides ANSI)\n");
    printf("\t\t\t\tDefault: none\n");

    printf("\nArguments:\n");
    printf("\t<command>\t\tShell command or script to run\n");

    printf("\nExample:\n");
    printf("\t%s -s 24 -f ./font.ttf ./script.sh\n\n", name);

    exit(0);
}

static int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (!hex || strlen(hex) != 6) return 0;

    char r_str[3] = {hex[0], hex[1], '\0'};
    char g_str[3] = {hex[2], hex[3], '\0'};
    char b_str[3] = {hex[4], hex[5], '\0'};

    char *end;
    errno = 0;
    unsigned long r = strtoul(r_str, &end, 16);
    if (errno || *end != '\0' || r > 255) return 0;

    errno = 0;
    unsigned long g = strtoul(g_str, &end, 16);
    if (errno || *end != '\0' || g > 255) return 0;

    errno = 0;
    unsigned long b = strtoul(b_str, &end, 16);
    if (errno || *end != '\0' || b > 255) return 0;

    out->r = (Uint8) r;
    out->g = (Uint8) g;
    out->b = (Uint8) b;
    out->a = 255;

    return 1;
}

static inline void reset_cell(Cell *c) {
    c->codepoint = (Uint32) ' ';
    c->fg = current_fg;
    c->style = current_style;
}

static void clear_screen(void) {
    size_t total = (size_t) TERM_ROWS * (size_t) TERM_COLS;
    for (size_t i = 0; i < total; i++) {
        reset_cell(&screen_buf[i]);
    }
}

static void set_cursor_position(int row, int col) {
    if (row >= 1 && row <= TERM_ROWS) cursor_row = row - 1;
    if (col >= 1 && col <= TERM_COLS) cursor_col = col - 1;
}

static void scroll_up(void) {
    if (TERM_ROWS <= 1) return;

    // Move all rows up by one
    memmove(screen_buf, screen_buf + TERM_COLS,
            sizeof(Cell) * (size_t) TERM_COLS * (size_t) (TERM_ROWS - 1));

    // Clear last row
    for (int c = 0; c < TERM_COLS; c++) {
        reset_cell(CELL(TERM_ROWS - 1, c));
    }
}

static void put_char(Uint32 ch) {
    if (ch == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (ch == '\r') {
        cursor_col = 0;
    } else if (ch == '\b') {
        if (cursor_col > 0) cursor_col--;
    } else if (ch == '\t') {
        // Basic tab stops every 8 columns...?
        int next = ((cursor_col / 8) + 1) * 8;
        if (next >= TERM_COLS) {
            cursor_row++;
            cursor_col = 0;
        } else {
            cursor_col = next;
        }
    } else {
        if (cursor_col >= TERM_COLS) {
            cursor_row++;
            cursor_col = 0;
        }

        if (cursor_row >= TERM_ROWS) {
            scroll_up();
            cursor_row = TERM_ROWS - 1;
        }

        Cell *cell = CELL(cursor_row, cursor_col);
        cell->codepoint = ch;
        cell->fg = use_solid_fg ? solid_fg : current_fg;
        cell->style = current_style;
        cursor_col++;
    }

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

static inline int is_csi_final(unsigned char ch) {
    // CSI final byte is in @..~ (0x40...0x7E) :)
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

static void parse_csi(const char *seq) {
    // Sequence includes everything after ESC, e.g. "[31m" or "[2J"

    if (use_solid_fg) return;
    if (seq[0] != '[') return;

    int params[16];
    int count = 0;
    for (int i = 0; i < 16; i++) params[i] = 0;

    const char *p = seq + 1; // after '['

    while (*p && !is_csi_final((unsigned char) *p)) {
        if (*p >= '0' && *p <= '9') {
            if (count < 16) params[count] = parse_int_fast(&p);
            else while (*p >= '0' && *p <= '9') p++;
        } else if (*p == ';') {
            if (count < 15) count++;
            p++;
        } else {
            p++;
        }
    }

    char cmd = *p ? *p : '\0';
    switch (cmd) {
        case 'H': {
            int row = params[0] ? params[0] : 1;
            int col = params[1] ? params[1] : 1;
            set_cursor_position(row, col);
        }
            break;

        case 'J': {
            if (params[0] == 2) {
                clear_screen();
                set_cursor_position(1, 1);
            }
        }
            break;

        case 'K': {
            if (cursor_row >= TERM_ROWS) break;
            for (int c = cursor_col; c < TERM_COLS; c++) reset_cell(CELL(cursor_row, c));
        }
            break;

        case 'm': {
            // If no params (e.g. ESC[m), treat as reset
            int num_params = (count == 0 && params[0] == 0) ? 1 : (count + 1);

            for (int i = 0; i < num_params; i++) {
                int code = params[i];
                switch (code) {
                    case 0:
                        current_fg = base_colours[7];
                        current_style = 0;
                        break;
                    case 1:
                        current_style |= STYLE_BOLD;
                        break;
                    case 4:
                        current_style |= STYLE_UNDERLINE;
                        break;
                    case 22:
                        current_style &= (Uint8) ~STYLE_BOLD;
                        break;
                    case 24:
                        current_style &= (Uint8) ~STYLE_UNDERLINE;
                        break;
                    case 39:
                        current_fg = base_colours[7];
                        break;
                    default:
                        if (code >= 30 && code <= 37) {
                            SDL_Color base = base_colours[code - 30];
                            SDL_Color bright = bright_colours[code - 30];
                            current_fg = (current_style & STYLE_BOLD) ? bright : base;
                        } else if (code >= 90 && code <= 97) {
                            current_fg = bright_colours[code - 90];
                        }
                        break;
                }
            }
        }
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

#define GLYPH_BUCKETS 1024u
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

static GlyphEntry *glyph_cache_get(SDL_Renderer *ren, Uint32 cp, SDL_Color fg, Uint8 style) {
    Uint32 h = glyph_hash(cp, fg, style);
    unsigned b = (unsigned) (h % GLYPH_BUCKETS);

    for (GlyphEntry *e = glyph_table[b]; e; e = e->next) {
        if (e->codepoint == cp && e->style == (style & 3) && colour_equal(e->fg, fg)) {
            return e;
        }
    }

    if (glyph_entries >= GLYPH_MAX_ENTRIES) {
        glyph_cache_clear();
        b = (unsigned) (h % GLYPH_BUCKETS);
    }

    GlyphEntry *ne = (GlyphEntry *) calloc(1, sizeof(*ne));
    if (!ne) return NULL;

    ne->codepoint = cp;
    ne->fg = fg;
    ne->style = (Uint8) (style & 3);

    char utf8[8];
    (void) utf8_encode(utf8, cp);

    TTF_Font *font = fonts[ne->style];
    if (!font) {
        free(ne);
        return NULL;
    }

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, utf8, fg);
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

static int screen_dirty = 1;

static void render_screen_to_target(SDL_Renderer *ren, SDL_Texture *render_target, SDL_Texture *bg_texture) {
    SDL_SetRenderTarget(ren, render_target);

    if (bg_texture) {
        SDL_RenderCopy(ren, bg_texture, NULL, NULL);
    } else if (use_solid_bg) {
        SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
        SDL_RenderClear(ren);
    } else {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
    }

    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            Cell *cell = CELL(r, c);
            if (cell->codepoint == (Uint32) ' ') continue;

            SDL_Color draw_fg = use_solid_fg ? solid_fg : cell->fg;
            Uint8 style = (Uint8) (cell->style & 3);

            GlyphEntry *g = glyph_cache_get(ren, cell->codepoint, draw_fg, style);
            if (!g) continue;

            SDL_Rect dst = {c * CELL_WIDTH, r * CELL_HEIGHT, g->w, g->h};
            SDL_RenderCopy(ren, g->tex, NULL, &dst);
        }
    }

    SDL_SetRenderTarget(ren, NULL);
}

int main(int argc, char *argv[]) {
    // Required for mbrtoc32 decoding - wonder if it is worth setting elsewhere?
    setlocale(LC_CTYPE, "");

    load_device(&device);
    load_config(&config);

    int term_width = 0;
    int term_height = 0;
    int font_size = 0;

    const char *font_path = NULL;
    const char *bg_path = NULL;
    const char **cmd = NULL;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) print_help(argv[0]);

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if ((strcmp(arg, "-w") == 0 || strcmp(arg, "--width") == 0) && i + 1 < argc) {
            term_width = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(arg, "-h") == 0 || strcmp(arg, "--height") == 0) && i + 1 < argc) {
            term_height = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(arg, "-s") == 0 || strcmp(arg, "--size") == 0) && i + 1 < argc) {
            font_size = safe_atoi(str_trim(argv[++i]));
        } else if ((strcmp(arg, "-f") == 0 || strcmp(arg, "--font") == 0) && i + 1 < argc) {
            font_path = str_trim(argv[++i]);
        } else if ((strcmp(arg, "-i") == 0 || strcmp(arg, "--image") == 0) && i + 1 < argc) {
            bg_path = str_trim(argv[++i]);
        } else if ((strcmp(arg, "-bg") == 0 || strcmp(arg, "--bgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_bg)) return 1;
            use_solid_bg = 1;
        } else if ((strcmp(arg, "-fg") == 0 || strcmp(arg, "--fgcolour") == 0) && i + 1 < argc) {
            if (!parse_hex_colour(str_trim(argv[++i]), &solid_fg)) return 1;
            use_solid_fg = 1;
        } else if (arg[0] != '-') {
            cmd = (const char **) &argv[i];
            break;
        }
    }

    if (term_width == 0) term_width = device.SCREEN.WIDTH;
    if (term_height == 0) term_height = device.SCREEN.HEIGHT;

    if (!font_size) {
        if (strcasecmp(device.BOARD.NAME, "tui-brick") == 0) font_size = 28;
        else font_size = 16;
    }

    if (!font_path) font_path = OPT_PATH "share/font/muterm.ttf";

    if (!cmd) print_help(argv[0]);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (IMG_Init(IMG_INIT_PNG) == 0) return 1;
    if (TTF_Init() != 0) return 1;

    // Open base font then clone for each style
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
        int style = TTF_STYLE_NORMAL;
        if (i & STYLE_BOLD) style |= TTF_STYLE_BOLD;
        if (i & STYLE_UNDERLINE) style |= TTF_STYLE_UNDERLINE;
        TTF_SetFontStyle(fonts[i], style);
    }
    TTF_CloseFont(base);

    screen_buf = (Cell *) calloc((size_t) TERM_ROWS * (size_t) TERM_COLS, sizeof(Cell));
    if (!screen_buf) return 1;

    SDL_Window *win = SDL_CreateWindow("MustardOS Virtual Terminal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       term_width, term_height, SDL_WINDOW_SHOWN);
    if (!win) return 1;

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
        SDL_Surface *bg_surface = IMG_Load(bg_path);
        if (bg_surface) {
            bg_texture = SDL_CreateTextureFromSurface(ren, bg_surface);
            SDL_FreeSurface(bg_surface);
        }
    }

    current_fg = base_colours[7];
    current_style = 0;
    clear_screen();
    screen_dirty = 1;

    int pty_fd = -1;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));

    ws.ws_col = (unsigned short) TERM_COLS;
    ws.ws_row = (unsigned short) TERM_ROWS;

    ws.ws_xpixel = (unsigned short) term_width;
    ws.ws_ypixel = (unsigned short) term_height;

    pid_t child = forkpty(&pty_fd, NULL, NULL, &ws);
    if (child == 0) {
        if (cmd) {
            size_t cmd_count = 0;
            while (cmd[cmd_count]) cmd_count++;
            run_exec(cmd, cmd_count + 1, 0, 1, NULL, NULL);
        }
        _exit(127);
    }

    if (pty_fd < 0) return 1;

    int fl = fcntl(pty_fd, F_GETFL);
    if (fl >= 0) fcntl(pty_fd, F_SETFL, fl | O_NONBLOCK);

    enum {
        ESC_NONE = 0,
        ESC_GOT_ESC = 1,
        ESC_IN_CSI = 2
    };

    int esc_state = ESC_NONE;
    char esc_buf[128];
    size_t esc_len = 0;

    mbstate_t mbst;
    memset(&mbst, 0, sizeof(mbst));

    int running = 1;
    SDL_Event e;

    Uint32 last_wait_check = 0;
    Uint32 last_frame = 0;
    const Uint32 frame_ms = 33; // ~30fps - Don't need more

    while (running) {
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) running = 0;

        // Poll PTY with 0 timeout (we control frame cap below)
        struct pollfd pfd;
        pfd.fd = pty_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        (void) poll(&pfd, 1, 0);

        if (pfd.revents & POLLIN) {
            char buf[1024];
            ssize_t n;
            while ((n = read(pty_fd, buf, sizeof(buf))) > 0) {
                const char *p = buf;
                const char *end = buf + n;

                while (p < end) {
                    unsigned char ch = (unsigned char) *p;

                    switch (esc_state) {
                        case ESC_NONE:
                            if (ch == 0x1B) {
                                memset(&mbst, 0, sizeof(mbst)); // reset UTF-8 decoder
                                esc_state = ESC_GOT_ESC;
                                p++;
                                break;
                            }

                            if (ch == '\n' || ch == '\r' || ch == '\b' || ch == '\t') {
                                put_char((Uint32) ch);
                                screen_dirty = 1;
                                p++;
                                break;
                            }

                            char32_t cp = 0;
                            size_t consumed = mbrtoc32(&cp, p, (size_t) (end - p), &mbst);
                            if (consumed == (size_t) -2) {
                                p = end;
                                break;
                            } else if (consumed == (size_t) -1) {
                                memset(&mbst, 0, sizeof(mbst));
                                put_char((Uint32) 0xFFFD);
                                screen_dirty = 1;
                                p++;
                                break;
                            } else if (consumed == 0) {
                                p++;
                                break;
                            } else {
                                put_char((Uint32) cp);
                                screen_dirty = 1;
                                p += (ptrdiff_t) consumed;
                                break;
                            }
                        case ESC_GOT_ESC:
                            if (ch == '[') {
                                esc_state = ESC_IN_CSI;
                                esc_len = 0;
                                esc_buf[esc_len++] = '[';
                                esc_buf[esc_len] = '\0';
                            } else {
                                esc_state = ESC_NONE;
                            }

                            p++;
                            break;
                        case ESC_IN_CSI:
                            if (esc_len + 1 < sizeof(esc_buf)) {
                                esc_buf[esc_len++] = (char) ch;
                                esc_buf[esc_len] = '\0';
                            }

                            if (is_csi_final(ch)) {
                                parse_csi(esc_buf);
                                screen_dirty = 1;
                                esc_state = ESC_NONE;
                                esc_len = 0;
                            }

                            p++;
                            break;
                        default: // Fuck you
                            esc_state = ESC_NONE;
                            break;
                    }
                }
            }
        }

        // Periodically check child process exit
        Uint32 now = SDL_GetTicks();
        if (now - last_wait_check >= 250) {
            last_wait_check = now;
            int status;
            if (waitpid(child, &status, WNOHANG) > 0) running = 0;
        }

        // Frame cap if no vsync to keep CPU down
        if (now - last_frame < frame_ms) {
            SDL_Delay(1);
            continue;
        }
        last_frame = now;

        // Redraw cached target only when dirty
        if (screen_dirty) {
            render_screen_to_target(ren, render_target, bg_texture);
            screen_dirty = 0;
        }

        float zoom = device.SCREEN.ZOOM;
        float scale_width = (float) (TERM_COLS * CELL_WIDTH) * zoom;
        float scale_height = (float) (TERM_ROWS * CELL_HEIGHT) * zoom;
        float underscan = (config.SETTINGS.HDMI.SCAN == 1) ? 16.0f : 0.0f;

        SDL_FRect dest_rect = {
                ((float) term_width - scale_width) / 2.0f + underscan,
                ((float) term_height - scale_height) / 2.0f + underscan,
                scale_width - (underscan * 2.0f),
                scale_height - (underscan * 2.0f)
        };

        double angle;
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
            default:
                angle = 0;
                break;
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopyExF(ren, render_target, NULL, &dest_rect, angle, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(ren);
    }

    if (pty_fd >= 0) close(pty_fd);

    SDL_DestroyTexture(render_target);
    if (bg_texture) SDL_DestroyTexture(bg_texture);

    glyph_cache_clear();

    for (int i = 0; i < 4; i++) {
        if (fonts[i]) TTF_CloseFont(fonts[i]);
        fonts[i] = NULL;
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    free(screen_buf);
    screen_buf = NULL;

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
