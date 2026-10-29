#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <common/runtime/log.h>
#include <common/config/config.h>
#include <common/saver/saver.h>
#include <common/saver/lavalamp.h>

#define LAVA_BLOBS       12
#define LAVA_COLOURS     3
#define LAVA_SHADES      3
#define LAVA_MAX_CELLS   12000
#define LAVA_EVENT_EVERY 35000
#define LAVA_GATHER_MS   6000

typedef struct {
    float x, y;
    float vx, vy;
    float heat;
    float heat_rate;
    float radius;
    int colour;
} lava_blob_t;

typedef struct {
    saver_state_t base;
    lava_blob_t blob[LAVA_BLOBS];
    int blobs;
    uint8_t palette[LAVA_COLOURS][3];
    uint8_t glow[3];
    int cell;
    int cols;
    int rows;
    SDL_Rect *rects[LAVA_COLOURS][LAVA_SHADES];
    int rect_count[LAVA_COLOURS][LAVA_SHADES];
    uint32_t event_due;
    uint32_t gather_until;
    float gather_x, gather_y;
} lava_module_t;

static lava_module_t mod = {0};

static float frand(const float lo, const float hi) {
    return lo + (hi - lo) * (float) saver_rand_range(10000) / 10000.0f;
}

static void seed_blobs(void) {
    const float w = (float) mod.base.screen_w;
    const float h = (float) mod.base.screen_h;
    const float base = (w < h ? w : h) / 9.0f;

    static const float hues[] = {0.0f, 0.1f, 0.55f, 0.27f, 0.85f};
    const int colour = saver_option(config.settings.saver.lavalamp_colour, 5, 0);
    const float hue = colour ? hues[colour] : frand(0.0f, 1.0f);
    for (int c = 0; c < LAVA_COLOURS; c++) {
        const float shift = (colour ? frand(0.02f, 0.06f) : frand(0.08f, 0.33f)) * (float) c;
        float tone = hue + shift;
        if (tone >= 1.0f) tone -= 1.0f;
        saver_hsv(tone, frand(0.6f, 0.95f), frand(0.75f, 0.95f), mod.palette[c]);
    }
    float glow = hue + 0.5f;
    if (glow >= 1.0f) glow -= 1.0f;
    saver_hsv(glow, 0.6f, 0.18f, mod.glow);

    static const int counts[] = {0, 6, 9, 12};
    const int count = saver_option(config.settings.saver.lavalamp_count, 4, 0);
    mod.blobs = count ? counts[count] : 5 + (int) saver_rand_range(5);
    for (int i = 0; i < mod.blobs; i++) {
        lava_blob_t *b = &mod.blob[i];
        b->x = frand(w * 0.15f, w * 0.85f);
        b->y = frand(h * 0.1f, h * 0.9f);
        b->vx = frand(-15.0f, 15.0f);
        b->vy = frand(-25.0f, 25.0f);
        b->heat = frand(0.0f, 1.0f);
        b->heat_rate = frand(0.12f, 0.45f);
        b->radius = base * frand(0.5f, 1.6f);
        b->colour = (int) saver_rand_range(LAVA_COLOURS);
    }

    mod.event_due = SDL_GetTicks() + LAVA_EVENT_EVERY / 2 + (uint32_t) saver_rand_range(LAVA_EVENT_EVERY);
    mod.gather_until = 0;
}

static void on_idle_enter(void *user) {
    (void) user;
    seed_blobs();
}

static void free_rects(void) {
    for (int c = 0; c < LAVA_COLOURS; c++) {
        for (int s = 0; s < LAVA_SHADES; s++) {
            free(mod.rects[c][s]);
            mod.rects[c][s] = NULL;
        }
    }
}

int lavalamp_init(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    (void) renderer;

    saver_init_base(&mod.base, screen_w, screen_h, "Lava Lamp", 217, 164, 14, NULL, on_idle_enter, &mod);

    mod.cell = screen_w / 80;
    if (mod.cell < 6) mod.cell = 6;
    mod.cols = (screen_w + mod.cell - 1) / mod.cell;
    mod.rows = (screen_h + mod.cell - 1) / mod.cell;
    while (mod.cols * mod.rows > LAVA_MAX_CELLS) {
        mod.cell++;
        mod.cols = (screen_w + mod.cell - 1) / mod.cell;
        mod.rows = (screen_h + mod.cell - 1) / mod.cell;
    }

    free_rects();
    for (int c = 0; c < LAVA_COLOURS; c++) {
        for (int s = 0; s < LAVA_SHADES; s++) {
            mod.rects[c][s] = malloc(sizeof(SDL_Rect) * (size_t) (mod.cols * mod.rows));
            if (!mod.rects[c][s]) {
                free_rects();
                return 0;
            }
        }
    }

    seed_blobs();
    LOG_INFO("saver", "Lava Lamp Initialised (%dx%d, cell=%d)", screen_w, screen_h, mod.cell);
    return 1;
}

void lavalamp_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    const float dt =
        (float) (elapsed > 100 ? 100 : elapsed) / 1000.0f * (float) saver_speed_scale_q8(&mod.base) / 256.0f;
    const float w = (float) mod.base.screen_w;
    const float h = (float) mod.base.screen_h;

    if (!mod.gather_until && now >= mod.event_due) {
        if (saver_rand_range(2)) {
            mod.gather_x = frand(w * 0.25f, w * 0.75f);
            mod.gather_y = frand(h * 0.25f, h * 0.75f);
            mod.gather_until = now + LAVA_GATHER_MS;
        } else {
            const float gust = frand(-60.0f, 60.0f);
            for (int i = 0; i < mod.blobs; i++)
                mod.blob[i].vx += gust * frand(0.5f, 1.5f);
        }
        mod.event_due = now + LAVA_EVENT_EVERY / 2 + (uint32_t) saver_rand_range(LAVA_EVENT_EVERY);
    } else if (mod.gather_until && now >= mod.gather_until) {
        mod.gather_until = 0;
        for (int i = 0; i < mod.blobs; i++) {
            mod.blob[i].vx = frand(-45.0f, 45.0f);
            mod.blob[i].vy = frand(-45.0f, 45.0f);
        }
    }

    for (int i = 0; i < mod.blobs; i++) {
        lava_blob_t *b = &mod.blob[i];

        if (mod.gather_until) {
            b->vx += ((mod.gather_x - b->x) * 0.9f - b->vx * 1.5f) * dt;
            b->vy += ((mod.gather_y - b->y) * 0.9f - b->vy * 1.5f) * dt;
        } else {
            const float depth = b->y / h;
            if (depth > 0.8f) b->heat += b->heat_rate * dt;
            if (depth < 0.2f) b->heat -= b->heat_rate * dt;
            if (b->heat < 0.0f) b->heat = 0.0f;
            if (b->heat > 1.0f) b->heat = 1.0f;

            b->vy += (0.5f - b->heat) * 60.0f * dt;
            b->vx += frand(-8.0f, 8.0f) * dt;
            b->vx *= 1.0f - 0.3f * dt;
            b->vy *= 1.0f - 0.2f * dt;

            if (b->vy > 45.0f) b->vy = 45.0f;
            if (b->vy < -45.0f) b->vy = -45.0f;
            if (b->vx > 25.0f) b->vx = 25.0f;
            if (b->vx < -25.0f) b->vx = -25.0f;
        }

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        const float margin = b->radius * 0.5f;
        if (b->x < margin) {
            b->x = margin;
            b->vx = -b->vx * 0.5f;
        } else if (b->x > w - margin) {
            b->x = w - margin;
            b->vx = -b->vx * 0.5f;
        }
        if (b->y < margin) {
            b->y = margin;
            b->vy = -b->vy * 0.5f;
        } else if (b->y > h - margin) {
            b->y = h - margin;
            b->vy = -b->vy * 0.5f;
        }
    }
}

void lavalamp_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.rects[0][0]) return;

    const int bands = 8;
    for (int i = 0; i < bands; i++) {
        const int glow = 10 + i * 4;
        SDL_SetRenderDrawColor(
            renderer, saver_clamp_u8(mod.glow[0] + glow - 10), saver_clamp_u8(mod.glow[1] + glow - 10),
            saver_clamp_u8(mod.glow[2] + glow - 10), 255
        );
        SDL_Rect band = {0, mod.base.screen_h * i / bands, mod.base.screen_w, mod.base.screen_h / bands + 1};
        SDL_RenderFillRect(renderer, &band);
    }

    for (int c = 0; c < LAVA_COLOURS; c++)
        for (int s = 0; s < LAVA_SHADES; s++)
            mod.rect_count[c][s] = 0;

    float rsq[LAVA_BLOBS];
    for (int i = 0; i < mod.blobs; i++)
        rsq[i] = mod.blob[i].radius * mod.blob[i].radius;

    const float half = (float) mod.cell * 0.5f;
    for (int row = 0; row < mod.rows; row++) {
        const float py = (float) (row * mod.cell) + half;
        for (int col = 0; col < mod.cols; col++) {
            const float px = (float) (col * mod.cell) + half;
            float field = 0.0f;
            float best = 0.0f;
            int owner = 0;
            for (int i = 0; i < mod.blobs; i++) {
                const float dx = px - mod.blob[i].x;
                const float dy = py - mod.blob[i].y;
                const float contribution = rsq[i] / (dx * dx + dy * dy + 1.0f);
                field += contribution;
                if (contribution > best) {
                    best = contribution;
                    owner = mod.blob[i].colour;
                }
            }
            if (field < 1.0f) continue;

            const int shade = field > 2.4f ? 2 : field > 1.4f ? 1 : 0;
            SDL_Rect *rect = &mod.rects[owner][shade][mod.rect_count[owner][shade]++];
            rect->x = col * mod.cell;
            rect->y = row * mod.cell;
            rect->w = mod.cell;
            rect->h = mod.cell;
        }
    }

    unsigned draw_calls = (unsigned) bands;
    for (int c = 0; c < LAVA_COLOURS; c++) {
        for (int s = 0; s < LAVA_SHADES; s++) {
            if (!mod.rect_count[c][s]) continue;
            const int lift = s == 2 ? 40 : s == 1 ? 15 : -25;
            SDL_SetRenderDrawColor(
                renderer, saver_clamp_u8(mod.palette[c][0] + lift), saver_clamp_u8(mod.palette[c][1] + lift),
                saver_clamp_u8(mod.palette[c][2] + lift), 255
            );
            SDL_RenderFillRects(renderer, mod.rects[c][s], mod.rect_count[c][s]);
            draw_calls++;
        }
    }

    saver_perf_note_draw_calls(draw_calls);
}

int lavalamp_active(void) {
    return saver_active_base(&mod.base);
}

void lavalamp_stop(void) {
    saver_stop_base(&mod.base);
}

void lavalamp_shutdown(void) {
    free_rects();
    saver_shutdown_base(&mod.base);
}
