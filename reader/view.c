#include <math.h>
#include <stdio.h>
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
#define VIEW_PLAIN_PAPER 0xF2F0EA
#define VIEW_PLAIN_INK   0xC0C0C0
#define VIEW_TILE        32
#define VIEW_TEXT_MAX    256
#define VIEW_ELLIPSIS    "\xE2\x80\xA6"

typedef struct {
    uint32_t paper;
    uint32_t ink;
} view_palette;

typedef struct {
    int low;
    int high;
    float gamma;
} view_curve;

typedef enum {
    text_left,
    text_centre,
    text_right,
    text_top_left,
    text_top_right,
    text_count,
} text_slot_id;

typedef struct {
    char text[VIEW_TEXT_MAX];
    uint32_t ink;
    int limit;
    SDL_Surface *surface;
} text_slot;

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

/* Contrast stretches the levels so grey paper turns white and grey ink turns black,
 * then darkens the mid tones a little so thin and faint text gets heavier */
static const view_curve curves[reader_contrast_count] = {
    {0, 255, 1.0f},
    {16, 235, 1.3f},
    {40, 215, 1.6f},
};

static SDL_Texture *texture = NULL;
static SDL_Texture *texture_prev = NULL;
static TTF_Font *status_font = NULL;
static uint32_t *logical = NULL;
static uint32_t *previous = NULL;
static uint32_t *frame = NULL;
static uint8_t *turn_kinds = NULL;
static uint32_t *turn_levels = NULL;
static int *turn_sources = NULL;
static text_slot texts[text_count];
static uint8_t curve_table[reader_contrast_count][256];
static int curves_ready = 0;
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

static void build_curves(void) {
    if (curves_ready) return;
    curves_ready = 1;

    for (int c = 0; c < reader_contrast_count; c++) {
        const view_curve curve = curves[c];
        for (int v = 0; v < 256; v++) {
            float x = (float) (v - curve.low) / (float) (curve.high - curve.low);
            if (x < 0.0f) x = 0.0f;
            if (x > 1.0f) x = 1.0f;
            if (curve.gamma != 1.0f) x = powf(x, curve.gamma);
            curve_table[c][v] = (uint8_t) lrintf(x * 255.0f);
        }
    }
}

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

    build_curves();

    if (!TTF_WasInit() && TTF_Init() != 0) LOG_WARN(mux_module, "Chutney: TTF_Init failed: %s", TTF_GetError());

    const int short_side = width < height ? width : height;
    int size = short_side / 30;
    if (size < 12) size = 12;
    status_font = TTF_OpenFont(VIEW_STATUS_FONT, size);
    if (!status_font) LOG_WARN(mux_module, "Chutney: no status font: %s", TTF_GetError());

    return 1;
}

static void forget_texts(void) {
    for (int i = 0; i < text_count; i++) {
        if (texts[i].surface) SDL_FreeSurface(texts[i].surface);
        texts[i] = (text_slot) {0};
    }
}

void view_free(void) {
    forget_texts();
    if (texture) SDL_DestroyTexture(texture);
    if (texture_prev) SDL_DestroyTexture(texture_prev);
    if (status_font) TTF_CloseFont(status_font);
    free(logical);
    free(previous);
    free(frame);
    free(turn_kinds);
    free(turn_levels);
    free(turn_sources);

    texture = NULL;
    texture_prev = NULL;
    status_font = NULL;
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

int view_status_height(void) {
    return status_font ? TTF_FontLineSkip(status_font) + 4 : 0;
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

static int text_width(const char *text) {
    int w = 0;
    int h = 0;
    return TTF_SizeUTF8(status_font, text, &w, &h) == 0 ? w : 0;
}

/* Cuts the text on a character boundary and adds an ellipsis so it fits in the limit */
static void fit_text(const char *text, const int limit, char *out, const size_t size) {
    snprintf(out, size, "%s", text);
    if (limit <= 0 || text_width(out) <= limit) return;

    int bounds[VIEW_TEXT_MAX];
    int count = 0;
    for (int i = 1; out[i - 1] && i < (int) size && count < VIEW_TEXT_MAX; i++)
        if ((out[i] & 0xC0) != 0x80) bounds[count++] = i;

    char trial[VIEW_TEXT_MAX + 4];
    int low = 0;
    int high = count - 1;
    int best = -1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        snprintf(trial, sizeof(trial), "%.*s" VIEW_ELLIPSIS, bounds[mid], out);
        if (text_width(trial) <= limit) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (best < 0) {
        out[0] = '\0';
        return;
    }
    snprintf(trial, sizeof(trial), "%.*s" VIEW_ELLIPSIS, bounds[best], out);
    snprintf(out, size, "%s", trial);
}

/* The status band is drawn on every scroll step, so each piece of text is rendered once and kept */
static SDL_Surface *status_text(const text_slot_id id, const char *text, const uint32_t ink, const int limit) {
    text_slot *slot = &texts[id];
    if (!text || !text[0] || !status_font) return NULL;
    if (slot->surface && slot->ink == ink && slot->limit == limit && strcmp(slot->text, text) == 0)
        return slot->surface;

    if (slot->surface) SDL_FreeSurface(slot->surface);
    slot->surface = NULL;
    snprintf(slot->text, sizeof(slot->text), "%s", text);
    slot->ink = ink;
    slot->limit = limit;

    char fitted[VIEW_TEXT_MAX];
    fit_text(text, limit, fitted, sizeof(fitted));
    if (!fitted[0]) return NULL;

    const SDL_Color colour = {(ink >> 16) & 0xFF, (ink >> 8) & 0xFF, ink & 0xFF, 255};
    SDL_Surface *rendered = TTF_RenderUTF8_Blended(status_font, fitted, colour);
    if (!rendered) return NULL;

    slot->surface = SDL_ConvertSurfaceFormat(rendered, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(rendered);
    return slot->surface;
}

static void blend_text(const SDL_Surface *surface, const int left, const int top, const int width, const int height) {
    if (!surface) return;

    for (int y = 0; y < surface->h; y++) {
        const int dy = top + y;
        if (dy < 0 || dy >= height) continue;

        const uint32_t *src = (const uint32_t *) ((const uint8_t *) surface->pixels + (size_t) y * surface->pitch);
        uint32_t *row = logical + (size_t) dy * (size_t) width;
        for (int x = 0; x < surface->w; x++) {
            const int dx = left + x;
            if (dx < 0 || dx >= width) continue;

            const uint32_t s = src[x];
            const uint32_t alpha = s >> 24;
            if (!alpha) continue;

            uint32_t *d = &row[dx];
            const uint32_t r = (((s >> 16) & 0xFF) * alpha + ((*d >> 16) & 0xFF) * (255 - alpha)) / 255;
            const uint32_t g = (((s >> 8) & 0xFF) * alpha + ((*d >> 8) & 0xFF) * (255 - alpha)) / 255;
            const uint32_t b = ((s & 0xFF) * alpha + (*d & 0xFF) * (255 - alpha)) / 255;
            *d = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

/* One band of text: an optional centre piece, then left and right pieces sharing what room is left.
 * Without a centre piece the right one is placed first, so only the left one is ever shortened */
static void blend_band(
    const int top, const int width, const int height, const uint32_t ink, const text_slot_id left_id,
    const char *left_text, const text_slot_id centre_id, const char *centre_text, const text_slot_id right_id,
    const char *right_text
) {
    const int band = view_status_height();
    const int pad = band / 2;
    const int y = top + 2;

    const SDL_Surface *centre = centre_text ? status_text(centre_id, centre_text, ink, width - pad * 2) : NULL;
    if (centre) {
        const int centre_w = centre->w;
        blend_text(centre, (width - centre_w) / 2, y, width, height);

        const int side = (width - centre_w) / 2 - pad * 2;
        if (side <= 0) return;

        const SDL_Surface *right = status_text(right_id, right_text, ink, side);
        if (right) blend_text(right, width - pad - right->w, y, width, height);

        const SDL_Surface *left = status_text(left_id, left_text, ink, side);
        if (left) blend_text(left, pad, y, width, height);
        return;
    }

    const SDL_Surface *right = status_text(right_id, right_text, ink, width - pad * 2);
    const int right_w = right ? right->w : 0;
    if (right) blend_text(right, width - pad - right_w, y, width, height);

    const int room = width - pad * 2 - (right_w ? right_w + pad * 2 : 0);
    if (room <= 0) return;

    const SDL_Surface *left = status_text(left_id, left_text, ink, room);
    if (left) blend_text(left, pad, y, width, height);
}

static void
blend_status(const view_status *status, const int width, const int height, const uint32_t paper, const uint32_t ink) {
    if (!status || !status_font) return;

    const int band = view_status_height();
    const int bottom = height - band > 0 ? height - band : 0;
    fill_rows(paper, width, bottom, height);
    blend_band(
        bottom, width, height, ink, text_left, status->left, text_centre, status->centre, text_right, status->right
    );

    if (!status->top) return;

    fill_rows(paper, width, 0, band < height ? band : height);
    blend_band(
        0, width, height, ink, text_top_left, status->top_left, text_count, NULL, text_top_right, status->top_right
    );
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

static void
build_luts(const view_palette palette, const int plain, const int contrast, uint8_t *r, uint8_t *g, uint8_t *b) {
    const uint8_t *curve = curve_table[contrast >= 0 && contrast < reader_contrast_count ? contrast : 0];
    for (int v = 0; v < 256; v++) {
        const uint8_t c = curve[v];
        r[v] = plain ? c : mix(palette.ink, palette.paper, 16, c);
        g[v] = plain ? c : mix(palette.ink, palette.paper, 8, c);
        b[v] = plain ? c : mix(palette.ink, palette.paper, 0, c);
    }
}

static int luts_identity(const uint8_t *r, const uint8_t *g, const uint8_t *b) {
    for (int v = 0; v < 256; v++)
        if (r[v] != v || g[v] != v || b[v] != v) return 0;
    return 1;
}

void view_compose(
    const doc_image *image, const int scroll_x, const int scroll_y, const int theme, const int plain,
    const int contrast, const view_status *status
) {
    if (!texture || !logical) return;

    int width = 0;
    int height = 0;
    view_logical_size(&width, &height);

    const view_palette palette = palettes[theme >= 0 && theme < reader_theme_count ? theme : 0];
    const uint32_t backdrop = plain ? VIEW_PLAIN_BG : palette.paper;
    anim_paper = 0xFF000000u | (plain ? VIEW_PLAIN_PAPER : palette.paper);

    const int band = status && status_font ? view_status_height() : 0;
    const int top = band && status->top ? band : 0;
    const int area = height - band - top > 1 ? height - band - top : height;

    int copy_w = 0;
    int copy_h = 0;
    int dst_x = 0;
    int dst_y = 0;
    if (image && image->samples) {
        copy_w = image->width < width ? image->width : width;
        copy_h = image->height < area ? image->height : area;
        dst_x = image->width < width ? (width - image->width) / 2 : 0;
        dst_y = top + (image->height < area ? (area - image->height) / 2 : 0);
    }

    /* Only the strips around the page need the backdrop, the page itself overwrites the rest */
    fill_rows(backdrop, width, 0, dst_y);
    fill_rows(backdrop, width, dst_y + copy_h, height);

    if (copy_w > 0 && copy_h > 0) {
        uint8_t lut_r[256];
        uint8_t lut_g[256];
        uint8_t lut_b[256];
        build_luts(palette, plain, contrast, lut_r, lut_g, lut_b);
        const int identity = luts_identity(lut_r, lut_g, lut_b);
        const uint32_t edge = 0xFF000000u | backdrop;
        const int components = image->components;

        for (int y = 0; y < copy_h; y++) {
            const unsigned char *src = image->samples + (size_t) (scroll_y + y) * (size_t) image->stride
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
            } else {
                for (int x = 0; x < copy_w; x++, src += components)
                    dst[x] = 0xFF000000u | ((uint32_t) lut_r[src[0]] << 16) | ((uint32_t) lut_g[src[1]] << 8)
                             | lut_b[src[2]];
            }
        }
    }

    blend_status(status, width, height, backdrop, plain ? VIEW_PLAIN_INK : palette.ink);

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
