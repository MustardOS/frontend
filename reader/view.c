#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_ttf.h>
#include <common/base/options.h>
#include <common/platform/display.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "settings.h"
#include "view.h"

#define VIEW_STATUS_FONT OPT_PATH "share/font/NotoSans/NotoSans-SemiBold.ttf"
#define VIEW_PLAIN_BG    0x000000

typedef struct {
    uint32_t paper;
    uint32_t ink;
} view_palette;

static const view_palette palettes[reader_theme_count] = {
    {0xFFFFFF, 0x000000},
    {0xF4ECD8, 0x4B3A2A},
    {0x1F1F1F, 0xD6D6D6},
    {0x000000, 0xFFB347},
};

static SDL_Texture *texture = NULL;
static TTF_Font *status_font = NULL;
static uint32_t *logical = NULL;
static uint32_t *previous = NULL;
static uint32_t *frame = NULL;
static int transition = reader_transition_instant;
static int pending = 0;
static int animating = 0;
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

    const size_t pixels = (size_t) width * (size_t) height;
    logical = malloc(pixels * sizeof(*logical));
    previous = malloc(pixels * sizeof(*previous));
    frame = malloc(pixels * sizeof(*frame));
    if (!logical || !previous || !frame) return 0;

    if (!TTF_WasInit() && TTF_Init() != 0) LOG_WARN(mux_module, "Chutney: TTF_Init failed: %s", TTF_GetError());

    const int short_side = width < height ? width : height;
    int size = short_side / 30;
    if (size < 12) size = 12;
    status_font = TTF_OpenFont(VIEW_STATUS_FONT, size);
    if (!status_font) LOG_WARN(mux_module, "Chutney: no status font: %s", TTF_GetError());

    return 1;
}

void view_free(void) {
    if (texture) SDL_DestroyTexture(texture);
    if (status_font) TTF_CloseFont(status_font);
    free(logical);
    free(previous);
    free(frame);

    texture = NULL;
    status_font = NULL;
    logical = NULL;
    previous = NULL;
    frame = NULL;
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

int view_status_height(void) {
    return status_font ? TTF_FontLineSkip(status_font) + 4 : 0;
}

static uint8_t mix(const uint32_t ink, const uint32_t paper, const int shift, const unsigned char value) {
    const int from = (int) ((ink >> shift) & 0xFF);
    const int to = (int) ((paper >> shift) & 0xFF);
    return (uint8_t) (from + (to - from) * value / 255);
}

static void fill(const uint32_t colour, const int width, const int height) {
    const uint32_t pixel = 0xFF000000u | colour;
    const size_t count = (size_t) width * (size_t) height;
    for (size_t i = 0; i < count; i++)
        logical[i] = pixel;
}

static void
blend_status(const char *status, const int width, const int height, const uint32_t paper, const uint32_t ink) {
    if (!status || !status[0] || !status_font) return;

    const SDL_Color colour = {(ink >> 16) & 0xFF, (ink >> 8) & 0xFF, ink & 0xFF, 255};
    SDL_Surface *rendered = TTF_RenderUTF8_Blended(status_font, status, colour);
    if (!rendered) return;

    SDL_Surface *surface = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(rendered);
    if (!surface) return;

    const int band = view_status_height();
    const int top = height - band;
    const int left = (width - surface->w) / 2;
    const uint32_t plate = 0xFF000000u | paper;

    for (int y = top > 0 ? top : 0; y < height; y++)
        for (int x = 0; x < width; x++)
            logical[(size_t) y * (size_t) width + (size_t) x] = plate;

    for (int y = 0; y < surface->h; y++) {
        const int dy = top + 2 + y;
        if (dy < 0 || dy >= height) continue;

        const uint32_t *src = (const uint32_t *) ((const uint8_t *) surface->pixels + (size_t) y * surface->pitch);
        for (int x = 0; x < surface->w; x++) {
            const int dx = left + x;
            if (dx < 0 || dx >= width) continue;

            const uint32_t s = src[x];
            const uint32_t alpha = s >> 24;
            if (!alpha) continue;

            uint32_t *d = &logical[(size_t) dy * (size_t) width + (size_t) dx];
            const uint32_t r = (((s >> 16) & 0xFF) * alpha + ((*d >> 16) & 0xFF) * (255 - alpha)) / 255;
            const uint32_t g = (((s >> 8) & 0xFF) * alpha + ((*d >> 8) & 0xFF) * (255 - alpha)) / 255;
            const uint32_t b = ((s & 0xFF) * alpha + (*d & 0xFF) * (255 - alpha)) / 255;
            *d = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }

    SDL_FreeSurface(surface);
}

static void present_logical(const uint32_t *source, const int width, const int height) {
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch) != 0) return;

    uint8_t *base = pixels;

    if (rotation == reader_rotation_none) {
        for (int y = 0; y < height; y++)
            memcpy(
                base + (size_t) y * (size_t) pitch, source + (size_t) y * (size_t) width,
                (size_t) width * sizeof(uint32_t)
            );
    } else {
        for (int ly = 0; ly < height; ly++) {
            const uint32_t *row = source + (size_t) ly * (size_t) width;
            for (int lx = 0; lx < width; lx++) {
                int sx;
                int sy;
                if (rotation == reader_rotation_right) {
                    sx = screen_w - 1 - ly;
                    sy = lx;
                } else if (rotation == reader_rotation_upside) {
                    sx = screen_w - 1 - lx;
                    sy = screen_h - 1 - ly;
                } else {
                    sx = ly;
                    sy = screen_h - 1 - lx;
                }
                ((uint32_t *) (base + (size_t) sy * (size_t) pitch))[sx] = row[lx];
            }
        }
    }

    SDL_UnlockTexture(texture);
}

void view_compose(
    const doc_image *image, const int scroll_x, const int scroll_y, const int theme, const int plain, const char *status
) {
    if (!texture || !logical) return;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);

    const view_palette palette = palettes[theme >= 0 && theme < reader_theme_count ? theme : 0];
    const uint32_t backdrop = plain ? VIEW_PLAIN_BG : palette.paper;
    anim_paper = 0xFF000000u | (plain ? 0xF2F0EAu : palette.paper);
    fill(backdrop, width, height);

    int area = height;
    if (status && status_font) area = height - view_status_height() > 1 ? height - view_status_height() : height;

    if (image && image->samples) {
        const int copy_w = image->width < width ? image->width : width;
        const int copy_h = image->height < area ? image->height : area;
        const int dst_x = image->width < width ? (width - image->width) / 2 : 0;
        const int dst_y = image->height < area ? (area - image->height) / 2 : 0;

        uint8_t lut_r[256];
        uint8_t lut_g[256];
        uint8_t lut_b[256];
        for (int v = 0; v < 256; v++) {
            lut_r[v] = plain ? (uint8_t) v : mix(palette.ink, palette.paper, 16, (unsigned char) v);
            lut_g[v] = plain ? (uint8_t) v : mix(palette.ink, palette.paper, 8, (unsigned char) v);
            lut_b[v] = plain ? (uint8_t) v : mix(palette.ink, palette.paper, 0, (unsigned char) v);
        }

        for (int y = 0; y < copy_h; y++) {
            const unsigned char *src = image->samples + (size_t) (scroll_y + y) * (size_t) image->stride
                                       + (size_t) scroll_x * (size_t) image->components;
            uint32_t *dst = logical + (size_t) (dst_y + y) * (size_t) width + (size_t) dst_x;
            for (int x = 0; x < copy_w; x++) {
                dst[x] =
                    0xFF000000u | ((uint32_t) lut_r[src[0]] << 16) | ((uint32_t) lut_g[src[1]] << 8) | lut_b[src[2]];
                src += image->components;
            }
        }
    }

    blend_status(status, width, height, plain ? VIEW_PLAIN_BG : palette.paper, plain ? 0xC0C0C0 : palette.ink);
    if (pending && transition != reader_transition_instant) {
        pending = 0;
        animating = 1;
        anim_start = SDL_GetTicks();
        anim_length = transition == reader_transition_page_turn ? 340
                      : transition == reader_transition_slide   ? 240
                                                                : 200;
        anim_w = width;
        anim_h = height;
        present_logical(previous, width, height);
        return;
    }

    pending = 0;
    animating = 0;
    present_logical(logical, width, height);
}

void view_draw(SDL_Renderer *renderer) {
    if (texture) display_render_logical_texture(renderer, texture);
}

void view_set_transition(const int value) {
    transition = value >= 0 && value < reader_transition_count ? value : reader_transition_instant;
}

void view_prepare_transition(const int direction) {
    if (!logical || transition == reader_transition_instant) return;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);

    const size_t pixels = (size_t) width * (size_t) height;
    memcpy(previous, animating ? frame : logical, pixels * sizeof(*previous));

    anim_direction = direction < 0 ? -1 : 1;
    pending = 1;
}

static uint32_t blend(const uint32_t from, const uint32_t to, const uint32_t amount) {
    const uint32_t keep = 256 - amount;
    const uint32_t rb = (((from & 0xFF00FFu) * keep + (to & 0xFF00FFu) * amount) >> 8) & 0xFF00FFu;
    const uint32_t g = (((from & 0x00FF00u) * keep + (to & 0x00FF00u) * amount) >> 8) & 0x00FF00u;
    return 0xFF000000u | rb | g;
}

static uint32_t shade(const uint32_t pixel, const uint32_t level) {
    return blend(pixel, 0xFF000000u, level);
}

static void crossfade(const uint32_t *from, const uint32_t *to, const double s, const int width, const int height) {
    const uint32_t amount = (uint32_t) (s * 256.0);
    const size_t count = (size_t) width * (size_t) height;
    for (size_t i = 0; i < count; i++)
        frame[i] = blend(from[i], to[i], amount);
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

    for (int y = 0; y < height; y++) {
        const size_t row = (size_t) y * (size_t) width;

        for (int x = 0; x < width; x++) {
            uint32_t pixel;

            if (x >= edge) {
                pixel = to[row + (size_t) x];
                const int near = x - edge;
                if (near < shadow) pixel = shade(pixel, (uint32_t) (110 * (shadow - near) / shadow));
            } else if (x >= flap_start && flap > 0) {
                int source = edge + (edge - x) * 2;
                if (source >= width) source = width - 1;
                const uint32_t back = blend(from[row + (size_t) source], anim_paper, 200);
                const int depth = edge - x;
                pixel = shade(back, (uint32_t) (60 * depth / flap));
            } else {
                pixel = from[row + (size_t) x];
                const int near = flap_start - x;
                if (flap > 0 && near >= 0 && near < shadow)
                    pixel = shade(pixel, (uint32_t) (70 * (shadow - near) / shadow));
            }

            frame[row + (size_t) x] = pixel;
        }
    }
}

int view_tick(void) {
    if (!animating) return 0;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);
    if (width != anim_w || height != anim_h) {
        animating = 0;
        present_logical(logical, width, height);
        return 1;
    }

    const uint32_t elapsed = SDL_GetTicks() - anim_start;
    if (elapsed >= anim_length) {
        animating = 0;
        present_logical(logical, width, height);
        return 1;
    }

    const double t = (double) elapsed / (double) anim_length;
    const double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    const int forward = anim_direction > 0;
    const uint32_t *from = forward ? previous : logical;
    const uint32_t *to = forward ? logical : previous;
    const double s = forward ? eased : 1.0 - eased;

    if (transition == reader_transition_crossfade)
        crossfade(previous, logical, eased, width, height);
    else if (transition == reader_transition_slide)
        slide(from, to, s, width, height);
    else
        page_turn(from, to, s, width, height);

    present_logical(frame, width, height);
    return 1;
}

int view_animating(void) {
    return animating;
}
