#include "crt.h"

#define CRT_OFF_SQUASH_MS 220
#define CRT_OFF_SHRINK_MS 160
#define CRT_OFF_FADE_MS   280
#define CRT_OFF_MS        (CRT_OFF_SQUASH_MS + CRT_OFF_SHRINK_MS + CRT_OFF_FADE_MS)

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

void crt_effect_render_power_off(
    SDL_Renderer *renderer, const SDL_Rect *frame, const uint32_t started, const uint32_t now,
    const crt_effect_draw_fn draw, void *userdata
) {
    if (!renderer || !frame || !started) return;

    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const uint32_t elapsed = now - started;
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
