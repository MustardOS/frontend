#include "crt.h"

#include <math.h>

#define CRT_OFF_SQUASH_MS 175
#define CRT_OFF_SHRINK_MS 125
#define CRT_OFF_FADE_MS   220
#define CRT_OFF_MS        (CRT_OFF_SQUASH_MS + CRT_OFF_SHRINK_MS + CRT_OFF_FADE_MS)
#define CRT_DEGAUSS_MS    1600
#define CRT_DEGAUSS_BANDS 32

static float ease_out(const float value) {
    const float clamped = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    const float inverse = 1.0f - clamped;
    return 1.0f - inverse * inverse * inverse;
}

static float ease_in(const float value) {
    const float clamped = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    return clamped * clamped;
}

static void fill_rect_alpha(SDL_Renderer *renderer, const SDL_Rect *rect, const Uint8 level, const float alpha) {
    if (alpha <= 0.0f) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, level, level, level, (Uint8) (alpha > 1.0f ? 255.0f : alpha * 255.0f));
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

uint32_t crt_effect_power_off_duration(void) {
    return CRT_OFF_MS;
}

uint32_t crt_effect_degauss_duration(void) {
    return CRT_DEGAUSS_MS;
}

static void render_at(
    SDL_Renderer *renderer, const SDL_Rect *frame, const uint32_t elapsed, const crt_effect_draw_fn draw, void *userdata
) {
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const int centre_x = frame->w > 0 ? frame->x + frame->w / 2 : width / 2;
    const int centre_y = frame->h > 0 ? frame->y + frame->h / 2 : height / 2;
    const int line = height / 180 > 2 ? height / 180 : 2;

    if (elapsed < CRT_OFF_SQUASH_MS) {
        const float progress = (float) elapsed / CRT_OFF_SQUASH_MS;
        int squashed = (int) ((float) frame->h * (1.0f - ease_in(progress)));
        if (squashed < line) squashed = line;
        const SDL_Rect rect = {frame->x, centre_y - squashed / 2, frame->w, squashed};
        if (draw) draw(renderer, &rect, userdata);
        fill_rect_alpha(renderer, &rect, 255, progress * 0.9f);
        return;
    }

    if (elapsed < CRT_OFF_SQUASH_MS + CRT_OFF_SHRINK_MS) {
        const float progress = (float) (elapsed - CRT_OFF_SQUASH_MS) / CRT_OFF_SHRINK_MS;
        int span = (int) ((float) frame->w * (1.0f - ease_in(progress)));
        if (span < line * 2) span = line * 2;
        const SDL_Rect rect = {centre_x - span / 2, centre_y - line / 2, span, line};
        fill_rect_alpha(renderer, &rect, 255, 1.0f);
        return;
    }

    if (elapsed < CRT_OFF_MS) {
        const float progress = (float) (elapsed - CRT_OFF_SQUASH_MS - CRT_OFF_SHRINK_MS) / CRT_OFF_FADE_MS;
        const int dot = line * 2;
        const int glow = dot * 3;
        const SDL_Rect halo = {centre_x - glow / 2, centre_y - glow / 2, glow, glow};
        const SDL_Rect core = {centre_x - dot / 2, centre_y - dot / 2, dot, dot};
        fill_rect_alpha(renderer, &halo, 255, (1.0f - progress) * 0.25f);
        fill_rect_alpha(renderer, &core, 255, 1.0f - ease_out(progress));
    }
}

void crt_effect_render_power_off(
    SDL_Renderer *renderer, const SDL_Rect *frame, const uint32_t started, const uint32_t now,
    const crt_effect_draw_fn draw, void *userdata
) {
    if (!renderer || !frame || !started) return;
    render_at(renderer, frame, now - started, draw, userdata);
}

void crt_effect_render_power_on(
    SDL_Renderer *renderer, const SDL_Rect *frame, const uint32_t started, const uint32_t now,
    const crt_effect_draw_fn draw, void *userdata
) {
    if (!renderer || !frame || !started) return;

    const uint32_t elapsed = now - started;
    render_at(renderer, frame, elapsed >= CRT_OFF_MS ? 0 : CRT_OFF_MS - elapsed, draw, userdata);
}

void crt_effect_render_degauss(
    SDL_Renderer *renderer, const SDL_Rect *frame, const uint32_t started, const uint32_t now, SDL_Texture *texture
) {
    if (!renderer || !frame || !started || !texture || frame->w <= 0 || frame->h <= 0) return;

    const uint32_t elapsed = now - started;
    if (elapsed >= CRT_DEGAUSS_MS) {
        SDL_RenderCopy(renderer, texture, NULL, frame);
        return;
    }

    const float progress = (float) elapsed / CRT_DEGAUSS_MS;
    const float remaining = 1.0f - progress;
    const float settle = remaining * remaining * (3.0f - 2.0f * remaining);
    const float shock = expf(-progress * 11.0f);
    const float smooth_amplitude = (float) frame->w * 0.038f * settle;
    const float rough_amplitude = (float) frame->w * 0.04f * shock;
    const float colour_amplitude = (float) frame->w * (0.012f * settle + 0.022f * shock);
    const int expansion = (int) ((float) frame->w * (0.018f * settle + 0.018f * shock));
    const int vertical =
        (int) ((float) frame->h
               * (0.008f * settle * sinf(progress * 22.0f) + 0.018f * shock * sinf(progress * 95.0f + 0.8f)));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip = {0};
    if (clipped) SDL_RenderGetClipRect(renderer, &previous_clip);

    SDL_BlendMode previous_blend = SDL_BLENDMODE_NONE;
    Uint8 previous_red = 255;
    Uint8 previous_green = 255;
    Uint8 previous_blue = 255;
    Uint8 previous_alpha = 255;
    SDL_GetTextureBlendMode(texture, &previous_blend);
    SDL_GetTextureColorMod(texture, &previous_red, &previous_green, &previous_blue);
    SDL_GetTextureAlphaMod(texture, &previous_alpha);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
    SDL_SetTextureAlphaMod(texture, 255);

    static const Uint8 colour[3][3] = {{0, 255, 0}, {255, 0, 0}, {0, 0, 255}};
    static const int direction[3] = {0, 1, -1};
    for (int channel = 0; channel < 3; channel++) {
        SDL_SetTextureColorMod(texture, colour[channel][0], colour[channel][1], colour[channel][2]);

        for (int band = 0; band < CRT_DEGAUSS_BANDS; band++) {
            SDL_Rect clip = {
                frame->x,
                frame->y + frame->h * band / CRT_DEGAUSS_BANDS,
                frame->w,
                frame->h * (band + 1) / CRT_DEGAUSS_BANDS - frame->h * band / CRT_DEGAUSS_BANDS,
            };
            if (clipped) {
                SDL_Rect intersection;
                if (!SDL_IntersectRect(&clip, &previous_clip, &intersection)) continue;
                clip = intersection;
            }
            SDL_RenderSetClipRect(renderer, &clip);

            const float position = ((float) band + 0.5f) / CRT_DEGAUSS_BANDS;
            const float phase = progress * 24.0f + position * 9.0f;
            const float rough_phase = progress * 95.0f + position * 31.0f;
            const int wobble =
                (int) (smooth_amplitude * (sinf(phase) + 0.22f * sinf(phase * 0.49f + 1.7f))
                       + rough_amplitude * (sinf(rough_phase) + 0.45f * sinf(rough_phase * 1.73f + position * 7.0f)));
            const float colour_wave =
                0.62f + 0.23f * sinf(phase * 0.61f + position * 4.0f) + 0.15f * shock * sinf(rough_phase);
            const int colour_shift = (int) (colour_amplitude * colour_wave) * direction[channel];
            const SDL_Rect warped = {
                frame->x + wobble + colour_shift - expansion / 2,
                frame->y + vertical,
                frame->w + expansion,
                frame->h,
            };
            SDL_RenderCopy(renderer, texture, NULL, &warped);
        }
    }

    SDL_RenderSetClipRect(renderer, clipped ? &previous_clip : NULL);
    SDL_SetTextureColorMod(texture, previous_red, previous_green, previous_blue);
    SDL_SetTextureAlphaMod(texture, previous_alpha);
    SDL_SetTextureBlendMode(texture, previous_blend);
    fill_rect_alpha(renderer, frame, 255, settle * 0.04f + shock * 0.12f);
}
