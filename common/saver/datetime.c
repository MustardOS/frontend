#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <common/runtime/log.h>
#include <common/saver/saver.h>
#include <common/config/config.h>
#include <common/saver/datetime.h>

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

    SDL_Texture *tex_time;
    SDL_Texture *tex_time_left;
    SDL_Texture *tex_colon;
    SDL_Texture *tex_time_right;
    SDL_Texture *tex_date;

    char cached_time[32];
    char cached_date[64];
    time_t cached_second;
    int time_left_w;
    int colon_w;
} datetime_module_t;

static datetime_module_t mod = {0};

static int dt_load_fonts(int screen_h) {
    char path[512];
    const char *name = config.settings.font.name[0] ? config.settings.font.name : "Noto Sans";

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
        TTF_CloseFont(mod.font_time);
        mod.font_time = NULL;
        return 0;
    }

    return 1;
}

static void dt_pick_position(void) {
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

int datetime_init(
    SDL_Renderer *renderer, int screen_w, int screen_h, uint8_t col_r, uint8_t col_g, uint8_t col_b, uint8_t col_a
) {
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

    saver_init_base(
        &mod.base, screen_w, screen_h, "DateTime", mod.col_r, mod.col_g, mod.col_b, dt_on_speed_changed,
        dt_on_idle_enter, &mod
    );

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

static void dt_destroy_texture(SDL_Texture **tex) {
    if (!*tex) return;

    SDL_DestroyTexture(*tex);
    *tex = NULL;
}

static SDL_Texture *dt_render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text) {
    SDL_Color c = {mod.col_r, mod.col_g, mod.col_b, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return NULL;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

static void dt_blit(SDL_Renderer *renderer, SDL_Texture *tex, int x, int y, uint8_t alpha) {
    if (!tex) return;
    int w, h;

    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_Rect r = {x, y, w, h};

    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_RenderCopy(renderer, tex, NULL, &r);
}

static void dt_format_strings(const struct tm *tm, char time_str[32], char date_str[64]) {
    if (config.clock.notation == 0)
        strftime(time_str, 32, "%I:%M %p", tm);
    else
        strftime(time_str, 32, "%H:%M", tm);

    char wday[16], mon[16];
    strftime(wday, sizeof(wday), "%a", tm);
    strftime(mon, sizeof(mon), "%b", tm);
    snprintf(date_str, 64, "%s %d %s, %d", wday, tm->tm_mday, mon, tm->tm_year + 1900);
}

static void dt_refresh_time_texture(SDL_Renderer *renderer, const char *time_str) {
    if (strcmp(mod.cached_time, time_str) == 0) return;

    dt_destroy_texture(&mod.tex_time);
    dt_destroy_texture(&mod.tex_time_left);
    dt_destroy_texture(&mod.tex_colon);
    dt_destroy_texture(&mod.tex_time_right);

    mod.time_left_w = 0;
    mod.colon_w = 0;

    TTF_SizeUTF8(mod.font_time, time_str, &mod.time_w, &mod.time_h);

    if (config.clock.notation == 0) {
        const char *cp = strchr(time_str, ':');
        if (cp) {
            char left[16];
            char right[16];
            const size_t left_len = (size_t) (cp - time_str);

            memcpy(left, time_str, left_len);
            left[left_len] = '\0';
            snprintf(right, sizeof(right), "%s", cp + 1);

            mod.tex_time_left = dt_render_text(renderer, mod.font_time, left);
            mod.tex_colon = dt_render_text(renderer, mod.font_time, ":");
            mod.tex_time_right = dt_render_text(renderer, mod.font_time, right);

            int dummy;
            TTF_SizeUTF8(mod.font_time, left, &mod.time_left_w, &dummy);
            TTF_SizeUTF8(mod.font_time, ":", &mod.colon_w, &dummy);
        } else {
            mod.tex_time = dt_render_text(renderer, mod.font_time, time_str);
        }
    } else {
        mod.tex_time = dt_render_text(renderer, mod.font_time, time_str);
    }

    snprintf(mod.cached_time, sizeof(mod.cached_time), "%s", time_str);
}

static void dt_refresh_date_texture(SDL_Renderer *renderer, const char *date_str) {
    if (strcmp(mod.cached_date, date_str) == 0) return;

    dt_destroy_texture(&mod.tex_date);
    TTF_SizeUTF8(mod.font_date, date_str, &mod.date_w, &mod.date_h);
    mod.tex_date = dt_render_text(renderer, mod.font_date, date_str);
    snprintf(mod.cached_date, sizeof(mod.cached_date), "%s", date_str);
}

static void dt_update_block_size(void) {
    mod.block_w = mod.time_w > mod.date_w ? mod.time_w : mod.date_w;
    mod.gap = mod.time_h / 8;
    mod.block_h = mod.time_h + mod.gap + mod.date_h;
}

void datetime_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.font_time || !mod.font_date) return;

    uint32_t now = SDL_GetTicks();

    const time_t t = time(NULL);
    if (!mod.cached_time[0] || t != mod.cached_second) {
        const struct tm *tm = localtime(&t);
        char time_str[32];
        char date_str[64];

        dt_format_strings(tm, time_str, date_str);
        dt_refresh_time_texture(renderer, time_str);
        dt_refresh_date_texture(renderer, date_str);
        dt_update_block_size();
        mod.cached_second = t;
    }

    if (mod.reposition_pending) {
        dt_pick_position();
        mod.reposition_pending = 0;
        mod.phase_start = now;
    }

    uint8_t alpha = dt_current_alpha(now);
    if (alpha == 0) return;

    int is_12h = config.clock.notation == 0;
    int time_x = mod.pos_x + (mod.block_w - mod.time_w) / 2;

    if (is_12h && mod.tex_colon) {
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

        dt_blit(renderer, mod.tex_time_left, time_x, mod.pos_y, alpha);
        dt_blit(renderer, mod.tex_colon, time_x + mod.time_left_w, mod.pos_y, colon_alpha);
        dt_blit(renderer, mod.tex_time_right, time_x + mod.time_left_w + mod.colon_w, mod.pos_y, alpha);
    } else {
        dt_blit(renderer, mod.tex_time, time_x, mod.pos_y, alpha);
    }

    int date_x = mod.pos_x + (mod.block_w - mod.date_w) / 2;
    int date_y = mod.pos_y + mod.time_h + mod.gap;
    dt_blit(renderer, mod.tex_date, date_x, date_y, alpha);
}

int datetime_active(void) {
    return saver_active_base(&mod.base);
}

void datetime_stop(void) {
    saver_stop_base(&mod.base);
}

void datetime_shutdown(void) {
    dt_destroy_texture(&mod.tex_time);
    dt_destroy_texture(&mod.tex_time_left);
    dt_destroy_texture(&mod.tex_colon);
    dt_destroy_texture(&mod.tex_time_right);
    dt_destroy_texture(&mod.tex_date);

    mod.cached_time[0] = '\0';
    mod.cached_date[0] = '\0';
    mod.cached_second = 0;

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
