#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <common/base/options.h>
#include <common/runtime/log.h>
#include <common/saver/saver.h>
#include <common/saver/hallway.h>
#include <common/saver/boxart.h>

#define HALL_DEPTH        16
#define HALL_SAME_DOOR    37
#define HALL_TURN_SECONDS 1.8f
#define HALL_LEG_MIN      45
#define HALL_LEG_RANGE    50
#define HALL_PIXIE_TRAIL  10
#define HALL_PIXEL        4
#define HALL_SPRITE_SIZE  12
#define HALL_SPRITE_KINDS 7
#define HALL_HALF_HEIGHT  0.75f
#define HALL_LIGHT_EVERY  3
#define HALL_MAX_ROWS     1080
#define HALL_ART_COUNT    6
#define HALL_ART_W        24
#define HALL_ART_H        32

typedef struct {
    uint32_t base;
    float len;
    int turn;
    float phase[2];
} hall_leg_t;

typedef struct {
    saver_state_t base;
    uint32_t seed;
    hall_leg_t leg;
    hall_leg_t next;
    hall_leg_t prev;
    int has_prev;
    float walk_z;
    float heading;
    int turning;
    float turn_progress;
    uint32_t pixie_due;
    uint32_t pixie_start;
    uint32_t pixie_leave;
    float pixie_x, pixie_y;
    float pixie_vx, pixie_vy;
    float pixie_phase;
    float trail[HALL_PIXIE_TRAIL][2];
    int trail_head;
    uint32_t trail_at;
    SDL_Texture *texture;
    uint8_t *pixels;
    float *depth;
    int lw, lh;
    float bob_phase;
    float cam_x;
    float cam_z;
    char **art_paths;
    int art_path_count;
    uint8_t art[HALL_ART_COUNT][HALL_ART_H][HALL_ART_W][3];
    int art_loaded;
    int art_attempts;
} hall_module_t;

static hall_module_t mod = {0};

static uint32_t hash_segment(const uint32_t n) {
    uint32_t h = n * 2654435761u ^ mod.seed;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

static hall_leg_t random_leg(void) {
    hall_leg_t leg;
    leg.base = 64 + (uint32_t) saver_rand_range(4000);
    leg.len = (float) (HALL_LEG_MIN + saver_rand_range(HALL_LEG_RANGE));
    leg.turn = saver_rand_range(2) ? 1 : -1;
    leg.phase[0] = (float) saver_rand_range(6283) / 1000.0f;
    leg.phase[1] = (float) saver_rand_range(6283) / 1000.0f;
    return leg;
}

static void on_idle_enter(void *user) {
    (void) user;
    const uint32_t now = SDL_GetTicks();
    mod.seed = (uint32_t) saver_rand_range(0x7fffffff);
    mod.leg = random_leg();
    mod.next = random_leg();
    mod.has_prev = 0;
    mod.walk_z = 0.0f;
    mod.heading = 0.0f;
    mod.turning = 0;
    mod.turn_progress = 0.0f;
    mod.pixie_due = now + 20000 + (uint32_t) saver_rand_range(40000);
    mod.pixie_start = 0;
    mod.art_loaded = 0;
    mod.art_attempts = 0;
    mod.cam_x = 0.0f;
    mod.cam_z = 0.0f;
}

static int load_art(uint8_t pixels[HALL_ART_H][HALL_ART_W][3], const char *path) {
    SDL_Surface *loaded = IMG_Load(path);
    if (!loaded) return 0;

    SDL_Surface *rgb = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(loaded);
    if (!rgb) return 0;
    if (rgb->w < HALL_ART_W || rgb->h < HALL_ART_H) {
        SDL_FreeSurface(rgb);
        return 0;
    }

    const uint8_t *src = rgb->pixels;
    for (int y = 0; y < HALL_ART_H; y++) {
        const int y0 = rgb->h * y / HALL_ART_H;
        const int y1 = rgb->h * (y + 1) / HALL_ART_H;
        for (int x = 0; x < HALL_ART_W; x++) {
            const int x0 = rgb->w * x / HALL_ART_W;
            const int x1 = rgb->w * (x + 1) / HALL_ART_W;
            const int step_x = (x1 - x0) / 3 > 0 ? (x1 - x0) / 3 : 1;
            const int step_y = (y1 - y0) / 3 > 0 ? (y1 - y0) / 3 : 1;
            int r = 0, g = 0, b = 0, n = 0;
            for (int sy = y0; sy < y1; sy += step_y) {
                for (int sx = x0; sx < x1; sx += step_x) {
                    const uint8_t *px = src + (size_t) sy * rgb->pitch + (size_t) sx * 3;
                    r += px[0];
                    g += px[1];
                    b += px[2];
                    n++;
                }
            }
            pixels[y][x][0] = (uint8_t) (n ? r / n : 0);
            pixels[y][x][1] = (uint8_t) (n ? g / n : 0);
            pixels[y][x][2] = (uint8_t) (n ? b / n : 0);
        }
    }

    SDL_FreeSurface(rgb);
    return 1;
}

static void load_next_art(void) {
    if (!mod.art_path_count || mod.art_loaded >= HALL_ART_COUNT || mod.art_attempts >= HALL_ART_COUNT * 3) return;

    mod.art_attempts++;
    const char *path = mod.art_paths[saver_rand_range(mod.art_path_count)];
    if (load_art(mod.art[mod.art_loaded], path)) mod.art_loaded++;
}

int hallway_init(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    (void) renderer;
    saver_init_base(&mod.base, screen_w, screen_h, "Endless Hallway", 140, 255, 170, NULL, on_idle_enter, &mod);

    if (!mod.art_paths) {
        int cap = 0;
        boxart_collect_images(INFO_CAT_PATH, &mod.art_paths, &mod.art_path_count, &cap);
        LOG_INFO("saver", "Endless Hallway: %d box art images for the picture frames", mod.art_path_count);
    }
    on_idle_enter(NULL);
    LOG_INFO("saver", "Endless Hallway Initialised (%dx%d)", screen_w, screen_h);
    return 1;
}

static void plan_walk(float dt);

void hallway_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    const uint32_t elapsed = now - mod.base.last_tick;
    if (!elapsed) return;
    mod.base.last_tick = now;

    const double dt = (double) (elapsed > 100 ? 100 : elapsed) / 1000.0 * saver_speed_scale_q8(&mod.base) / 256.0;
    mod.bob_phase += (float) (elapsed > 100 ? 100 : elapsed) / 1000.0f * 6.2831853f * 1.7f;
    load_next_art();
    if (mod.bob_phase > 6.2831853f) mod.bob_phase -= 6.2831853f;
    plan_walk((float) dt);

    const float w = (float) mod.base.screen_w;
    const float h = (float) mod.base.screen_h;
    const float fdt = (float) dt;

    if (!mod.pixie_start && now >= mod.pixie_due) {
        mod.pixie_start = now;
        mod.pixie_leave = now + 6000 + (uint32_t) saver_rand_range(4000);
        mod.pixie_x = (float) saver_rand_range((int32_t) w);
        mod.pixie_y = h * (0.2f + (float) saver_rand_range(50) / 100.0f);
        mod.pixie_vx = (float) (saver_rand_range(80) - 40);
        mod.pixie_vy = (float) (saver_rand_range(40) - 20);
        mod.pixie_phase = 0.0f;
        for (int i = 0; i < HALL_PIXIE_TRAIL; i++) {
            mod.trail[i][0] = mod.pixie_x;
            mod.trail[i][1] = mod.pixie_y;
        }
    } else if (mod.pixie_start) {
        mod.pixie_phase += fdt * 3.0f;
        if (now < mod.pixie_leave) {
            mod.pixie_vx += sinf(mod.pixie_phase * 0.7f) * 90.0f * fdt;
            mod.pixie_vy += cosf(mod.pixie_phase * 1.3f) * 70.0f * fdt;
            mod.pixie_vx += (w * 0.5f - mod.pixie_x) * 0.15f * fdt;
            mod.pixie_vy += (h * 0.45f - mod.pixie_y) * 0.15f * fdt;
            mod.pixie_vx *= 1.0f - 0.4f * fdt;
            mod.pixie_vy *= 1.0f - 0.4f * fdt;
        } else {
            const float dir = mod.pixie_x < w * 0.5f ? -1.0f : 1.0f;
            mod.pixie_vx += dir * 600.0f * fdt;
            mod.pixie_vy -= 250.0f * fdt;
        }
        mod.pixie_x += mod.pixie_vx * fdt;
        mod.pixie_y += mod.pixie_vy * fdt;

        if (now - mod.trail_at > 60) {
            mod.trail_at = now;
            mod.trail_head = (mod.trail_head + 1) % HALL_PIXIE_TRAIL;
            mod.trail[mod.trail_head][0] = mod.pixie_x;
            mod.trail[mod.trail_head][1] = mod.pixie_y;
        }

        if (now >= mod.pixie_leave && (mod.pixie_x < -40.0f || mod.pixie_x > w + 40.0f || mod.pixie_y < -40.0f)) {
            mod.pixie_start = 0;
            mod.pixie_due = now + 40000 + (uint32_t) saver_rand_range(50000);
        }
    }
}

typedef struct {
    float w, h;
    const char *art[HALL_SPRITE_SIZE];
} hall_sprite_t;

static const hall_sprite_t hall_sprites[HALL_SPRITE_KINDS] = {
    {0.34f, 0.34f, {"dddddddddddd", "dbbbbbbbbbbd", "dbdbbbbbbdbd", "dbbdbbbbdbbd", "dbbbdbbdbbbd", "dbbbbddbbbbd",
                    "dbbbbddbbbbd", "dbbbdbbdbbbd", "dbbdbbbbdbbd", "dbdbbbbbbdbd", "dbbbbbbbbbbd", "dddddddddddd"}},
    {0.24f, 0.40f, {".....oo.....", ".....oo.....", "....oooo....", "....wwww....", "...oooooo...", "...oooooo...",
                    "..wwwwwwww..", "..oooooooo..", ".oooooooooo.", ".oooooooooo.", "dddddddddddd", "dddddddddddd"}},
    {0.22f, 0.22f, {"............", "......yyy...", ".....yyyyy..", ".....ykyyyoo", ".....yyyyyoo", "......yyy...",
                    "..y..yyyyy..", "..yyyyyyyyy.", "..yyyyyyyyy.", "...yyyyyyy..", "....yyyyy...", "............"}},
    {0.22f, 0.22f, {"............", "....rrrr....", "..rrrrrrrr..", ".rrllrrrrrr.", ".rlllrrrrrr.", "rrllrrrrrrrr",
                    "rrrrrrrrrrrr", "rrrrrrrrrrrr", ".rrrrrrrrrr.", ".rrrrrrrrrr.", "..rrrrrrrr..", "....rrrr...."}},
    {0.30f, 0.70f, {"...ssssss...", "..ssssssss..", ".ssssssssss.", "ssssssssssss", ".....gg.....", ".....gg.....",
                    ".....gg.....", ".....gg.....", ".....gg.....", ".....gg.....", "...gggggg...", "..gggggggg.."}},
    {0.36f, 0.55f, {"..pppppppp..", "..pqqqqqqp..", "..pqqqqqqp..", "..pqqqqqqp..", "..pppppppp..", "..p......p..",
                    "pppppppppppp", "pqqqqqqqqqqp", "pppppppppppp", ".p........p.", ".p........p.", ".p........p."}},
    {0.40f, 0.40f, {"............", "............", "............", "............", "............", "............",
                    "............", "............", "..wwww......", ".wmwwmw.ww..", "wwwwwwwwmww.", ".wwmww.wwww."}},
};

static float smooth(const float t) {
    const float c = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return c * c * (3.0f - 2.0f * c);
}

static float walk_x(const hall_leg_t *leg, const float z) {
    const float sway = 0.32f * sinf(z * 0.42f + leg->phase[0]) + 0.12f * sinf(z * 1.05f + leg->phase[1]);
    return sway * smooth(z / 4.0f) * smooth((leg->len + 1.0f - z) / 5.0f);
}

static int item_at(const hall_leg_t *leg, const int k, const hall_sprite_t **sprite, float *x, float *z) {
    const uint32_t h = hash_segment((leg->base + (uint32_t) k) ^ 0x5bd1e995u);
    if ((h >> 20) % 2) return 0;

    *sprite = &hall_sprites[(h >> 21) % HALL_SPRITE_KINDS];
    *z = (float) k + 0.2f + (float) ((h >> 9) % 60) / 100.0f;
    if (*z < 2.5f || *z > leg->len - 1.0f) return 0;

    const float path = walk_x(leg, *z);
    const float half = (*sprite)->w * 0.5f;
    const float gap = half + 0.30f + (float) ((h >> 4) % 16) / 100.0f;
    const float limit = 0.80f - half;
    int side = path > 0.05f ? -1 : path < -0.05f ? 1 : ((h >> 2) % 2 ? 1 : -1);

    for (int attempt = 0; attempt < 2; attempt++, side = -side) {
        const float candidate = path + (float) side * gap;
        if (candidate < -limit || candidate > limit) continue;
        *x = candidate;
        return 1;
    }
    return 0;
}

static void plan_walk(const float dt) {
    if (!mod.turning) {
        mod.walk_z += dt * 0.9f;
        if (mod.walk_z >= mod.leg.len + 1.0f) {
            mod.walk_z = mod.leg.len + 1.0f;
            mod.turning = 1;
            mod.turn_progress = 0.0f;
        }
    } else {
        mod.turn_progress += dt / HALL_TURN_SECONDS;
        if (mod.turn_progress >= 1.0f) {
            mod.prev = mod.leg;
            mod.has_prev = 1;
            mod.leg = mod.next;
            mod.next = random_leg();
            mod.walk_z = 0.0f;
            mod.turning = 0;
            mod.turn_progress = 0.0f;
        }
    }

    mod.heading = mod.turning ? (float) mod.leg.turn * 1.5707963f * smooth(mod.turn_progress) : 0.0f;
    mod.cam_x = mod.turning ? 0.0f : walk_x(&mod.leg, mod.walk_z);
    mod.cam_z = mod.walk_z;
}

static int sprite_colour(const char key, uint8_t *rgb) {
    static const struct {
        char key;
        uint8_t r, g, b;
    } colours[] = {
        {'d', 92, 58, 30},   {'b', 168, 112, 56}, {'o', 240, 120, 30}, {'w', 236, 236, 228}, {'y', 250, 214, 40},
        {'k', 20, 20, 20},   {'r', 210, 50, 50},  {'l', 255, 170, 170}, {'s', 244, 230, 170}, {'g', 120, 120, 128},
        {'p', 150, 90, 200}, {'q', 100, 56, 140}, {'m', 160, 160, 170},
    };
    for (size_t i = 0; i < sizeof(colours) / sizeof(colours[0]); i++) {
        if (colours[i].key != key) continue;
        rgb[0] = colours[i].r;
        rgb[1] = colours[i].g;
        rgb[2] = colours[i].b;
        return 1;
    }
    return 0;
}

static const uint8_t hall_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void shade_pixel(uint8_t *out, const uint8_t *rgb, const float light, const int x, const int y) {
    float l = light < 0.0f ? 0.0f : light > 1.0f ? 1.0f : light;
    const float steps = 7.0f;
    const float scaled = l * steps + (float) hall_bayer[y & 3][x & 3] / 16.0f - 0.5f;
    int level = (int) scaled;
    if (level < 0) level = 0;
    if (level > (int) steps) level = (int) steps;
    const int k = (int) ((float) level * 256.0f / steps);
    out[0] = (uint8_t) (rgb[0] * k >> 8);
    out[1] = (uint8_t) (rgb[1] * k >> 8);
    out[2] = (uint8_t) (rgb[2] * k >> 8);
}

static float fog(const float z) {
    const float k = 1.0f - z / (float) HALL_DEPTH;
    return k <= 0.0f ? 0.0f : k * k * 1.15f;
}

static void wall_colour(uint8_t *rgb, const float u, const float v, const int left, const int plain,
                        const uint32_t now) {
    const uint32_t segment = (uint32_t) floorf(u);
    const float frac = u - floorf(u);
    const uint32_t h = hash_segment(segment);
    const int same = !plain && segment > 0 && segment % HALL_SAME_DOOR == 0;
    const int door_side = same ? 0 : (int) ((h >> 8) % 2);
    const int has_door = !plain && (same || h % 3 == 0) && door_side == left;
    const float hh = HALL_HALF_HEIGHT;

    if (has_door && frac > 0.22f && frac < 0.78f && v < hh * 0.42f) {
        const int frame = frac < 0.27f || frac > 0.73f || v > hh * 0.36f;
        if (frame) {
            rgb[0] = 70, rgb[1] = 60, rgb[2] = 52;
            return;
        }
        const int ajar = !same && (h >> 12) % 9 == 0;
        if (ajar) {
            uint8_t r, g, b;
            saver_pastel_pick((int) ((h >> 16) % SAVER_PASTEL_COUNT), &r, &g, &b);
            const float pulse = 0.92f + 0.08f * sinf((float) (now % 600000u) / 1000.0f * 1.6f + (float) (h & 0xff));
            const float glow = (0.55f + 0.45f * (frac - 0.27f) / 0.46f) * pulse;
            rgb[0] = saver_clamp_u8((int) (r * glow));
            rgb[1] = saver_clamp_u8((int) (g * glow));
            rgb[2] = saver_clamp_u8((int) (b * glow));
            return;
        }
        if (frac > 0.62f && frac < 0.66f && v > -hh * 0.25f && v < -hh * 0.15f) {
            rgb[0] = 230, rgb[1] = 190, rgb[2] = 70;
            return;
        }
        if (same) {
            rgb[0] = 180, rgb[1] = 30, rgb[2] = 34;
        } else {
            const int grain = ((int) (frac * 40.0f) % 3) == 0;
            rgb[0] = (uint8_t) (grain ? 110 : 126);
            rgb[1] = (uint8_t) (grain ? 72 : 84);
            rgb[2] = (uint8_t) (grain ? 40 : 46);
        }
        return;
    }

    const uint32_t ph = hash_segment(segment ^ 0x1b873593u);
    if (!plain && !has_door && ph % 4 == 0 && (int) ((ph >> 6) % 2) == left && frac > 0.31f && frac < 0.69f &&
        v > -hh * 0.06f && v < hh * 0.60f) {
        const float fx = (frac - 0.31f) / 0.38f;
        const float fy = (hh * 0.60f - v) / (hh * 0.66f);
        if (fx < 0.08f || fx > 0.92f || fy < 0.07f || fy > 0.93f) {
            const int gilt = (ph >> 9) % 2;
            const int edge = fx < 0.03f || fx > 0.97f || fy < 0.025f || fy > 0.975f;
            rgb[0] = gilt ? (edge ? 150 : 200) : (edge ? 70 : 104);
            rgb[1] = gilt ? (edge ? 110 : 160) : (edge ? 44 : 66);
            rgb[2] = gilt ? (edge ? 30 : 60) : (edge ? 24 : 36);
            return;
        }
        const float ax = (fx - 0.08f) / 0.84f;
        const float ay = (fy - 0.07f) / 0.86f;
        if (mod.art_loaded) {
            const uint8_t *px = mod.art[(ph >> 12) % mod.art_loaded][(int) (ay * (HALL_ART_H - 1) + 0.5f)]
                                       [(int) (ax * (HALL_ART_W - 1) + 0.5f)];
            rgb[0] = px[0], rgb[1] = px[1], rgb[2] = px[2];
            return;
        }
        const int block = ((int) (ax * 3.0f) + (int) (ay * 3.0f) * 3 + (int) (ph >> 14)) % 5;
        static const uint8_t paint[5][3] = {{200, 40, 40}, {240, 220, 200}, {40, 70, 160}, {230, 190, 40}, {30, 30, 30}};
        rgb[0] = paint[block][0], rgb[1] = paint[block][1], rgb[2] = paint[block][2];
        return;
    }

    if (frac < 0.03f) {
        rgb[0] = 40, rgb[1] = 66, rgb[2] = 70;
        return;
    }
    if (v < -hh * 0.22f && v > -hh * 0.27f) {
        rgb[0] = 160, rgb[1] = 170, rgb[2] = 150;
        return;
    }
    if (v < -hh * 0.27f) {
        rgb[0] = 54, rgb[1] = 92, rgb[2] = 96;
        return;
    }
    rgb[0] = 92, rgb[1] = 146, rgb[2] = 140;
}

static void floor_colour(uint8_t *rgb, const float x, const float z) {
    const int tile = ((int) floorf(z * 2.0f) + (int) floorf(x * 2.0f + 100.0f)) & 1;
    const float fz = z * 2.0f - floorf(z * 2.0f);
    const float fx = x * 2.0f - floorf(x * 2.0f);
    if (fz < 0.05f || fx < 0.05f) {
        rgb[0] = 60, rgb[1] = 56, rgb[2] = 52;
        return;
    }
    rgb[0] = tile ? 150 : 118;
    rgb[1] = tile ? 140 : 108;
    rgb[2] = tile ? 120 : 92;
}

enum hall_light {
    hall_light_none = 0,
    hall_light_normal,
    hall_light_broken,
    hall_light_blinking,
};

static enum hall_light light_kind(const uint32_t segment) {
    if (segment % HALL_LIGHT_EVERY) return hall_light_none;
    const uint32_t h = hash_segment(segment ^ 0x2c1b3c6du);
    if (h % 7 == 0) return hall_light_broken;
    if ((h >> 8) % 8 == 0) return hall_light_blinking;
    return hall_light_normal;
}

static float light_level(const uint32_t segment, const uint32_t now) {
    switch (light_kind(segment)) {
        case hall_light_normal:
            return 1.0f;
        case hall_light_blinking: {
            const uint32_t h = hash_segment(segment ^ 0x68e31da4u);
            const float t = (float) ((now + (h & 0xffff)) % 600000u) / 1000.0f;
            const float offset = (float) (h >> 16 & 0xff) / 40.0f;
            float level = 0.78f + 0.12f * sinf(t * 2.2f + offset) + 0.06f * sinf(t * 5.3f + offset * 2.0f);

            const float cycle = fmodf(t + offset, 7.0f);
            if (cycle > 4.0f && cycle < 6.5f) {
                const float dip = 0.5f - 0.5f * cosf((cycle - 4.0f) / 2.5f * 6.2831853f);
                level -= dip * 0.45f;
            }
            return level < 0.35f ? 0.35f : level > 1.0f ? 1.0f : level;
        }
        default:
            return 0.0f;
    }
}

static float ambient(const float z, const uint32_t now) {
    const int centre = (int) floorf(z);
    float best = 0.0f;
    for (int s = centre - HALL_LIGHT_EVERY; s <= centre + HALL_LIGHT_EVERY; s++) {
        if (s < 0) continue;
        const float level = light_level((uint32_t) s, now);
        if (level <= 0.0f) continue;
        const float d = z - ((float) s + 0.5f);
        const float reach = 1.0f - (d < 0.0f ? -d : d) / 2.6f;
        if (reach * level > best) best = reach * level;
    }
    return 0.38f + 0.62f * best;
}

static float ceiling_colour(uint8_t *rgb, const float x, const float z, const uint32_t now) {
    const float frac = z - floorf(z);
    const uint32_t segment = (uint32_t) floorf(z);
    const enum hall_light kind = light_kind(segment);

    if (kind != hall_light_none && frac > 0.36f && frac < 0.64f && x > -0.34f && x < 0.34f) {
        if (frac < 0.38f || frac > 0.62f || x < -0.32f || x > 0.32f) {
            rgb[0] = 150, rgb[1] = 152, rgb[2] = 160;
            return 0.0f;
        }
        if (kind == hall_light_broken) {
            const int crack = ((int) (x * 40.0f + 40.0f) + (int) (frac * 50.0f)) % 9 == 0 ||
                              ((int) (x * 40.0f + 40.0f) - (int) (frac * 50.0f) + 90) % 13 == 0;
            rgb[0] = crack ? 34 : 112, rgb[1] = crack ? 34 : 114, rgb[2] = crack ? 38 : 120;
            return 0.0f;
        }
        const float level = light_level(segment, now);
        if (kind == hall_light_blinking) {
            rgb[0] = (uint8_t) (128.0f + (225.0f - 128.0f) * level);
            rgb[1] = (uint8_t) (130.0f + (238.0f - 130.0f) * level);
            rgb[2] = (uint8_t) (138.0f + (255.0f - 138.0f) * level);
            return 0.6f * level;
        }
        rgb[0] = 250, rgb[1] = 248, rgb[2] = 220;
        return 0.6f;
    }
    rgb[0] = 70, rgb[1] = 74, rgb[2] = 82;
    return 0.0f;
}

static int ensure_buffer(SDL_Renderer *renderer) {
    if (mod.pixels && mod.texture) return 1;

    mod.lw = mod.base.screen_w / HALL_PIXEL;
    mod.lh = mod.base.screen_h / HALL_PIXEL;
    if (mod.lw < 120) mod.lw = 120;
    if (mod.lh < 90) mod.lh = 90;

    free(mod.pixels);
    free(mod.depth);
    mod.pixels = malloc((size_t) mod.lw * mod.lh * 3);
    mod.depth = malloc(sizeof(float) * (size_t) mod.lw);
    if (!mod.pixels || !mod.depth) return 0;

    if (mod.texture) SDL_DestroyTexture(mod.texture);
    mod.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, mod.lw, mod.lh);
    if (!mod.texture) return 0;
    SDL_SetTextureScaleMode(mod.texture, SDL_ScaleModeNearest);
    return 1;
}

static const hall_leg_t *owner_point(const float x, const float z, float *ox, float *oz) {
    const hall_leg_t *leg = &mod.leg;
    if (z >= leg->len) {
        const float t = (float) leg->turn;
        *oz = t * x;
        *ox = -t * (z - (leg->len + 1.0f));
        return &mod.next;
    }

    const int open_side = mod.has_prev ? mod.prev.turn : 0;
    if (open_side && x * (float) open_side >= 1.0f && z > -1.0f && z < 1.0f) {
        const float t = (float) mod.prev.turn;
        *oz = (mod.prev.len + 1.0f) - t * x;
        *ox = t * z;
        return &mod.prev;
    }

    *ox = x;
    *oz = z;
    return leg;
}

static void leg_to_view(const hall_leg_t *owner, const float ox, const float oz, float *x, float *z) {
    if (owner == &mod.next) {
        const float t = (float) mod.leg.turn;
        *x = t * oz;
        *z = (mod.leg.len + 1.0f) - t * ox;
        return;
    }
    if (owner == &mod.prev) {
        const float t = (float) mod.prev.turn;
        *z = t * ox;
        *x = -t * (oz - (mod.prev.len + 1.0f));
        return;
    }
    *x = ox;
    *z = oz;
}

static float cast_ray(const float px, const float pz, const float rx, const float rz, float *hx, float *hz) {
    const float len = mod.leg.len;
    const int open_side = mod.has_prev ? mod.prev.turn : 0;
    float best = 1e9f;

    if (rx > 1e-6f) {
        const float t = (1.0f - px) / rx;
        const float z = pz + rz * t;
        if (t > 1e-4f && z < len && !(open_side == 1 && z > -1.0f && z < 1.0f) && t < best) best = t;
    } else if (rx < -1e-6f) {
        const float t = (-1.0f - px) / rx;
        const float z = pz + rz * t;
        if (t > 1e-4f && z < len && !(open_side == -1 && z > -1.0f && z < 1.0f) && t < best) best = t;
    }

    if (rz > 1e-6f) {
        const float t = (len + 2.0f - pz) / rz;
        if (t > 1e-4f && t < best) best = t;
    }

    if (rz > 1e-6f || rz < -1e-6f) {
        const float t = (len - pz) / rz;
        const float x = px + rx * t;
        if (t > 1e-4f && (x >= 1.0f || x <= -1.0f) && t < best) best = t;

        if (open_side) {
            for (int side = -1; side <= 1; side += 2) {
                const float tw = ((float) side - pz) / rz;
                const float xw = px + rx * tw;
                if (tw > 1e-4f && xw * (float) open_side >= 1.0f && tw < best) best = tw;
            }
        }
    }

    *hx = px + rx * best;
    *hz = pz + rz * best;
    return best;
}

static void draw_scene(const uint32_t now) {
    const int lw = mod.lw;
    const int lh = mod.lh;
    const float focal = (float) lh * 0.75f;
    const float cx = (float) lw * 0.5f;
    const float cy = (float) lh * 0.5f;
    const float hh = HALL_HALF_HEIGHT;
    const float fx = sinf(mod.heading);
    const float fz = cosf(mod.heading);
    const float rgx = cosf(mod.heading);
    const float rgz = -sinf(mod.heading);
    const float px = mod.cam_x;
    const float pz = mod.cam_z;
    const int straight = mod.heading == 0.0f;

    static float row_light[HALL_MAX_ROWS];
    if (straight) {
        for (int row = 0; row < lh && row < HALL_MAX_ROWS; row++) {
            const float dy = (float) row + 0.5f - cy;
            const float d = hh * focal / (dy < 0.0f ? -dy : dy);
            row_light[row] = d < (float) HALL_DEPTH ? ambient((float) mod.leg.base + pz + d, now) : 1.0f;
        }
    }

    for (int col = 0; col < lw; col++) {
        const float c = ((float) col + 0.5f - cx) / focal;
        const float rx = fx + rgx * c;
        const float rz = fz + rgz * c;

        float hx, hz;
        float depth = cast_ray(px, pz, rx, rz, &hx, &hz);
        if (depth > (float) HALL_DEPTH * 4.0f) depth = (float) HALL_DEPTH * 4.0f;
        mod.depth[col] = depth;

        const int top = (int) (cy - hh * focal / depth);
        const int bottom = (int) (cy + hh * focal / depth);

        float wox = 0.0f, woz = 0.0f;
        const hall_leg_t *wall_owner = owner_point(hx, hz, &wox, &woz);
        const int wall_left = wox < 0.0f;
        const int wall_plain = woz < 2.5f || woz > wall_owner->len - 1.5f;
        const float wall_u = (float) wall_owner->base + woz;
        const float wall_light = depth < (float) HALL_DEPTH ? ambient(wall_u, now) : 1.0f;

        for (int row = 0; row < lh; row++) {
            uint8_t *out = mod.pixels + ((size_t) row * lw + col) * 3;
            uint8_t rgb[3];

            if (row >= top && row <= bottom) {
                if (depth >= (float) HALL_DEPTH) {
                    out[0] = out[1] = out[2] = 0;
                    continue;
                }
                const float v = (cy - (float) row) * depth / focal;
                wall_colour(rgb, wall_u, v, wall_left, wall_plain, now);
                shade_pixel(out, rgb, fog(depth) * wall_light, col, row);
                continue;
            }

            const float dy = (float) row + 0.5f - cy;
            const float d = hh * focal / (dy < 0.0f ? -dy : dy);
            if (d >= (float) HALL_DEPTH) {
                out[0] = out[1] = out[2] = 0;
                continue;
            }

            float ox, oz;
            const hall_leg_t *owner = owner_point(px + rx * d, pz + rz * d, &ox, &oz);
            const float world_z = (float) owner->base + oz;
            const float lit = straight && owner == &mod.leg && row < HALL_MAX_ROWS ? row_light[row] : ambient(world_z, now);
            if (row > bottom) {
                floor_colour(rgb, ox, world_z);
                shade_pixel(out, rgb, fog(d) * lit, col, row);
            } else {
                const float boost = ceiling_colour(rgb, ox, world_z, now);
                shade_pixel(out, rgb, fog(d) * lit + boost, col, row);
            }
        }
    }

    const hall_leg_t *legs[3] = {&mod.prev, &mod.leg, &mod.next};
    for (int l = 0; l < 3; l++) {
        const hall_leg_t *leg = legs[l];
        if (leg == &mod.prev && !mod.has_prev) continue;

        for (int k = (int) leg->len; k >= 0; k--) {
            const hall_sprite_t *sprite;
            float item_x, item_z;
            if (!item_at(leg, k, &sprite, &item_x, &item_z)) continue;

            float vx, vz;
            leg_to_view(leg, item_x, item_z, &vx, &vz);
            const float relx = vx - px;
            const float relz = vz - pz;
            const float z = relx * fx + relz * fz;
            if (z < 0.35f || z >= (float) HALL_DEPTH) continue;
            const float lateral = relx * rgx + relz * rgz;

            const float scale = focal / z;
            const float sx = cx + lateral * scale;
            const float floor_row = cy + hh * scale;
            const float pw = sprite->w * scale;
            const float ph = sprite->h * scale;
            const int x0 = (int) (sx - pw * 0.5f);
            const int x1 = (int) (sx + pw * 0.5f);
            const int y0 = (int) (floor_row - ph);
            const int y1 = (int) floor_row;
            if (x1 <= x0 || y1 <= y0 || x1 < 0 || x0 >= lw) continue;
            const float light = fog(z) * ambient((float) leg->base + item_z, now);

            for (int col = x0 < 0 ? 0 : x0; col < x1 && col < lw; col++) {
                if (mod.depth[col] < z) continue;
                const int u = (int) ((float) (col - x0) * HALL_SPRITE_SIZE / (float) (x1 - x0));
                for (int row = y0 < 0 ? 0 : y0; row < y1 && row < lh; row++) {
                    const int v = (int) ((float) (row - y0) * HALL_SPRITE_SIZE / (float) (y1 - y0));
                    uint8_t rgb[3];
                    if (!sprite_colour(sprite->art[v][u], rgb)) continue;
                    shade_pixel(mod.pixels + ((size_t) row * lw + col) * 3, rgb, light, col, row);
                }
            }
        }
    }
}

static void add_light(const int x, const int y, const int r, const int g, const int b) {
    if (x < 0 || y < 0 || x >= mod.lw || y >= mod.lh) return;
    uint8_t *out = mod.pixels + ((size_t) y * mod.lw + x) * 3;
    out[0] = saver_clamp_u8(out[0] + r);
    out[1] = saver_clamp_u8(out[1] + g);
    out[2] = saver_clamp_u8(out[2] + b);
}

static void draw_pixie(void) {
    if (!mod.pixie_start) return;

    const float sx = (float) mod.lw / (float) mod.base.screen_w;
    const float sy = (float) mod.lh / (float) mod.base.screen_h;
    const int px = (int) (mod.pixie_x * sx);
    const int py = (int) (mod.pixie_y * sy);

    for (int i = 1; i < HALL_PIXIE_TRAIL; i++) {
        const int index = (mod.trail_head + i) % HALL_PIXIE_TRAIL;
        const int strength = 20 + i * 14;
        add_light((int) (mod.trail[index][0] * sx), (int) (mod.trail[index][1] * sy), strength / 2, strength,
                  strength / 2);
    }

    for (int y = -6; y <= 6; y++) {
        for (int x = -6; x <= 6; x++) {
            const int d = x * x + y * y;
            if (d > 36) continue;
            add_light(px + x, py + y, 8, 60 - d * 3 / 2, 16);
        }
    }

    const int flap = sinf(mod.pixie_phase * 9.0f) > 0.0f ? 1 : 0;
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 2; i <= 5; i++) {
            const int lift = flap ? (i - 1) : (i / 2);
            add_light(px + side * i, py - lift, 110, 150, 135);
            add_light(px + side * i, py - lift + 1, 70, 100, 90);
        }
    }

    for (int y = -2; y <= 2; y++) {
        for (int x = -1; x <= 1; x++)
            add_light(px + x, py + y, 150, 255, 160);
    }
    add_light(px, py - 3, 200, 255, 200);
}

void hallway_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active) return;
    if (!ensure_buffer(renderer)) return;

    const uint32_t now = SDL_GetTicks();
    draw_scene(now);
    draw_pixie();

    SDL_UpdateTexture(mod.texture, NULL, mod.pixels, mod.lw * 3);

    const int speed = mod.base.speed > 0 ? mod.base.speed : 90;
    const float calm = speed <= 150 ? 1.0f : 150.0f / (float) speed * 0.7f;
    const int margin = mod.base.screen_h / 60 + 2;
    const float bob = sinf(mod.bob_phase) * (float) mod.base.screen_h * 0.006f * calm;
    SDL_Rect dst = {0, -margin + (int) bob, mod.base.screen_w, mod.base.screen_h + margin * 2};
    SDL_RenderCopy(renderer, mod.texture, NULL, &dst);
    saver_perf_note_draw_calls(1);
}

int hallway_active(void) {
    return saver_active_base(&mod.base);
}

void hallway_stop(void) {
    saver_stop_base(&mod.base);
}

void hallway_shutdown(void) {
    if (mod.texture) {
        SDL_DestroyTexture(mod.texture);
        mod.texture = NULL;
    }
    free(mod.pixels);
    free(mod.depth);
    mod.pixels = NULL;
    mod.depth = NULL;
    for (int i = 0; i < mod.art_path_count; i++)
        free(mod.art_paths[i]);
    free(mod.art_paths);
    mod.art_paths = NULL;
    mod.art_path_count = 0;
    mod.art_loaded = 0;
    saver_shutdown_base(&mod.base);
}
