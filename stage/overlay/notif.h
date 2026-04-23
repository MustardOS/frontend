#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GLES2/gl2.h>
#include "../common/common.h"

#define NOTIF_PATH "/run/muos/overlay.notif"
#define NOTIF_FONT "/opt/muos/share/font/mustage.ttf"

#define NOTIF_PADDING 12
#define NOTIF_BORDER  2
#define NOTIF_ALPHA   100

#define NOTIF_MAX_BOX_FRAC 0.80f
#define NOTIF_SCROLL_PPS   60
#define NOTIF_SCROLL_DELAY 1200
#define NOTIF_MARQUEE_GAP  80

typedef enum {
    NOTIF_POS_TOP,
    NOTIF_POS_MIDDLE,
    NOTIF_POS_BOTTOM
} notif_pos_t;

typedef struct {
    int font_size;
    SDL_Color fg;
    SDL_Color border;
    notif_pos_t position;
    int dim;
    char text[1024];
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
