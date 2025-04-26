#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/language.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

int msgbox_active = 0;
int nav_sound = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

typedef struct {
    Uint32 codepoint;
    SDL_Color fg;
    int bold;
    int underline;
} Cell;

Cell **screen = NULL;

int TERM_COLS, TERM_ROWS, CELL_WIDTH, CELL_HEIGHT;
int cursor_row = 0, cursor_col = 0;
int current_bold = 0, current_underline = 0;

SDL_Color current_fg = {255, 255, 255, 255};
SDL_Color solid_bg = {0, 0, 0, 255};
SDL_Color solid_fg = {0, 0, 0, 0};

int use_solid_bg = 0;
int use_solid_fg = 0;

SDL_Color base_colours[8] = {
        {0,   0,   0,   255},
        {170, 0,   0,   255},
        {0,   170, 0,   255},
        {170, 85,  0,   255},
        {0,   0,   170, 255},
        {170, 0,   170, 255},
        {0,   170, 170, 255},
        {170, 170, 170, 255}
};

SDL_Color bright_colours[8] = {
        {85,  85,  85,  255},
        {255, 85,  85,  255},
        {85,  255, 85,  255},
        {255, 255, 85,  255},
        {85,  85,  255, 255},
        {255, 85,  255, 255},
        {85,  255, 255, 255},
        {255, 255, 255, 255}
};

void print_help(const char *name) {
    printf("muOS Virtual Terminal\n");

    printf("\nUsage:\n");
    printf("\t%s [options] <command>\n", name);

    printf("\nOptions:\n");
    printf("\t-w, --width <pixels>\tOverride screen width\n");
    printf("\t\t\t\tDefault: device screen width\n");

    printf("\t-h, --height <pixels>\tOverride screen height\n");
    printf("\t\t\t\tDefault: device screen height\n");

    printf("\t-s, --size <points>\tSet font size\n");
    printf("\t\t\t\tDefault: 16\n");

    printf("\t-f, --font <path.ttf>\tPath to TTF font file\n");
    printf("\t\t\t\tDefault: /opt/muos/share/font/muterm.ttf\n");

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

int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (strlen(hex) != 6) return 0;

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

void reset_cell(Cell *c) {
    c->codepoint = ' ';
    c->fg = current_fg;
    c->bold = current_bold;
    c->underline = current_underline;
}

void clear_screen() {
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            reset_cell(&screen[r][c]);
        }
    }
}

void set_cursor_position(int row, int col) {
    if (row >= 1 && row <= TERM_ROWS) {
        cursor_row = row - 1;
    }

    if (col >= 1 && col <= TERM_COLS) {
        cursor_col = col - 1;
    }
}

void scroll_up() {
    for (int r = 0; r < TERM_ROWS - 1; r++) {
        memcpy(screen[r], screen[r + 1], sizeof(Cell) * TERM_COLS);
    }

    for (int c = 0; c < TERM_COLS; c++) {
        reset_cell(&screen[TERM_ROWS - 1][c]);
    }
}

void put_char(Uint32 ch) {
    if (ch == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (ch == '\r') {
        cursor_col = 0;
    } else if (ch == '\b') {
        if (cursor_col > 0) cursor_col--;
    } else {
        if (cursor_col >= TERM_COLS) {
            cursor_row++;
            cursor_col = 0;
        }

        if (cursor_row >= TERM_ROWS) {
            scroll_up();
            cursor_row = TERM_ROWS - 1;
        }

        Cell *cell = &screen[cursor_row][cursor_col];

        cell->codepoint = ch;
        cell->fg = use_solid_fg ? solid_fg : current_fg;
        cell->bold = current_bold;
        cell->underline = current_underline;

        cursor_col++;
    }

    if (cursor_row >= TERM_ROWS) {
        scroll_up();
        cursor_row = TERM_ROWS - 1;
    }
}

void parse_ansi(const char *seq) {
    if (use_solid_fg) return;
    int params[8] = {0}, count = 0;
    const char *p = seq;
    while (*p && *p != 'm' && *p != 'H' && *p != 'J' && *p != 'K') {
        if (*p >= '0' && *p <= '9') {
            long val = strtol(p, (char **) &p, 10);
            params[count] = (val > INT_MAX) ? INT_MAX : (val < INT_MIN) ? INT_MIN : (int) val;
        } else if (*p == ';') {
            count++;
            p++;
        } else p++;
    }
    char cmd = *p;
    switch (cmd) {
        case 'H':
            set_cursor_position(params[0] ? params[0] : 1, params[1] ? params[1] : 1);
            break;
        case 'J':
            if (params[0] == 2) clear_screen();
            break;
        case 'K':
            for (int c = cursor_col; c < TERM_COLS; c++) reset_cell(&screen[cursor_row][c]);
            break;
        case 'm':
            for (int i = 0; i <= count; i++) {
                int code = params[i];
                if (code == 0) {
                    current_fg = base_colours[7];
                    current_bold = 0;
                    current_underline = 0;
                } else if (code == 1) current_bold = 1;
                else if (code == 4) current_underline = 1;
                else if (code >= 30 && code <= 37)
                    current_fg = current_bold ? bright_colours[code - 30] : base_colours[code - 30];
            }
            break;
        default:
            break;
    }
}

void render_screen(SDL_Renderer *ren, TTF_Font *font) {
    char buf[8];
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            Cell *cell = &screen[r][c];
            if (cell->codepoint == ' ') continue;
            snprintf(buf, sizeof(buf), "%lc", (wint_t) cell->codepoint);
            SDL_Color draw_fg = use_solid_fg ? solid_fg : cell->fg;
            SDL_Surface *surf = TTF_RenderUTF8_Blended(font, buf, draw_fg);
            if (!surf) continue;
            SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
            if (tex) {
                SDL_Rect dst = {c * CELL_WIDTH, r * CELL_HEIGHT, surf->w, surf->h};
                SDL_RenderCopy(ren, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
}

int main(int argc, char *argv[]) {
    load_device(&device);
    load_config(&config);

    int term_width = 0;
    int term_height = 0;
    int font_size = 0;

    const char *font_path = NULL;
    const char *bg_path = NULL;
    const char **cmd = NULL;

    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) print_help(argv[0]);

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
        if (strcasecmp(device.DEVICE.NAME, "tui-brick") == 0) {
            font_size = 28;
        } else {
            font_size = 16;
        }
    }
    if (!font_path) font_path = "/opt/muos/share/font/muterm.ttf";

    if (!cmd) print_help(argv[0]);

    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    TTF_Font *font = TTF_OpenFont(font_path, font_size);
    if (!font) return 1;

    TTF_SizeUTF8(font, "M", &CELL_WIDTH, &CELL_HEIGHT);
    TERM_COLS = term_width / CELL_WIDTH;
    TERM_ROWS = term_height / CELL_HEIGHT;

    screen = malloc(sizeof(Cell *) * TERM_ROWS);
    for (int r = 0; r < TERM_ROWS; r++) screen[r] = calloc(TERM_COLS, sizeof(Cell));

    SDL_Window *win = SDL_CreateWindow("muOS Terminal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       term_width, term_height, SDL_WINDOW_SHOWN);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture *render_target = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                                   TERM_COLS * CELL_WIDTH, TERM_ROWS * CELL_HEIGHT);

    SDL_Texture *bg_texture = NULL;
    if (bg_path && access(bg_path, R_OK) == 0) {
        SDL_Surface *bg_surface = IMG_Load(bg_path);
        if (bg_surface) {
            bg_texture = SDL_CreateTextureFromSurface(ren, bg_surface);
            SDL_FreeSurface(bg_surface);
        }
    }

    clear_screen();

    int pty_fd;
    pid_t child = forkpty(&pty_fd, NULL, NULL, NULL);
    if (child == 0) {
        if (cmd) { // Surely we are running something...?!
            size_t cmd_count = 0;
            while (cmd[cmd_count]) cmd_count++;
            run_exec(cmd, cmd_count + 1);
            exit(127);
        }
    }

    fcntl(pty_fd, F_SETFL, fcntl(pty_fd, F_GETFL) | O_NONBLOCK);

    char buf[MAX_BUFFER_SIZE], esc_seq[64];
    int esc_len = 0, in_esc = 0;
    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) running = 0;

        ssize_t n;
        while ((n = read(pty_fd, buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; i++) {
                unsigned char ch = buf[i];
                if (in_esc) {
                    if (esc_len < sizeof(esc_seq) - 1) esc_seq[esc_len++] = ch;
                    if ((ch >= 'A' && ch <= 'Z') || ch == 'm') {
                        esc_seq[esc_len] = '\0';
                        parse_ansi(esc_seq);
                        esc_len = 0;
                        in_esc = 0;
                    }
                } else if (ch == 0x1B) {
                    in_esc = 1;
                    esc_len = 0;
                } else {
                    put_char(ch);
                }
            }
        }

        SDL_SetRenderTarget(ren, render_target);

        if (bg_texture) {
            SDL_RenderCopy(ren, bg_texture, NULL, NULL);
        } else if (use_solid_bg) {
            SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255), SDL_RenderClear(ren);
        } else {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255), SDL_RenderClear(ren);
        }

        render_screen(ren, font);
        SDL_SetRenderTarget(ren, NULL);

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

        SDL_RenderClear(ren);
        SDL_RenderCopyExF(ren, render_target, NULL, &dest_rect, angle, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(ren);
        SDL_Delay(33);

        int status;
        if (waitpid(child, &status, WNOHANG) > 0) running = 0;
    }

    for (int r = 0; r < TERM_ROWS; r++) free(screen[r]);

    free(screen);

    if (render_target) SDL_DestroyTexture(render_target);
    if (bg_texture) SDL_DestroyTexture(bg_texture);

    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
