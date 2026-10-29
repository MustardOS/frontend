#pragma once

#include <stdint.h>
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
    unsigned last_idle_changes;

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

void saver_init_base(
    saver_state_t *s, int screen_w, int screen_h, const char *log_tag, uint8_t default_r, uint8_t default_g,
    uint8_t default_b, saver_speed_changed_cb on_speed_changed, saver_idle_enter_cb on_idle_enter, void *user
);

int saver_poll_idle(saver_state_t *s, uint32_t sp_now);

int saver_active_base(const saver_state_t *s);

void saver_stop_base(saver_state_t *s);

void saver_shutdown_base(saver_state_t *s);

void saver_pastel_pick(int idx, uint8_t *r, uint8_t *g, uint8_t *b);

void saver_perf_note_draw_calls(unsigned count);

int saver_image_file_allowed(const char *path);

int saver_image_dimensions_allowed(int width, int height);

typedef struct {
    uint8_t *rgb;
    int w;
    int h;
} saver_snapshot_t;

typedef struct {
    SDL_Texture *texture;
    int width;
    int height;
    int screen_width;
    int screen_height;
} saver_message_t;

int saver_message_init(
    saver_message_t *message, SDL_Renderer *renderer, int screen_width, int screen_height, const char *text
);

void saver_message_render(const saver_message_t *message, SDL_Renderer *renderer);

void saver_message_destroy(saver_message_t *message);

int saver_snapshot_take(saver_snapshot_t *snapshot);

void saver_snapshot_free(saver_snapshot_t *snapshot);

SDL_Texture *saver_snapshot_texture(SDL_Renderer *renderer, const saver_snapshot_t *snapshot);

int saver_snapshot_luma(const saver_snapshot_t *snapshot, int x0, int y0, int x1, int y1);

int saver_speed_scale_q8(const saver_state_t *s);

void saver_hsv(float hue, float sat, float val, uint8_t *out);

#define SAVER_COLOUR_COUNT 10

int saver_option(int value, int count, int fallback);

int saver_option_scale(int value, int index);

void saver_apply_colour(saver_state_t *s, int index);
