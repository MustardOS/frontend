#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../log.h"
#include "../saver.h"
#include "../config.h"
#include "datetime.h"

#define DT_FADE_IN_MS   1200
#define DT_FADE_HOLD_MS 6000
#define DT_FADE_OUT_MS  1200

#define DT_COLON_FADE_MS 150

#define DT_MARGIN 32

typedef enum {
    DT_FADE_IN = 0,
    DT_FADE_HOLD,
    DT_FADE_OUT,
} dt_fade_state_t;

typedef struct {
    saver_state_t base;

    TTF_Font *font_time;
    TTF_Font *font_date;

    uint8_t col_r, col_g, col_b, col_a;

    dt_fade_state_t fade_state;
    uint32_t phase_start;

    int reposition_pending;

    int pos_x, pos_y;

    int block_w, block_h;
    int time_w, time_h;
    int date_w, date_h;
    int gap;
} datetime_module_t;

static datetime_module_t mod = {0};

static int dt_load_fonts(int screen_h) {
    char path[512];
    const char *name = config.SETTINGS.FONT.NAME[0] ? config.SETTINGS.FONT.NAME : "Noto Sans";

    int sz_time = screen_h / 7;
    if (sz_time < 24) sz_time = 24;

    int sz_date = screen_h / 18;
    if (sz_date < 12) sz_date = 12;

    snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", name);
    mod.font_time = TTF_OpenFont(path, sz_time);
    if (!mod.font_time && strcmp(name, "Noto Sans") != 0) {
        snprintf(path, sizeof(path), INTERNAL_FONTS "/Noto Sans.ttf");
        mod.font_time = TTF_OpenFont(path, sz_time);
    }

    if (!mod.font_time) {
        LOG_ERROR("saver", "DateTime: failed to open time font: %s", TTF_GetError());
        return 0;
    }

    snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", name);
    mod.font_date = TTF_OpenFont(path, sz_date);
    if (!mod.font_date && strcmp(name, "Noto Sans") != 0) {
        snprintf(path, sizeof(path), INTERNAL_FONTS "/Noto Sans.ttf");
        mod.font_date = TTF_OpenFont(path, sz_date);
    }

    if (!mod.font_date) {
        LOG_ERROR("saver", "DateTime: failed to open date font: %s", TTF_GetError());
        return 0;
    }

    return 1;
}

static void dt_pick_position(void) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char time_str[32];
    char date_str[64];

    if (config.CLOCK.NOTATION == 0) {
        strftime(time_str, sizeof(time_str), "%I:%M %p", tm);
    } else {
        strftime(time_str, sizeof(time_str), "%H:%M", tm);
    }

    char wday[16], mon[16];
    strftime(wday, sizeof(wday), "%a", tm);
    strftime(mon, sizeof(mon), "%b", tm);
    snprintf(date_str, sizeof(date_str), "%s %d %s, %d", wday, tm->tm_mday, mon, tm->tm_year + 1900);

    int tw = 0, th = 0, dw = 0, dh = 0;
    TTF_SizeUTF8(mod.font_time, time_str, &tw, &th);
    TTF_SizeUTF8(mod.font_date, date_str, &dw, &dh);

    mod.time_w = tw;
    mod.time_h = th;
    mod.date_w = dw;
    mod.date_h = dh;
    mod.block_w = (tw > dw) ? tw : dw;
    mod.gap = th / 8;
    mod.block_h = th + mod.gap + dh;

    int max_x = mod.base.screen_w - mod.block_w - DT_MARGIN * 2;
    int max_y = mod.base.screen_h - mod.block_h - DT_MARGIN * 2;

    if (max_x < 1) max_x = 1;
    if (max_y < 1) max_y = 1;

    mod.pos_x = DT_MARGIN + (int) saver_rand_range(max_x);
    mod.pos_y = DT_MARGIN + (int) saver_rand_range(max_y);
}

static void dt_on_speed_changed(void *user) {
    (void) user;
}

static void dt_on_idle_enter(void *user) {
    (void) user;

    mod.reposition_pending = 1;
    mod.fade_state = DT_FADE_IN;
    mod.phase_start = SDL_GetTicks();
}

int datetime_init(SDL_Renderer *renderer, int screen_w, int screen_h, uint8_t col_r, uint8_t col_g, uint8_t col_b, uint8_t col_a) {
    (void) renderer;

    if (!TTF_WasInit() && TTF_Init() != 0) {
        LOG_ERROR("saver", "DateTime: TTF_Init failed: %s", TTF_GetError());
        return 0;
    }

    if (!dt_load_fonts(screen_h)) return 0;

    mod.col_r = col_r;
    mod.col_g = col_g;
    mod.col_b = col_b;
    mod.col_a = col_a;

    saver_init_base(&mod.base, screen_w, screen_h, "DateTime", mod.col_r, mod.col_g, mod.col_b, dt_on_speed_changed, dt_on_idle_enter, &mod);

    mod.reposition_pending = 1;
    mod.fade_state = DT_FADE_IN;
    mod.phase_start = SDL_GetTicks();

    LOG_INFO("saver", "DateTime Initialised (%dx%d)", screen_w, screen_h);

    return 1;
}

void datetime_update(void) {
    uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    uint32_t elapsed = now - mod.phase_start;

    switch (mod.fade_state) {
        case DT_FADE_IN:
            if (elapsed >= DT_FADE_IN_MS) {
                mod.fade_state = DT_FADE_HOLD;
                mod.phase_start = now;
            }
            break;
        case DT_FADE_HOLD:
            if (elapsed >= DT_FADE_HOLD_MS) {
                mod.fade_state = DT_FADE_OUT;
                mod.phase_start = now;
            }
            break;
        case DT_FADE_OUT:
            if (elapsed >= DT_FADE_OUT_MS) {
                mod.reposition_pending = 1;
                mod.fade_state = DT_FADE_IN;
                mod.phase_start = now;
            }
            break;
    }
}

static uint8_t dt_current_alpha(uint32_t now) {
    uint32_t elapsed = now - mod.phase_start;
    float t;

    switch (mod.fade_state) {
        case DT_FADE_IN:
            t = (float) elapsed / (float) DT_FADE_IN_MS;
            if (t > 1.0f) t = 1.0f;
            return (uint8_t) (t * (float) mod.col_a);
        case DT_FADE_HOLD:
            return mod.col_a;
        case DT_FADE_OUT:
            t = (float) elapsed / (float) DT_FADE_OUT_MS;
            if (t > 1.0f) t = 1.0f;
            return (uint8_t) ((1.0f - t) * (float) mod.col_a);
    }

    return mod.col_a;
}

static SDL_Texture *dt_render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, uint8_t alpha) {
    SDL_Color c = {mod.col_r, mod.col_g, mod.col_b, alpha};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return NULL;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

static void dt_blit(SDL_Renderer *renderer, SDL_Texture *tex, int x, int y) {
    if (!tex) return;
    int w, h;

    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_Rect r = {x, y, w, h};

    SDL_RenderCopy(renderer, tex, NULL, &r);
    SDL_DestroyTexture(tex);
}

void datetime_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.font_time || !mod.font_date) return;

    uint32_t now = SDL_GetTicks();

    if (mod.reposition_pending) {
        dt_pick_position();
        mod.reposition_pending = 0;
        mod.phase_start = now;
    }

    uint8_t alpha = dt_current_alpha(now);
    if (alpha == 0) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    int is_12h = (config.CLOCK.NOTATION == 0);
    int time_x = mod.pos_x + (mod.block_w - mod.time_w) / 2;

    if (is_12h) {
        char full[32];
        strftime(full, sizeof(full), "%I:%M %p", tm);

        char *cp = strchr(full, ':');
        if (!cp) {
            dt_blit(renderer, dt_render_text(renderer, mod.font_time, full, alpha), time_x, mod.pos_y);
        } else {
            char left[16], colon_str[2] = ":", right[16];
            int left_len = (int) (cp - full);

            strncpy(left, full, (size_t) left_len);
            left[left_len] = '\0';

            strncpy(right, cp + 1, sizeof(right) - 1);
            right[sizeof(right) - 1] = '\0';

            uint8_t colon_alpha = alpha;
            if (mod.fade_state == DT_FADE_HOLD) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);

                long sub_ms = ts.tv_nsec / 1000000L;
                int colon_on = (ts.tv_sec % 2) == 0;

                float factor;
                if (colon_on) {
                    factor = (sub_ms < DT_COLON_FADE_MS) ? (float) sub_ms / (float) DT_COLON_FADE_MS : 1.0f;
                } else {
                    factor = (sub_ms < DT_COLON_FADE_MS) ? 1.0f - (float) sub_ms / (float) DT_COLON_FADE_MS : 0.0f;
                }

                uint8_t colon_dim = alpha / 8;
                colon_alpha = colon_dim + (uint8_t) ((float) (alpha - colon_dim) * factor);
            }

            int lw = 0, cw = 0, dummy;
            TTF_SizeUTF8(mod.font_time, left, &lw, &dummy);
            TTF_SizeUTF8(mod.font_time, colon_str, &cw, &dummy);

            dt_blit(renderer, dt_render_text(renderer, mod.font_time, left, alpha), time_x, mod.pos_y);
            dt_blit(renderer, dt_render_text(renderer, mod.font_time, colon_str, colon_alpha), time_x + lw, mod.pos_y);
            dt_blit(renderer, dt_render_text(renderer, mod.font_time, right, alpha), time_x + lw + cw, mod.pos_y);
        }
    } else {
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M", tm);
        dt_blit(renderer, dt_render_text(renderer, mod.font_time, time_str, alpha), time_x, mod.pos_y);
    }

    int date_x = mod.pos_x + (mod.block_w - mod.date_w) / 2;
    int date_y = mod.pos_y + mod.time_h + mod.gap;

    char wday[16], mon[16], date_str[64];
    strftime(wday, sizeof(wday), "%a", tm);
    strftime(mon, sizeof(mon), "%b", tm);
    snprintf(date_str, sizeof(date_str), "%s %d %s, %d", wday, tm->tm_mday, mon, tm->tm_year + 1900);

    dt_blit(renderer, dt_render_text(renderer, mod.font_date, date_str, alpha), date_x, date_y);
}

int datetime_active(void) {
    return saver_active_base(&mod.base);
}

void datetime_stop(void) {
    saver_stop_base(&mod.base);
}

void datetime_shutdown(void) {
    if (mod.font_time) {
        TTF_CloseFont(mod.font_time);
        mod.font_time = NULL;
    }

    if (mod.font_date) {
        TTF_CloseFont(mod.font_date);
        mod.font_date = NULL;
    }

    saver_shutdown_base(&mod.base);
}
