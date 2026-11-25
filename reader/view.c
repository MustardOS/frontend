#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/base/options.h>
#include <common/display/screenshot.h>
#include <common/platform/display.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "settings.h"
#include "view.h"

#define VIEW_PLAIN_BG    0x000000
#define VIEW_PLAIN_PAPER 0xF2F0EA
#define VIEW_TILE        32
#define COLOUR_SHIFT     10
#define COLOUR_ONE       (1 << COLOUR_SHIFT)

typedef struct {
    uint32_t paper;
    uint32_t ink;
} view_palette;

typedef enum {
    turn_from,
    turn_flap,
    turn_to,
} turn_kind;

static const view_palette palettes[reader_theme_count] = {
    {0xFFFFFF, 0x000000},
    {0xF4ECD8, 0x4B3A2A},
    {0x1F1F1F, 0xD6D6D6},
    {0x000000, 0xFFB347},
};

static const int sharpen_gain[reader_sharpness_count] = {0, 3, 6};

static SDL_Texture *texture = NULL;
static SDL_Texture *texture_prev = NULL;
static uint32_t *logical = NULL;
static uint32_t *previous = NULL;
static uint32_t *frame = NULL;
static uint8_t *turn_kinds = NULL;
static uint32_t *turn_levels = NULL;
static int *turn_sources = NULL;
static uint8_t *sharp_row = NULL;
static size_t sharp_size = 0;
static uint8_t theme_lut[3][256];
static int pre_lut[3][256];
static uint8_t gamma_lut[COLOUR_ONE + 1];
static int colour_matrix[9];
static int colour_cross = 0;
static int transition = reader_transition_instant;
static int pending = 0;
static int animating = 0;
static int gpu_fade = 0;
static int fade_alpha = 255;
static int anim_direction = 1;
static uint32_t anim_start = 0;
static uint32_t anim_length = 0;
static int anim_w = 0;
static int anim_h = 0;
static uint32_t anim_paper = 0xFFFFFFFFu;
static int screen_w = 0;
static int screen_h = 0;
static int rotation = reader_rotation_none;

int view_init(SDL_Renderer *renderer, const int width, const int height) {
    screen_w = width;
    screen_h = height;

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture) {
        LOG_ERROR(mux_module, "Chutney: cannot create page texture: %s", SDL_GetError());
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    /* A second texture lets the renderer do the crossfade so no pixels are mixed by hand */
    texture_prev = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture_prev) SDL_SetTextureBlendMode(texture_prev, SDL_BLENDMODE_BLEND);

    const size_t pixels = (size_t) width * (size_t) height;
    const size_t longest = (size_t) (width > height ? width : height);
    logical = malloc(pixels * sizeof(*logical));
    previous = malloc(pixels * sizeof(*previous));
    frame = malloc(pixels * sizeof(*frame));
    turn_kinds = malloc(longest * sizeof(*turn_kinds));
    turn_levels = malloc(longest * sizeof(*turn_levels));
    turn_sources = malloc(longest * sizeof(*turn_sources));
    if (!logical || !previous || !frame || !turn_kinds || !turn_levels || !turn_sources) return 0;

    return 1;
}

void view_free(void) {
    if (texture) SDL_DestroyTexture(texture);
    if (texture_prev) SDL_DestroyTexture(texture_prev);
    free(logical);
    free(previous);
    free(frame);
    free(turn_kinds);
    free(turn_levels);
    free(turn_sources);
    free(sharp_row);
    sharp_row = NULL;
    sharp_size = 0;

    texture = NULL;
    texture_prev = NULL;
    logical = NULL;
    previous = NULL;
    frame = NULL;
    turn_kinds = NULL;
    turn_levels = NULL;
    turn_sources = NULL;
}

void view_set_rotation(const int value) {
    rotation = value >= 0 && value < reader_rotation_count ? value : reader_rotation_none;
}

void view_logical_size(int *width, int *height) {
    if (rotation == reader_rotation_none || rotation == reader_rotation_upside) {
        *width = screen_w;
        *height = screen_h;
    } else {
        *width = screen_h;
        *height = screen_w;
    }
}

static uint8_t mix(const uint32_t ink, const uint32_t paper, const int shift, const unsigned char value) {
    const int from = (int) ((ink >> shift) & 0xFF);
    const int to = (int) ((paper >> shift) & 0xFF);
    return (uint8_t) (from + (to - from) * value / 255);
}

static void fill_rows(const uint32_t colour, const int width, const int from, const int to) {
    const uint32_t pixel = 0xFF000000u | colour;
    uint32_t *cursor = logical + (size_t) from * (size_t) width;
    const uint32_t *end = logical + (size_t) to * (size_t) width;
    while (cursor < end)
        *cursor++ = pixel;
}

static void upload(SDL_Texture *target, const uint32_t *source, const int width, const int height) {
    void *pixels = NULL;
    int pitch = 0;
    if (!target || SDL_LockTexture(target, NULL, &pixels, &pitch) != 0) return;

    uint8_t *base = pixels;
    const size_t row_bytes = (size_t) width * sizeof(uint32_t);

    if (rotation == reader_rotation_none) {
        if ((size_t) pitch == row_bytes) {
            memcpy(base, source, row_bytes * (size_t) height);
        } else {
            for (int y = 0; y < height; y++)
                memcpy(base + (size_t) y * (size_t) pitch, source + (size_t) y * (size_t) width, row_bytes);
        }
    } else if (rotation == reader_rotation_upside) {
        for (int ly = 0; ly < height; ly++) {
            const uint32_t *row = source + (size_t) ly * (size_t) width;
            uint32_t *out = (uint32_t *) (base + (size_t) (screen_h - 1 - ly) * (size_t) pitch) + (screen_w - 1);
            for (int lx = 0; lx < width; lx++)
                *out-- = row[lx];
        }
    } else {
        /* Quarter turns walk the page in small tiles so the reads and writes both stay in cache */
        const int right = rotation == reader_rotation_right;
        for (int ty = 0; ty < height; ty += VIEW_TILE) {
            const int y_end = ty + VIEW_TILE < height ? ty + VIEW_TILE : height;
            for (int tx = 0; tx < width; tx += VIEW_TILE) {
                const int x_end = tx + VIEW_TILE < width ? tx + VIEW_TILE : width;
                for (int lx = tx; lx < x_end; lx++) {
                    const int sy = right ? lx : screen_h - 1 - lx;
                    uint32_t *out = (uint32_t *) (base + (size_t) sy * (size_t) pitch);
                    const uint32_t *in = source + (size_t) ty * (size_t) width + (size_t) lx;
                    if (right) {
                        for (int ly = ty; ly < y_end; ly++, in += width)
                            out[screen_w - 1 - ly] = *in;
                    } else {
                        for (int ly = ty; ly < y_end; ly++, in += width)
                            out[ly] = *in;
                    }
                }
            }
        }
    }

    SDL_UnlockTexture(target);
}

static uint8_t clamp_byte(const int value) {
    return (uint8_t) (value < 0 ? 0 : value > 255 ? 255 : value);
}

static void matrix_multiply(float left[3][3], float right[3][3], float out[3][3]) {
    float result[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            result[i][j] = left[i][0] * right[0][j] + left[i][1] * right[1][j] + left[i][2] * right[2][j];
    memcpy(out, result, sizeof(result));
}

static void build_colour(const view_palette palette, const int plain, const view_colour *colour) {
    const float brightness = colour ? (float) colour->brightness / 100.0f : 0.0f;
    const float contrast = colour ? (float) colour->contrast / 100.0f : 1.0f;
    const float saturation = colour ? (float) colour->saturation / 100.0f : 1.0f;
    const float hue = colour ? (float) colour->hue_shift * 0.01745329251994329577f : 0.0f;
    const float gamma = colour && colour->gamma > 0 ? 100.0f / (float) colour->gamma : 1.0f;

    for (int v = 0; v < 256; v++) {
        const unsigned char c = (unsigned char) v;
        theme_lut[0][v] = plain ? c : mix(palette.ink, palette.paper, 16, c);
        theme_lut[1][v] = plain ? c : mix(palette.ink, palette.paper, 8, c);
        theme_lut[2][v] = plain ? c : mix(palette.ink, palette.paper, 0, c);
        const float level = ((float) v / 255.0f + brightness - 0.5f) * contrast + 0.5f;
        pre_lut[0][v] = pre_lut[1][v] = pre_lut[2][v] = (int) lrintf(level * (float) COLOUR_ONE);
    }

    for (int v = 0; v <= COLOUR_ONE; v++)
        gamma_lut[v] = (uint8_t) lrintf(powf((float) v / (float) COLOUR_ONE, gamma) * 255.0f);

    static const float luma[3] = {0.2126f, 0.7152f, 0.0722f};
    float combined[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            combined[i][j] = (i == j ? saturation : 0.0f) + (1.0f - saturation) * luma[j];

    const float cs = cosf(hue);
    const float sn = sinf(hue);
    const float hue_columns[9] = {
        0.299f + 0.701f * cs + 0.168f * sn, 0.587f - 0.587f * cs + 0.330f * sn, 0.114f - 0.114f * cs - 0.497f * sn,
        0.299f - 0.299f * cs - 0.328f * sn, 0.587f + 0.413f * cs + 0.035f * sn, 0.114f - 0.114f * cs + 0.292f * sn,
        0.299f - 0.300f * cs + 1.250f * sn, 0.587f - 0.588f * cs - 1.050f * sn, 0.114f + 0.886f * cs - 0.203f * sn,
    };
    float stage[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            stage[i][j] = hue_columns[j * 3 + i];
    matrix_multiply(stage, combined, combined);

    if (colour && colour->filter_active) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                stage[i][j] = colour->matrix[j * 3 + i];
        matrix_multiply(stage, combined, combined);
    }

    colour_cross = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            colour_matrix[i * 3 + j] = (int) lrintf(combined[i][j] * (float) COLOUR_ONE);
            if (colour_matrix[i * 3 + j] != (i == j ? COLOUR_ONE : 0)) colour_cross = 1;
        }
}

static uint8_t colour_out(const int value) {
    return gamma_lut[value < 0 ? 0 : value > COLOUR_ONE ? COLOUR_ONE : value];
}

static uint32_t shade_pixel(const uint8_t sr, const uint8_t sg, const uint8_t sb) {
    const int r = pre_lut[0][theme_lut[0][sr]];
    const int g = pre_lut[1][theme_lut[1][sg]];
    const int b = pre_lut[2][theme_lut[2][sb]];
    const int *m = colour_matrix;
    return 0xFF000000u | ((uint32_t) colour_out((m[0] * r + m[1] * g + m[2] * b) >> COLOUR_SHIFT) << 16)
           | ((uint32_t) colour_out((m[3] * r + m[4] * g + m[5] * b) >> COLOUR_SHIFT) << 8)
           | colour_out((m[6] * r + m[7] * g + m[8] * b) >> COLOUR_SHIFT);
}

static uint32_t shade_colour(const uint32_t rgb) {
    const int r = pre_lut[0][(rgb >> 16) & 0xFF];
    const int g = pre_lut[1][(rgb >> 8) & 0xFF];
    const int b = pre_lut[2][rgb & 0xFF];
    const int *m = colour_matrix;
    return ((uint32_t) colour_out((m[0] * r + m[1] * g + m[2] * b) >> COLOUR_SHIFT) << 16)
           | ((uint32_t) colour_out((m[3] * r + m[4] * g + m[5] * b) >> COLOUR_SHIFT) << 8)
           | colour_out((m[6] * r + m[7] * g + m[8] * b) >> COLOUR_SHIFT);
}

static int colour_identity(const uint8_t *r, const uint8_t *g, const uint8_t *b) {
    if (colour_cross) return 0;
    for (int v = 0; v < 256; v++)
        if (r[v] != v || g[v] != v || b[v] != v) return 0;
    return 1;
}

static const uint8_t *
sharpen_row(const doc_image *image, const int row, const int from_x, const int count, const int gain) {
    const size_t need = (size_t) count * 3;
    if (need > sharp_size) {
        uint8_t *grown = realloc(sharp_row, need);
        if (!grown) return NULL;
        sharp_row = grown;
        sharp_size = need;
    }

    const int components = image->components;
    const int above = row > 0 ? row - 1 : row;
    const int below = row + 1 < image->height ? row + 1 : row;
    const unsigned char *line = image->samples + (size_t) row * (size_t) image->stride;
    const unsigned char *up = image->samples + (size_t) above * (size_t) image->stride;
    const unsigned char *down = image->samples + (size_t) below * (size_t) image->stride;

    for (int i = 0; i < count; i++) {
        const int x = from_x + i;
        const int left = (x > 0 ? x - 1 : x) * components;
        const int right = (x + 1 < image->width ? x + 1 : x) * components;
        const int at = x * components;
        for (int ch = 0; ch < 3; ch++) {
            const int c = line[at + ch];
            const int edge = 4 * c - line[left + ch] - line[right + ch] - up[at + ch] - down[at + ch];
            sharp_row[i * 3 + ch] = clamp_byte(c + edge * gain / 8);
        }
    }

    return sharp_row;
}

void view_compose(
    const doc_image *image, const int scroll_x, const int scroll_y, const int theme, const int plain,
    const view_colour *colour
) {
    if (!texture || !logical) return;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);

    const view_palette palette = palettes[theme >= 0 && theme < reader_theme_count ? theme : 0];
    build_colour(palette, plain, colour);
    const uint32_t backdrop = shade_colour(plain ? VIEW_PLAIN_BG : palette.paper);
    anim_paper = 0xFF000000u | shade_colour(plain ? VIEW_PLAIN_PAPER : palette.paper);
    const int sharpness = colour ? colour->sharpness : reader_sharpness_normal;
    const int gain = sharpen_gain[sharpness >= 0 && sharpness < reader_sharpness_count ? sharpness : 0];

    const int area = height;

    int copy_w = 0;
    int copy_h = 0;
    int dst_x = 0;
    int dst_y = 0;
    if (image && image->samples) {
        copy_w = image->width < width ? image->width : width;
        copy_h = image->height < area ? image->height : area;
        dst_x = image->width < width ? (width - image->width) / 2 : 0;
        dst_y = image->height < area ? (area - image->height) / 2 : 0;
    }

    /* Only the strips around the page need the backdrop, the page itself overwrites the rest */
    fill_rows(backdrop, width, 0, dst_y);
    fill_rows(backdrop, width, dst_y + copy_h, height);

    if (copy_w > 0 && copy_h > 0) {
        uint8_t lut_r[256];
        uint8_t lut_g[256];
        uint8_t lut_b[256];
        for (int v = 0; v < 256; v++) {
            lut_r[v] = colour_out(pre_lut[0][theme_lut[0][v]]);
            lut_g[v] = colour_out(pre_lut[1][theme_lut[1][v]]);
            lut_b[v] = colour_out(pre_lut[2][theme_lut[2][v]]);
        }
        const int identity = colour_identity(lut_r, lut_g, lut_b);
        const uint32_t edge = 0xFF000000u | backdrop;

        for (int y = 0; y < copy_h; y++) {
            const uint8_t *sharpened = gain ? sharpen_row(image, scroll_y + y, scroll_x, copy_w, gain) : NULL;
            const int components = sharpened ? 3 : image->components;
            const unsigned char *src = sharpened ? sharpened
                                                 : image->samples + (size_t) (scroll_y + y) * (size_t) image->stride
                                                       + (size_t) scroll_x * (size_t) components;
            uint32_t *line = logical + (size_t) (dst_y + y) * (size_t) width;
            uint32_t *dst = line + dst_x;

            for (int x = 0; x < dst_x; x++)
                line[x] = edge;
            for (int x = dst_x + copy_w; x < width; x++)
                line[x] = edge;

            if (identity) {
                for (int x = 0; x < copy_w; x++, src += components)
                    dst[x] = 0xFF000000u | ((uint32_t) src[0] << 16) | ((uint32_t) src[1] << 8) | src[2];
            } else if (colour_cross) {
                for (int x = 0; x < copy_w; x++, src += components)
                    dst[x] = shade_pixel(src[0], src[1], src[2]);
            } else {
                for (int x = 0; x < copy_w; x++, src += components)
                    dst[x] = 0xFF000000u | ((uint32_t) lut_r[src[0]] << 16) | ((uint32_t) lut_g[src[1]] << 8)
                             | lut_b[src[2]];
            }
        }
    }

    if (pending && transition != reader_transition_instant) {
        pending = 0;
        animating = 1;
        anim_start = SDL_GetTicks();
        anim_length = transition == reader_transition_page_turn ? 340
                      : transition == reader_transition_slide   ? 240
                                                                : 200;
        anim_w = width;
        anim_h = height;

        gpu_fade = transition == reader_transition_crossfade && texture_prev;
        if (gpu_fade) {
            fade_alpha = 0;
            upload(texture_prev, previous, width, height);
            upload(texture, logical, width, height);
        } else {
            upload(texture, previous, width, height);
        }
        return;
    }

    pending = 0;
    animating = 0;
    gpu_fade = 0;
    upload(texture, logical, width, height);
}

int view_save_png(const char *path) {
    if (!logical || !path || !path[0]) return 0;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);
    if (width <= 0 || height <= 0) return 0;

    uint8_t *rgb = malloc((size_t) width * (size_t) height * 3U);
    if (!rgb) return 0;

    const size_t pixels = (size_t) width * (size_t) height;
    for (size_t i = 0; i < pixels; i++) {
        rgb[i * 3] = (uint8_t) (logical[i] >> 16);
        rgb[i * 3 + 1] = (uint8_t) (logical[i] >> 8);
        rgb[i * 3 + 2] = (uint8_t) logical[i];
    }

    const int result = screenshot_write_rgb(path, rgb, (uint32_t) width, (uint32_t) height);
    free(rgb);
    return result == 0;
}

void view_draw(SDL_Renderer *renderer) {
    if (!texture) return;

    if (animating && gpu_fade) {
        display_render_logical_texture(renderer, texture_prev);
        SDL_SetTextureAlphaMod(texture, (Uint8) fade_alpha);
        display_render_logical_texture(renderer, texture);
        SDL_SetTextureAlphaMod(texture, 255);
        return;
    }

    display_render_logical_texture(renderer, texture);
}

void view_set_transition(const int value) {
    transition = value >= 0 && value < reader_transition_count ? value : reader_transition_instant;
}

static uint32_t blend(const uint32_t from, const uint32_t to, const uint32_t amount) {
    const uint32_t keep = 256 - amount;
    const uint32_t rb = (((from & 0xFF00FFu) * keep + (to & 0xFF00FFu) * amount) >> 8) & 0xFF00FFu;
    const uint32_t g = (((from & 0x00FF00u) * keep + (to & 0x00FF00u) * amount) >> 8) & 0x00FF00u;
    return 0xFF000000u | rb | g;
}

static uint32_t shade(const uint32_t pixel, const uint32_t level) {
    return level ? blend(pixel, 0xFF000000u, level) : pixel;
}

static void
crossfade(const uint32_t *from, const uint32_t *to, const uint32_t amount, const int width, const int height) {
    const size_t count = (size_t) width * (size_t) height;
    for (size_t i = 0; i < count; i++)
        frame[i] = blend(from[i], to[i], amount);
}

void view_prepare_transition(const int direction) {
    if (!logical || transition == reader_transition_instant) return;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);

    /* A crossfade done by the renderer has no frame in memory, so make the one on screen */
    if (animating && gpu_fade) crossfade(previous, logical, (uint32_t) fade_alpha * 256u / 255u, width, height);

    const size_t pixels = (size_t) width * (size_t) height;
    memcpy(previous, animating ? frame : logical, pixels * sizeof(*previous));

    anim_direction = direction < 0 ? -1 : 1;
    pending = 1;
}

static void slide(const uint32_t *from, const uint32_t *to, const double s, const int width, const int height) {
    int offset = (int) (s * width);
    if (offset > width) offset = width;

    for (int y = 0; y < height; y++) {
        const size_t row = (size_t) y * (size_t) width;
        const int keep = width - offset;
        if (keep > 0) memcpy(frame + row, from + row + offset, (size_t) keep * sizeof(uint32_t));
        if (offset > 0) memcpy(frame + row + keep, to + row, (size_t) offset * sizeof(uint32_t));
    }
}

static void page_turn(const uint32_t *from, const uint32_t *to, const double s, const int width, const int height) {
    const int edge = (int) ((1.0 - s) * width);
    const int flap = (width - edge) / 2;
    const int flap_start = edge - flap;
    const int shadow = width / 20 > 4 ? width / 20 : 4;

    /* Only the columns around the fold change per pixel, everything either side is a straight copy */
    int low = flap > 0 ? flap_start - shadow + 1 : edge;
    int high = edge + shadow;
    if (low < 0) low = 0;
    if (low > width) low = width;
    if (high > width) high = width;
    if (high < low) high = low;

    for (int x = low; x < high; x++) {
        if (x >= edge) {
            turn_kinds[x] = turn_to;
            turn_levels[x] = (uint32_t) (110 * (shadow - (x - edge)) / shadow);
        } else if (flap > 0 && x >= flap_start) {
            int source = edge + (edge - x) * 2;
            if (source >= width) source = width - 1;
            turn_kinds[x] = turn_flap;
            turn_sources[x] = source;
            turn_levels[x] = (uint32_t) (60 * (edge - x) / flap);
        } else {
            turn_kinds[x] = turn_from;
            turn_levels[x] = (uint32_t) (70 * (shadow - (flap_start - x)) / shadow);
        }
    }

    for (int y = 0; y < height; y++) {
        const size_t row = (size_t) y * (size_t) width;
        uint32_t *out = frame + row;

        if (low > 0) memcpy(out, from + row, (size_t) low * sizeof(uint32_t));

        for (int x = low; x < high; x++) {
            switch (turn_kinds[x]) {
                case turn_to:
                    out[x] = shade(to[row + (size_t) x], turn_levels[x]);
                    break;
                case turn_flap:
                    out[x] = shade(blend(from[row + (size_t) turn_sources[x]], anim_paper, 200), turn_levels[x]);
                    break;
                default:
                    out[x] = shade(from[row + (size_t) x], turn_levels[x]);
                    break;
            }
        }

        if (high < width) memcpy(out + high, to + row + high, (size_t) (width - high) * sizeof(uint32_t));
    }
}

int view_tick(void) {
    if (!animating) return 0;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);
    if (width != anim_w || height != anim_h) {
        animating = 0;
        gpu_fade = 0;
        upload(texture, logical, width, height);
        return 1;
    }

    const uint32_t elapsed = SDL_GetTicks() - anim_start;
    if (elapsed >= anim_length) {
        animating = 0;
        if (!gpu_fade) upload(texture, logical, width, height);
        gpu_fade = 0;
        fade_alpha = 255;
        return 1;
    }

    const double t = (double) elapsed / (double) anim_length;
    const double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);

    if (gpu_fade) {
        fade_alpha = (int) (eased * 255.0);
        return 1;
    }

    if (transition == reader_transition_crossfade) {
        crossfade(previous, logical, (uint32_t) (eased * 256.0), width, height);
    } else {
        const int forward = anim_direction > 0;
        const uint32_t *from = forward ? previous : logical;
        const uint32_t *to = forward ? logical : previous;
        const double s = forward ? eased : 1.0 - eased;

        if (transition == reader_transition_slide)
            slide(from, to, s, width, height);
        else
            page_turn(from, to, s, width, height);
    }

    upload(texture, frame, width, height);
    return 1;
}

int view_animating(void) {
    return animating;
}
