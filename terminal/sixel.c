#include <stdlib.h>
#include <string.h>
#include "sixel.h"

#define SIXEL_MAX_W 4096
#define SIXEL_MAX_H 4096

#define SIXEL_PALETTE 256
#define SIXEL_MAX_IMG 16

typedef struct {
    Uint32 *pixels;
    int w, h;
    int cell_x, cell_y;
} SixelImage;

static SixelImage g_imgs[SIXEL_MAX_IMG];
static int g_img_count = 0;
static int g_cell_h = 16;
static int g_cell_w = 8;

void sixel_decode(const char *data, size_t len, int cell_x, int cell_y) {
    Uint32 palette[SIXEL_PALETTE];
    memset(palette, 0, sizeof(palette));

    int img_w = 0;
    int cur_x = 0;
    int cur_y = 0;
    int max_y = 0;

    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        unsigned char c = (unsigned char) *p++;

        if (c == '#') {
            while (p < end && *p != '#' && (*p < 63 || *p > 126) && *p != '-' && *p != '!')
                p++;
            continue;
        }

        if (c == '!') {
            int rep = 0;
            while (p < end && *p >= '0' && *p <= '9')
                rep = rep * 10 + (*p++ - '0');

            if (p < end) p++;
            cur_x += rep;

            if (cur_x > img_w) img_w = cur_x;
            continue;
        }

        if (c == '$') {
            cur_x = 0;
            continue;
        }

        if (c == '-') {
            cur_x = 0;
            cur_y += 6;
            continue;
        }

        if (c >= 63 && c <= 126) {
            cur_x++;
            if (cur_x > img_w) img_w = cur_x;
            if (cur_y + 6 > max_y) max_y = cur_y + 6;
        }
    }

    int img_h = (max_y > 0) ? max_y : cur_y + 6;
    if (img_w <= 0 || img_h <= 0 || img_w > SIXEL_MAX_W || img_h > SIXEL_MAX_H) return;

    if (g_img_count >= SIXEL_MAX_IMG) {
        free(g_imgs[0].pixels);
        memmove(&g_imgs[0], &g_imgs[1], sizeof(SixelImage) * (SIXEL_MAX_IMG - 1));
        g_img_count = SIXEL_MAX_IMG - 1;
    }

    SixelImage *img = &g_imgs[g_img_count];

    img->pixels = realloc(img->pixels, (size_t) img_w * (size_t) img_h * sizeof(Uint32));
    if (!img->pixels) return;

    memset(img->pixels, 0, (size_t) img_w * (size_t) img_h * sizeof(Uint32));

    img->w = img_w;
    img->h = img_h;

    img->cell_x = cell_x;
    img->cell_y = cell_y;

    g_img_count++;

    int colour = 0;
    cur_x = 0;
    cur_y = 0;
    p = data;

    while (p < end) {
        unsigned char c = (unsigned char) *p++;

        if (c == '#') {
            int n = 0;
            while (p < end && *p >= '0' && *p <= '9')
                n = n * 10 + (*p++ - '0');

            if (n < 0) n = 0;
            if (n >= SIXEL_PALETTE) n = SIXEL_PALETTE - 1;

            colour = n;

            if (p < end && *p == ';') {
                p++;
                int mode = 0;
                while (p < end && *p >= '0' && *p <= '9')
                    mode = mode * 10 + (*p++ - '0');

                int a = 0, b = 0, cv = 0;

                if (p < end && *p == ';') {
                    p++;
                    while (p < end && *p >= '0' && *p <= '9')
                        a = a * 10 + (*p++ - '0');
                }

                if (p < end && *p == ';') {
                    p++;
                    while (p < end && *p >= '0' && *p <= '9')
                        b = b * 10 + (*p++ - '0');
                }

                if (p < end && *p == ';') {
                    p++;
                    while (p < end && *p >= '0' && *p <= '9')
                        cv = cv * 10 + (*p++ - '0');
                }

                if (mode == 2) {
                    Uint8 r = (Uint8) ((a * 255) / 100);
                    Uint8 g = (Uint8) ((b * 255) / 100);
                    Uint8 bl = (Uint8) ((cv * 255) / 100);

                    palette[n] = ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) bl << 8) | 0xFF;
                } else if (mode == 1) {
                    float h = (float) a / 360.0f;
                    float l = (float) b / 100.0f;
                    float s = (float) cv / 100.0f;

                    float r, g, bl;

                    if (s == 0.0f) {
                        r = g = bl = l;
                    } else {
                        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
                        float p2 = 2.0f * l - q;

                        float t[3] = {h + 1.0f / 3.0f, h, h - 1.0f / 3.0f};
                        float rgb[3];
                        for (int j = 0; j < 3; j++) {
                            if (t[j] < 0.0f) t[j] += 1.0f;
                            if (t[j] > 1.0f) t[j] -= 1.0f;

                            if (t[j] < 1.0f / 6.0f) {
                                rgb[j] = p2 + (q - p2) * 6.0f * t[j];
                            } else if (t[j] < 1.0f / 2.0f) {
                                rgb[j] = q;
                            } else if (t[j] < 2.0f / 3.0f) {
                                rgb[j] = p2 + (q - p2) * (2.0f / 3.0f - t[j]) * 6.0f;
                            } else {
                                rgb[j] = p2;
                            }
                        }
                        r = rgb[0];
                        g = rgb[1];

                        bl = rgb[2];
                    }

                    palette[n] =
                        ((Uint32) (r * 255) << 24) | ((Uint32) (g * 255) << 16) | ((Uint32) (bl * 255) << 8) | 0xFF;
                }
            }
            continue;
        }

        if (c == '!') {
            int rep = 0;
            while (p < end && *p >= '0' && *p <= '9')
                rep = rep * 10 + (*p++ - '0');

            if (p >= end) break;
            unsigned char sixel_byte = (unsigned char) *p++;

            if (sixel_byte < 63 || sixel_byte > 126) continue;

            Uint32 col = palette[colour];
            int bits = sixel_byte - 63;

            for (int rx = 0; rx < rep && (cur_x + rx) < img_w; rx++) {
                for (int bit = 0; bit < 6; bit++) {
                    if ((bits >> bit) & 1) {
                        int py = cur_y + bit;
                        if (py < img_h) img->pixels[py * img_w + cur_x + rx] = col;
                    }
                }
            }

            cur_x += rep;
            continue;
        }

        if (c == '$') {
            cur_x = 0;
            continue;
        }

        if (c == '-') {
            cur_x = 0;
            cur_y += 6;
            continue;
        }

        if (c >= 63 && c <= 126) {
            if (cur_x < img_w) {
                Uint32 col = palette[colour];
                int bits = c - 63;
                for (int bit = 0; bit < 6; bit++) {
                    if ((bits >> bit) & 1) {
                        int py = cur_y + bit;
                        if (py < img_h) img->pixels[py * img_w + cur_x] = col;
                    }
                }
            }

            cur_x++;
        }
    }
}

int sixel_count(void) {
    return g_img_count;
}

const Uint32 *sixel_get(int idx, int *out_x, int *out_y, int *out_w, int *out_h) {
    if (idx < 0 || idx >= g_img_count) return NULL;
    SixelImage *img = &g_imgs[idx];

    if (!img->pixels || img->w <= 0 || img->h <= 0) return NULL;

    *out_x = img->cell_x;
    *out_y = img->cell_y;

    *out_w = img->w;
    *out_h = img->h;

    return img->pixels;
}

const Uint32 *sixel_pixels(int *out_x, int *out_y, int *out_w, int *out_h) {
    return sixel_get(g_img_count - 1, out_x, out_y, out_w, out_h);
}

void sixel_free(void) {
    for (int i = 0; i < g_img_count; i++) {
        free(g_imgs[i].pixels);
        g_imgs[i].pixels = NULL;
    }

    g_img_count = 0;
}

void sixel_scroll(int lines) {
    int i = 0;
    while (i < g_img_count) {
        g_imgs[i].cell_y -= lines;
        int img_cell_h = (g_imgs[i].h + g_cell_h - 1) / g_cell_h;

        if (g_imgs[i].cell_y + img_cell_h <= 0) {
            free(g_imgs[i].pixels);
            g_imgs[i].pixels = NULL;

            memmove(&g_imgs[i], &g_imgs[i + 1], sizeof(SixelImage) * (size_t) (g_img_count - i - 1));
            g_img_count--;
        } else {
            i++;
        }
    }
}

void sixel_set_cell_h(int cell_h) {
    if (cell_h > 0) g_cell_h = cell_h;
}

int sixel_cell_h(void) {
    return g_cell_h;
}

void sixel_set_cell_w(int cell_w) {
    if (cell_w > 0) g_cell_w = cell_w;
}

int sixel_cell_w(void) {
    return g_cell_w;
}

void sixel_set_cell_y(int new_y) {
    if (g_img_count > 0) g_imgs[g_img_count - 1].cell_y = new_y;
}
