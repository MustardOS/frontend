#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define SAVER_FRAME_SHF 16
#define SAVER_FRAME_ONE (1 << SAVER_FRAME_SHF)

#define SAVER_SPEED_COLOUR_THRESHOLD 600

#define SAVER_SPEED_DEFAULT 90
#define SAVER_PASTEL_COUNT  11

extern const uint8_t saver_pastel_r[SAVER_PASTEL_COUNT];
extern const uint8_t saver_pastel_g[SAVER_PASTEL_COUNT];
extern const uint8_t saver_pastel_b[SAVER_PASTEL_COUNT];

typedef struct saver_state saver_state_t;

typedef void (*saver_speed_changed_cb)(void *user);
typedef void (*saver_idle_enter_cb)(void *user);

struct saver_state {
    int screen_w;
    int screen_h;
    int speed;

    uint8_t colour_r;
    uint8_t colour_g;
    uint8_t colour_b;

    int idle_active;
    int enabled;

    uint32_t last_tick;

    uint32_t last_idle_poll;
    uint32_t suppress_until;
    int was_idle_active;

    const char *log_tag;

    saver_speed_changed_cb on_speed_changed;
    saver_idle_enter_cb on_idle_enter;
    void *user;
};

void saver_seed_rng_once(void);

int32_t saver_rand_range(int32_t range);

uint8_t saver_clamp_u8(int v);

int saver_int_abs(int v);

void saver_set_speed_override(int speed);

void saver_clear_speed_override(void);

int saver_read_speed(void);

void saver_init_base(saver_state_t *s, int screen_w, int screen_h, const char *log_tag, uint8_t default_r, uint8_t default_g, uint8_t default_b,
                     saver_speed_changed_cb on_speed_changed, saver_idle_enter_cb on_idle_enter, void *user);

int saver_poll_idle(saver_state_t *s, uint32_t now);

int saver_active_base(const saver_state_t *s);

void saver_stop_base(saver_state_t *s);

void saver_shutdown_base(saver_state_t *s);

void saver_pastel_pick(int idx, uint8_t *r, uint8_t *g, uint8_t *b);
