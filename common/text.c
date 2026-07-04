#include <stdlib.h>
#include "text.h"
#include "init.h"
#include "log.h"

#define TXT_SHADOW_ALPHA 160

void sdl_text_die(const char *msg) {
    LOG_ERROR(mux_module, "%s: %s", msg, SDL_GetError());
    exit(1);
}

float sdl_smoothstep01(const float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    return t * t * (3.0f - 2.0f * t);
}

SDL_Surface *sdl_render_text_surface(TTF_Font *font, const char *text, const SDL_Color col, const int shadow_offset) {
    if (!text || !*text) return NULL;

    const SDL_Color shadow_col = {0, 0, 0, 255};

    SDL_Surface *shadow = TTF_RenderUTF8_Blended(font, text, shadow_col);
    if (!shadow) return NULL;

    SDL_Surface *front = TTF_RenderUTF8_Blended(font, text, col);
    if (!front) {
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(
        0, front->w + shadow_offset, front->h + shadow_offset, 32, SDL_PIXELFORMAT_RGBA32
    );
    if (!out) {
        SDL_FreeSurface(front);
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_FillRect(out, NULL, SDL_MapRGBA(out->format, 0, 0, 0, 0));

    SDL_Rect dst;
    dst.x = shadow_offset;
    dst.y = shadow_offset;
    dst.w = shadow->w;
    dst.h = shadow->h;

    SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceAlphaMod(shadow, TXT_SHADOW_ALPHA);
    SDL_BlitSurface(shadow, NULL, out, &dst);

    dst.x = 0;
    dst.y = 0;
    dst.w = front->w;
    dst.h = front->h;

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(front, NULL, out, &dst);

    SDL_FreeSurface(front);
    SDL_FreeSurface(shadow);

    return out;
}

SDL_Surface *sdl_render_text_wrapped_surface(
    TTF_Font *font, const char *text, const SDL_Color col, const int wrap_w, const int shadow_offset
) {
    if (!text || !*text) return NULL;

    const SDL_Color shadow_col = {0, 0, 0, 255};

    TTF_SetFontWrappedAlign(font, TTF_WRAPPED_ALIGN_CENTER);

    SDL_Surface *shadow = TTF_RenderUTF8_Blended_Wrapped(font, text, shadow_col, wrap_w);
    if (!shadow) return NULL;

    SDL_Surface *front = TTF_RenderUTF8_Blended_Wrapped(font, text, col, wrap_w);
    if (!front) {
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(
        0, front->w + shadow_offset, front->h + shadow_offset, 32, SDL_PIXELFORMAT_RGBA32
    );
    if (!out) {
        SDL_FreeSurface(front);
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_FillRect(out, NULL, SDL_MapRGBA(out->format, 0, 0, 0, 0));

    SDL_Rect dst;
    dst.x = shadow_offset;
    dst.y = shadow_offset;
    dst.w = shadow->w;
    dst.h = shadow->h;

    SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceAlphaMod(shadow, TXT_SHADOW_ALPHA);
    SDL_BlitSurface(shadow, NULL, out, &dst);

    dst.x = 0;
    dst.y = 0;
    dst.w = front->w;
    dst.h = front->h;

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(front, NULL, out, &dst);

    SDL_FreeSurface(front);
    SDL_FreeSurface(shadow);

    return out;
}

SDL_Texture *sdl_render_text(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, const SDL_Color col, const int shadow_offset, int *out_w,
    int *out_h
) {
    SDL_Surface *surf = sdl_render_text_surface(font, text, col, shadow_offset);
    if (!surf) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, surf);
    *out_w = surf->w;
    *out_h = surf->h;

    SDL_FreeSurface(surf);

    return t;
}

SDL_Texture *sdl_render_text_wrapped(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, const SDL_Color col, const int wrap_w,
    const int shadow_offset, int *out_w, int *out_h
) {
    SDL_Surface *surf = sdl_render_text_wrapped_surface(font, text, col, wrap_w, shadow_offset);
    if (!surf) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, surf);
    *out_w = surf->w;
    *out_h = surf->h;

    SDL_FreeSurface(surf);

    return t;
}
