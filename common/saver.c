#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "common.h"
#include "saver.h"

const uint8_t saver_pastel_r[SAVER_PASTEL_COUNT] = {
        255, 255, 255, 255, 178, 153, 179, 204, 255, 255, 153
};

const uint8_t saver_pastel_g[SAVER_PASTEL_COUNT] = {
        255, 182, 179, 255, 255, 255, 204, 153, 153, 218, 255
};

const uint8_t saver_pastel_b[SAVER_PASTEL_COUNT] = {
        255, 193, 128, 153, 178, 255, 255, 255, 204, 153, 220
};

#define SAVER_SPEED_PATH "/opt/muos/config/settings/power/saver_speed"

static int saver_speed_override = 0;

void saver_seed_rng_once(void) {
    static int seeded = 0;

    if (seeded) return;

    srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
    seeded = 1;
}

int32_t saver_rand_range(int32_t range) {
    if (range <= 0) return 0;

    return (int32_t) (random() % (uint32_t) range);
}

uint8_t saver_clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;

    return (uint8_t) v;
}

int saver_int_abs(int v) {
    return v < 0 ? -v : v;
}

void saver_set_speed_override(int speed) {
    saver_speed_override = speed > 0 ? speed : 0;
}

void saver_clear_speed_override(void) {
    saver_speed_override = 0;
}

int saver_read_speed(void) {
    int speed = saver_speed_override;

    if (speed <= 0 && file_exist(SAVER_SPEED_PATH)) {
        speed = read_line_int_from(SAVER_SPEED_PATH, SAVER_SPEED_DEFAULT);
    }

    if (speed <= 0) speed = SAVER_SPEED_DEFAULT;

    return speed;
}

void saver_pastel_pick(int idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = idx % SAVER_PASTEL_COUNT;

    if (i < 0) i += SAVER_PASTEL_COUNT;

    *r = saver_pastel_r[i];
    *g = saver_pastel_g[i];
    *b = saver_pastel_b[i];
}

void saver_init_base(saver_state_t *s, int screen_w, int screen_h, const char *log_tag, uint8_t default_r, uint8_t default_g, uint8_t default_b,
                     saver_speed_changed_cb on_speed_changed, saver_idle_enter_cb on_idle_enter, void *user) {
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
    s->last_idle_poll = 0;
    s->suppress_until = 0;

    s->speed = saver_read_speed();
}

int saver_poll_idle(saver_state_t *s, uint32_t now) {
    if (!s->enabled) return 0;

    if (now < s->suppress_until) {
        s->idle_active = 0;
        s->was_idle_active = 0;
        s->last_tick = now;
        s->last_idle_poll = now;
        return 0;
    }

    if (now - s->last_idle_poll >= TIMER_IDLE) {
        s->was_idle_active = s->idle_active;
        s->idle_active = read_line_int_from(IDLE_STATE, 1);
        s->last_idle_poll = now;

        if (s->idle_active && !s->was_idle_active) {
            int new_speed = saver_read_speed();

            if (new_speed != s->speed) {
                s->speed = new_speed;

                if (s->on_speed_changed) s->on_speed_changed(s->user);

                LOG_INFO("saver", "%s Speed Refreshed: %d",
                         s->log_tag ? s->log_tag : "Saver", s->speed);
            }

            if (s->on_idle_enter) s->on_idle_enter(s->user);

            s->last_tick = now;
        }
    }

    if (!s->idle_active) {
        s->last_tick = now;
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
    s->enabled = 0;
    s->suppress_until = 0;

    LOG_INFO("saver", "%s Shutdown", s->log_tag ? s->log_tag : "Saver");
}
