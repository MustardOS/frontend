#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include <common/runtime/log.h>
#include <common/storage/inotify.h>
#include <common/runtime/init.h>
#include <common/runtime/perf.h>
#include <common/base/options.h>
#include <common/storage/fileio.h>
#include <common/saver/saver.h>
#include <common/display/screenshot.h>

const uint8_t saver_pastel_r[SAVER_PASTEL_COUNT] = {255, 255, 255, 255, 178, 153, 179, 204, 255, 255, 153};

const uint8_t saver_pastel_g[SAVER_PASTEL_COUNT] = {255, 182, 179, 255, 255, 255, 204, 153, 153, 218, 255};

const uint8_t saver_pastel_b[SAVER_PASTEL_COUNT] = {255, 193, 128, 153, 178, 255, 255, 255, 204, 153, 220};

#define SAVER_SPEED_PATH "/opt/muos/config/settings/power/saver_speed"

static int saver_speed_override = 0;

void saver_seed_rng_once(void) {
    static int seeded = 0;

    if (seeded) return;

    srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
    seeded = 1;
}

int32_t saver_rand_range(const int32_t range) {
    if (range <= 0) return 0;

    return (int32_t) (random() % (uint32_t) range);
}

uint8_t saver_clamp_u8(const int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;

    return (uint8_t) v;
}

int saver_int_abs(const int v) {
    return v < 0 ? -v : v;
}

void saver_set_speed_override(const int speed) {
    saver_speed_override = speed > 0 ? speed : 0;
}

void saver_clear_speed_override(void) {
    saver_speed_override = 0;
}

int saver_read_speed(void) {
    int speed = saver_speed_override;

    if (speed <= 0 && file_exist(SAVER_SPEED_PATH)) {
        speed = read_line_int_from(SAVER_SPEED_PATH, 1);
    }

    if (speed <= 0) speed = SAVER_SPEED_DEFAULT;

    return speed;
}

void saver_pastel_pick(const int idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = idx % SAVER_PASTEL_COUNT;

    if (i < 0) i += SAVER_PASTEL_COUNT;

    *r = saver_pastel_r[i];
    *g = saver_pastel_g[i];
    *b = saver_pastel_b[i];
}

void saver_perf_note_draw_calls(const unsigned count) {
    fe_perf_note_saver_draw_calls(count);
}

int saver_image_file_allowed(const char *path) {
    enum { max_image_file_bytes = 32 * 1024 * 1024 };
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size <= max_image_file_bytes;
}

int saver_image_dimensions_allowed(const int width, const int height) {
    enum { max_dimension = 8192, max_pixels = 16 * 1024 * 1024 };
    return width > 0 && height > 0 && width <= max_dimension && height <= max_dimension
           && (int64_t) width * (int64_t) height <= max_pixels;
}

void saver_init_base(
    saver_state_t *s, const int screen_w, const int screen_h, const char *log_tag, const uint8_t default_r,
    const uint8_t default_g, const uint8_t default_b, const saver_speed_changed_cb on_speed_changed,
    const saver_idle_enter_cb on_idle_enter, void *user
) {
    saver_seed_rng_once();

    s->screen_w = screen_w;
    s->screen_h = screen_h;

    s->log_tag = log_tag;
    s->colour_r = default_r;
    s->colour_g = default_g;
    s->colour_b = default_b;

    s->on_speed_changed = on_speed_changed;
    s->on_idle_enter = on_idle_enter;
    s->user = user;

    s->enabled = 1;
    s->idle_active = 0;
    s->was_idle_active = 0;

    s->last_tick = SDL_GetTicks();

    s->last_idle_poll = SDL_GetTicks() - TIMER_IDLE;
    s->last_idle_changes = idle_state_changes - 1;

    s->suppress_until = 0;

    s->speed = saver_read_speed();
}

int saver_poll_idle(saver_state_t *s, const uint32_t sp_now) {
    if (!s->enabled) return 0;

    if (sp_now < s->suppress_until) {
        s->idle_active = 0;
        s->was_idle_active = 0;
        s->last_tick = sp_now;
        s->last_idle_poll = sp_now;
        return 0;
    }

    int should_poll;
    if (ino_proc) {
        should_poll = idle_state_changes != s->last_idle_changes;
        if (should_poll) s->last_idle_changes = idle_state_changes;
    } else {
        should_poll = sp_now - s->last_idle_poll >= TIMER_IDLE;
    }

    if (should_poll) {
        s->was_idle_active = s->idle_active;
        s->idle_active = read_line_int_from(IDLE_STATE, 1);
        s->last_idle_poll = sp_now;

        if (s->idle_active && !s->was_idle_active) {
            const int new_speed = saver_read_speed();

            if (new_speed != s->speed) {
                s->speed = new_speed;

                if (s->on_speed_changed) s->on_speed_changed(s->user);

                LOG_INFO("saver", "%s Speed Refreshed: %d", s->log_tag ? s->log_tag : "Saver", s->speed);
            }

            if (s->on_idle_enter) s->on_idle_enter(s->user);

            s->last_tick = sp_now;
        }
    }

    if (!s->idle_active) {
        s->last_tick = sp_now;
        return 0;
    }

    return 1;
}

int saver_active_base(const saver_state_t *s) {
    if (s->suppress_until && SDL_GetTicks() < s->suppress_until) return 0;

    return s->enabled && s->idle_active;
}

void saver_stop_base(saver_state_t *s) {
    s->idle_active = 0;
    s->was_idle_active = 0;
    s->suppress_until = SDL_GetTicks() + SAVER_DELAY;
}

void saver_shutdown_base(saver_state_t *s) {
    if (!s->enabled) return;

    s->enabled = 0;
    s->suppress_until = 0;

    LOG_INFO("saver", "%s Shutdown", s->log_tag ? s->log_tag : "Saver");
}

int saver_snapshot_take(saver_snapshot_t *snapshot) {
    if (!snapshot) return 0;
    saver_snapshot_free(snapshot);

    uint8_t *rgb = NULL;
    uint32_t w = 0;
    uint32_t h = 0;
    if (screenshot_grab(screenshot_auto, &rgb, &w, &h) != 0 || !rgb || !w || !h) {
        free(rgb);
        return 0;
    }

    snapshot->rgb = rgb;
    snapshot->w = (int) w;
    snapshot->h = (int) h;
    return 1;
}

void saver_snapshot_free(saver_snapshot_t *snapshot) {
    if (!snapshot) return;
    free(snapshot->rgb);
    snapshot->rgb = NULL;
    snapshot->w = 0;
    snapshot->h = 0;
}

SDL_Texture *saver_snapshot_texture(SDL_Renderer *renderer, const saver_snapshot_t *snapshot) {
    if (!renderer || !snapshot || !snapshot->rgb) return NULL;

    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, snapshot->w, snapshot->h);
    if (!texture) return NULL;

    if (SDL_UpdateTexture(texture, NULL, snapshot->rgb, snapshot->w * 3) != 0) {
        SDL_DestroyTexture(texture);
        return NULL;
    }
    return texture;
}

int saver_snapshot_luma(const saver_snapshot_t *snapshot, int x0, int y0, int x1, int y1) {
    if (!snapshot || !snapshot->rgb) return 0;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > snapshot->w) x1 = snapshot->w;
    if (y1 > snapshot->h) y1 = snapshot->h;
    if (x1 <= x0 || y1 <= y0) return 0;

    const int step_x = (x1 - x0) / 4 > 0 ? (x1 - x0) / 4 : 1;
    const int step_y = (y1 - y0) / 4 > 0 ? (y1 - y0) / 4 : 1;
    int total = 0;
    int count = 0;
    for (int y = y0; y < y1; y += step_y) {
        const uint8_t *row = snapshot->rgb + ((size_t) y * snapshot->w) * 3;
        for (int x = x0; x < x1; x += step_x) {
            const uint8_t *px = row + (size_t) x * 3;
            total += (px[0] * 54 + px[1] * 183 + px[2] * 19) >> 8;
            count++;
        }
    }
    return count ? total / count : 0;
}

int saver_speed_scale_q8(const saver_state_t *s) {
    const int speed = s && s->speed > 0 ? s->speed : SAVER_SPEED_DEFAULT;
    return speed * 256 / SAVER_SPEED_DEFAULT;
}

void saver_hsv(const float hue, const float sat, const float val, uint8_t *out) {
    const float h = hue * 6.0f;
    const int sector = (int) h % 6;
    const float f = h - (float) (int) h;
    const float p = val * (1.0f - sat);
    const float q = val * (1.0f - sat * f);
    const float t = val * (1.0f - sat * (1.0f - f));
    float r, g, b;
    switch (sector) {
        case 0:
            r = val, g = t, b = p;
            break;
        case 1:
            r = q, g = val, b = p;
            break;
        case 2:
            r = p, g = val, b = t;
            break;
        case 3:
            r = p, g = q, b = val;
            break;
        case 4:
            r = t, g = p, b = val;
            break;
        default:
            r = val, g = p, b = q;
            break;
    }
    out[0] = saver_clamp_u8((int) (r * 255.0f));
    out[1] = saver_clamp_u8((int) (g * 255.0f));
    out[2] = saver_clamp_u8((int) (b * 255.0f));
}
