#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GLES2/gl2.h>
#include "../common/common.h"

#define NOTIF_PATH "/run/muos/overlay.notif"
#define NOTIF_FONT "/opt/muos/share/font/mustage.ttf"

#define NOTIF_PADDING 12
#define NOTIF_BORDER  2

#define NOTIF_MAX_BOX_FRAC 0.80f
#define NOTIF_SCROLL_PPS   60
#define NOTIF_SCROLL_DELAY 1200
#define NOTIF_MARQUEE_GAP  80

#define NOTIF_MAX_TEXT_LINES 128
#define NOTIF_TEXT_LINE_LEN  512

typedef enum {
    NOTIF_POS_TOP_LEFT = 0,
    NOTIF_POS_TOP_CENTRE = 1,
    NOTIF_POS_TOP_RIGHT = 2,
    NOTIF_POS_MID_LEFT = 3,
    NOTIF_POS_CENTRE = 4,
    NOTIF_POS_MID_RIGHT = 5,
    NOTIF_POS_BOT_LEFT = 6,
    NOTIF_POS_BOT_CENTRE = 7,
    NOTIF_POS_BOT_RIGHT = 8
} notif_pos_t;

typedef enum {
    NOTIF_ALIGN_AUTO = 0,
    NOTIF_ALIGN_RIGHT = 1,
    NOTIF_ALIGN_CENTRE = 2,
    NOTIF_ALIGN_LEFT = 3
} notif_align_t;

typedef struct {
    int font_size;
    notif_align_t font_align;

    SDL_Color font_colour;
    Uint8 font_alpha;

    SDL_Color box_colour;
    Uint8 box_alpha;

    SDL_Color border_colour;
    Uint8 border_alpha;

    SDL_Color dim_colour;
    Uint8 dim_alpha;

    notif_pos_t position;

    int text_line_count;
    char text_lines[NOTIF_MAX_TEXT_LINES][NOTIF_TEXT_LINE_LEN];
} notif_cfg_t;

extern int notif_visible;
extern notif_cfg_t notif_cfg;

void notif_update(void);

int notif_is_visible(void);

void sdl_notif_free(void);

void sdl_notif_draw(SDL_Renderer *renderer, int fb_w, int fb_h);

void gl_notif_free(void);

int gl_notif_prepare(int fb_w, int fb_h);

GLuint gl_notif_get_tex(void);

const gl_vtx_t *gl_notif_get_vtx(void);

int gl_notif_needs_dim(void);
