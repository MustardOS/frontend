#include "scope.h"
#include "shape.h"
#include "voices.h"
#include "../video/pixfont.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCOPE_WINDOW 160

static float peaks[WASABI_VOICE_MAX];

static int trigger_start(const float *samples, const int count) {
    for (int index = 1; index < count - SCOPE_WINDOW; index++) {
        if (samples[index - 1] < 0.0f && samples[index] >= 0.0f) return index;
    }
    return count - SCOPE_WINDOW;
}

static void draw_cell(
    SDL_Renderer *renderer, const wasabi_visualiser_frame *frame, const SDL_FRect *cell, const char *name,
    const float *samples, const int count, float *peak, const int highlighted, const int dimmed
) {
    const int alpha = dimmed ? 70 : 220;
    vis_round_rect(renderer, cell->x, cell->y, cell->w, cell->h, cell->h * 0.08f, (SDL_Color) {0, 0, 0, 90});
    if (highlighted) {
        const float border = cell->h * 0.03f > 2.0f ? cell->h * 0.03f : 2.0f;
        const SDL_Color edge = vis_alpha(frame->primary, 255);
        vis_line(renderer, cell->x, cell->y, cell->x + cell->w, cell->y, border, edge);
        vis_line(renderer, cell->x, cell->y + cell->h, cell->x + cell->w, cell->y + cell->h, border, edge);
        vis_line(renderer, cell->x, cell->y, cell->x, cell->y + cell->h, border, edge);
        vis_line(renderer, cell->x + cell->w, cell->y, cell->x + cell->w, cell->y + cell->h, border, edge);
    }

    const int scale = frame->height / 240 > 1 ? frame->height / 240 : 1;
    const float pad = (float) (scale * 4);
    char label[WASABI_VOICE_NAME];
    snprintf(label, sizeof(label), "%s", name);
    const SDL_Color text = vis_alpha(frame->secondary, alpha);
    SDL_SetRenderDrawColor(renderer, text.r, text.g, text.b, text.a);
    pixfont_draw(renderer, label, (int) (cell->x + pad), (int) (cell->y + pad), scale);

    float loudest = 0.0f;
    for (int index = 0; index < count; index++) {
        const float magnitude = fabsf(samples[index]);
        if (magnitude > loudest) loudest = magnitude;
    }
    *peak = loudest > *peak ? loudest : *peak * 0.92f + loudest * 0.08f;
    const float gain = *peak > 0.002f ? 0.9f / *peak : 0.0f;

    const int start = trigger_start(samples, count);
    const float middle = cell->y + cell->h * 0.58f;
    const float amplitude = cell->h * 0.36f;
    SDL_FPoint points[SCOPE_WINDOW];
    for (int index = 0; index < SCOPE_WINDOW; index++) {
        float value = samples[start + index] * gain;
        if (value > 1.0f) value = 1.0f;
        if (value < -1.0f) value = -1.0f;
        points[index].x = cell->x + pad + (cell->w - pad * 2.0f) * (float) index / (float) (SCOPE_WINDOW - 1);
        points[index].y = middle - value * amplitude;
    }
    const SDL_Color wave = vis_alpha(frame->primary, alpha);
    SDL_SetRenderDrawColor(renderer, wave.r, wave.g, wave.b, wave.a);
    SDL_RenderDrawLinesF(renderer, points, SCOPE_WINDOW);
}

void wasabi_scope_render(SDL_Renderer *renderer, const wasabi_visualiser_frame *frame) {
    static wasabi_voice_snapshot snapshot;
    const int count = wasabi_voices_snapshot(&snapshot);

    const float margin_x = (float) frame->width * 0.05f;
    const float top = (float) frame->height * 0.12f;
    const float bottom = (float) frame->height * 0.9f;
    const float gap = (float) frame->height * 0.015f;

    if (count < 1) {
        const SDL_FRect cell = {margin_x, top, (float) frame->width - margin_x * 2.0f, bottom - top};
        draw_cell(renderer, frame, &cell, "MIX", frame->mono, WASABI_VISUALISER_SAMPLES, &peaks[0], 0, 0);
        return;
    }

    const int columns = count > 4 ? 2 : 1;
    const int rows = (count + columns - 1) / columns;
    const float cell_width = ((float) frame->width - margin_x * 2.0f - gap * (float) (columns - 1)) / (float) columns;
    const float cell_height = (bottom - top - gap * (float) (rows - 1)) / (float) rows;
    for (int voice = 0; voice < count; voice++) {
        const int column = voice / rows;
        const int row = voice % rows;
        const SDL_FRect cell = {
            margin_x + (cell_width + gap) * (float) column, top + (cell_height + gap) * (float) row, cell_width,
            cell_height
        };
        const int soloed = snapshot.selected == voice + 1;
        draw_cell(
            renderer, frame, &cell, snapshot.names[voice], snapshot.samples[voice], WASABI_VOICE_SAMPLES, &peaks[voice],
            soloed, snapshot.selected > 0 && !soloed
        );
    }
}

void wasabi_scope_reset(void) {
    memset(peaks, 0, sizeof(peaks));
}
