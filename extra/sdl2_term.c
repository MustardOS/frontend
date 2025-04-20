#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FADE_STEPS 32
#define FADE_DELAY 16

typedef struct {
    Uint32 codepoint;
    SDL_Color fg;
    int bold;
    int underline;
} Cell;

Cell **screen = NULL;
int TERM_COLS, TERM_ROWS, CELL_WIDTH, CELL_HEIGHT;
int cursor_row = 0, cursor_col = 0;
SDL_Color current_fg = {255, 255, 255, 255};
int current_bold = 0, current_underline = 0;
SDL_Color solid_bg = {0, 0, 0, 255};
int use_solid_bg = 0;

SDL_Color base_colors[8] = {
    {0, 0, 0, 255}, {170, 0, 0, 255}, {0, 170, 0, 255}, {170, 85, 0, 255},
    {0, 0, 170, 255}, {170, 0, 170, 255}, {0, 170, 170, 255}, {170, 170, 170, 255}
};
SDL_Color bright_colors[8] = {
    {85, 85, 85, 255}, {255, 85, 85, 255}, {85, 255, 85, 255}, {255, 255, 85, 255},
    {85, 85, 255, 255}, {255, 85, 255, 255}, {85, 255, 255, 255}, {255, 255, 255, 255}
};

void print_help(const char *name) {
    printf("Usage: %s -w <width> -h <height> -s <font size> -f <font path> [-b <image.png>] [-c RRGGBB] <command>\n\n", name);
    printf("Arguments:\n");
    printf("  -w <width>      Window width in pixels\n");
    printf("  -h <height>     Window height in pixels\n");
    printf("  -s <font size>  Font size in points\n");
    printf("  -f <font path>  Path to TTF font file\n");
    printf("  -b image.png    Optional PNG background image\n");
    printf("  -c RRGGBB       Optional solid background hex colour\n");
    printf("  <command>       Shell command or script to execute\n");
    exit(0);
}

int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (strlen(hex) != 6) return 0;
    unsigned int r, g, b;
    if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3) return 0;
    out->r = r; out->g = g; out->b = b; out->a = 255;
    return 1;
}

void reset_cell(Cell *c) {
    c->codepoint = ' ';
    c->fg = current_fg;
    c->bold = current_bold;
    c->underline = current_underline;
}

void clear_screen() {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            reset_cell(&screen[r][c]);
}

void set_cursor_position(int row, int col) {
    if (row >= 1 && row <= TERM_ROWS) cursor_row = row - 1;
    if (col >= 1 && col <= TERM_COLS) cursor_col = col - 1;
}

void scroll_up() {
    for (int r = 0; r < TERM_ROWS - 1; r++)
        memcpy(screen[r], screen[r + 1], sizeof(Cell) * TERM_COLS);
    for (int c = 0; c < TERM_COLS; c++)
        reset_cell(&screen[TERM_ROWS - 1][c]);
}

void put_char(Uint32 ch) {
    if (ch == '\n') { cursor_row++; cursor_col = 0; }
    else if (ch == '\r') { cursor_col = 0; }
    else if (ch == '\b') { if (cursor_col > 0) cursor_col--; }
    else {
        if (cursor_col >= TERM_COLS) { cursor_row++; cursor_col = 0; }
        if (cursor_row >= TERM_ROWS) { scroll_up(); cursor_row = TERM_ROWS - 1; }
        Cell *cell = &screen[cursor_row][cursor_col];
        cell->codepoint = ch;
        cell->fg = current_fg;
        cell->bold = current_bold;
        cell->underline = current_underline;
        cursor_col++;
    }
    if (cursor_row >= TERM_ROWS) { scroll_up(); cursor_row = TERM_ROWS - 1; }
}

void parse_ansi(const char *seq) {
    int params[8] = {0}, count = 0;
    const char *p = seq;
    while (*p && *p != 'm' && *p != 'H' && *p != 'J' && *p != 'K') {
        if (*p >= '0' && *p <= '9') params[count] = strtol(p, (char **)&p, 10);
        else if (*p == ';') { count++; p++; }
        else p++;
    }
    char cmd = *p;
    switch (cmd) {
        case 'H': set_cursor_position(params[0] ? params[0] : 1, params[1] ? params[1] : 1); break;
        case 'J': if (params[0] == 2) clear_screen(); break;
        case 'K': for (int c = cursor_col; c < TERM_COLS; c++) reset_cell(&screen[cursor_row][c]); break;
        case 'm':
            for (int i = 0; i <= count; i++) {
                int code = params[i];
                if (code == 0) { current_fg = base_colors[7]; current_bold = 0; current_underline = 0; }
                else if (code == 1) current_bold = 1;
                else if (code == 4) current_underline = 1;
                else if (code >= 30 && code <= 37)
                    current_fg = current_bold ? bright_colors[code - 30] : base_colors[code - 30];
            }
            break;
    }
}

void render_screen(SDL_Renderer *ren, TTF_Font *font) {
    char buf[8];
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            Cell *cell = &screen[r][c];
            if (cell->codepoint == ' ') continue;
            int len = snprintf(buf, sizeof(buf), "%lc", (wint_t)cell->codepoint);
            SDL_Surface *surf = TTF_RenderUTF8_Blended(font, buf, cell->fg);
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

void fade(SDL_Renderer *ren, TTF_Font *font, SDL_Texture *bg, int w, int h, int fade_out) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_Rect r = {0, 0, w, h};
    for (int i = 0; i <= FADE_STEPS; i++) {
        int alpha = (fade_out ? i : (FADE_STEPS - i)) * 255 / FADE_STEPS;
        SDL_RenderClear(ren);
        if (bg) {
            int iw, ih;
            SDL_QueryTexture(bg, NULL, NULL, &iw, &ih);
            SDL_Rect dst = {(w - iw) / 2, (h - ih) / 2, iw, ih};
            SDL_RenderCopy(ren, bg, NULL, &dst);
        } else if (use_solid_bg) {
            SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
            SDL_RenderFillRect(ren, &r);
        }
        render_screen(ren, font);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
        SDL_RenderFillRect(ren, &r);
        SDL_RenderPresent(ren);
        SDL_Delay(FADE_DELAY);
    }
}

int main(int argc, char *argv[]) {
    int win_w = 0, win_h = 0, font_size = 0;
    const char *font_path = NULL;
    const char *bg_path = NULL;
    const char *cmd = NULL;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
    }

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) win_w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) win_h = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) font_size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) font_path = argv[++i];
        else if (!strcmp(argv[i], "-b") && i + 1 < argc) bg_path = argv[++i];
        else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            if (!parse_hex_colour(argv[++i], &solid_bg)) {
                fprintf(stderr, "Error: Invalid hex colour. Use format: RRGGBB\n");
                return 1;
            }
            use_solid_bg = 1;
        } else if (argv[i][0] != '-') cmd = argv[i];
    }

    if (!win_w || !win_h || !font_size || !font_path || !cmd) {
        fprintf(stderr, "Error: Missing required arguments.\n\n");
        print_help(argv[0]);
    }

    if (access(font_path, R_OK) != 0) {
        fprintf(stderr, "Error: Font file '%s' not found or unreadable.\n", font_path);
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    TTF_Font *font = TTF_OpenFont(font_path, font_size);
    if (!font) {
        fprintf(stderr, "Error: Could not load font: %s\n", font_path);
        return 1;
    }

    TTF_SizeUTF8(font, "M", &CELL_WIDTH, &CELL_HEIGHT);
    TERM_COLS = win_w / CELL_WIDTH;
    TERM_ROWS = win_h / CELL_HEIGHT;

    screen = malloc(sizeof(Cell*) * TERM_ROWS);
    for (int r = 0; r < TERM_ROWS; r++)
        screen[r] = calloc(TERM_COLS, sizeof(Cell));

    SDL_Window *win = SDL_CreateWindow("SDL2 Terminal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       TERM_COLS * CELL_WIDTH, TERM_ROWS * CELL_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture *bgtex = NULL;
    if (bg_path && access(bg_path, R_OK) == 0) {
        SDL_Surface *bgsurf = IMG_Load(bg_path);
        if (bgsurf) {
            bgtex = SDL_CreateTextureFromSurface(ren, bgsurf);
            SDL_FreeSurface(bgsurf);
        }
    }

    clear_screen();
    fade(ren, font, bgtex, TERM_COLS * CELL_WIDTH, TERM_ROWS * CELL_HEIGHT, 0);

    int pty_fd;
    pid_t child = forkpty(&pty_fd, NULL, NULL, NULL);
    if (child == 0) {
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        exit(127);
    }

    fcntl(pty_fd, F_SETFL, fcntl(pty_fd, F_GETFL) | O_NONBLOCK);

    char buf[1024], esc_seq[64];
    int esc_len = 0, in_esc = 0;
    SDL_Event e;
    int running = 1;

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
                        esc_len = 0; in_esc = 0;
                    }
                } else if (ch == 0x1B) {
                    in_esc = 1;
                    esc_len = 0;
                } else {
                    put_char(ch);
                }
            }
        }

        SDL_RenderClear(ren);
        if (bgtex) {
            int iw, ih;
            SDL_QueryTexture(bgtex, NULL, NULL, &iw, &ih);
            SDL_Rect dst = {(TERM_COLS * CELL_WIDTH - iw) / 2,
                            (TERM_ROWS * CELL_HEIGHT - ih) / 2,
                            iw, ih};
            SDL_RenderCopy(ren, bgtex, NULL, &dst);
        } else if (use_solid_bg) {
            SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
            SDL_RenderClear(ren);
        }
        render_screen(ren, font);
        SDL_RenderPresent(ren);
        SDL_Delay(33);

        int status;
        if (waitpid(child, &status, WNOHANG) > 0) running = 0;
    }

    for (int r = 0; r < TERM_ROWS; r++) free(screen[r]);

    free(screen);

    if (bgtex) SDL_DestroyTexture(bgtex);

    TTF_CloseFont(font);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}

