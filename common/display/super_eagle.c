#include <stddef.h>
#include <stdint.h>
#include "super_eagle.h"

static void split3_rgb565(const uint16_t p, int *r, int *g, int *b) {
    *r = (p >> 11) & 0x1F;
    *g = (p >> 5) & 0x3F;
    *b = p & 0x1F;
}

static uint16_t merge3_rgb565(const int r, const int g, const int b) {
    return (uint16_t) (((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

static void split3_argb1555(const uint16_t p, int *r, int *g, int *b) {
    *r = (p >> 10) & 0x1F;
    *g = (p >> 5) & 0x1F;
    *b = p & 0x1F;
}

static uint16_t merge3_argb1555(const int r, const int g, const int b) {
    return (uint16_t) (((r & 0x1F) << 10) | ((g & 0x1F) << 5) | (b & 0x1F));
}

static void split3_argb8888(const uint32_t p, int *r, int *g, int *b) {
    *r = (int) ((p >> 16) & 0xFF);
    *g = (int) ((p >> 8) & 0xFF);
    *b = (int) (p & 0xFF);
}

static uint32_t merge3_argb8888(const int r, const int g, const int b) {
    return 0xFF000000u | ((uint32_t) (r & 0xFF) << 16) | ((uint32_t) (g & 0xFF) << 8) | (uint32_t) (b & 0xFF);
}

#define SUPER_EAGLE_IMPL(NAME, TYPE, SPLIT, MERGE)                                                                     \
    static inline TYPE NAME##_interp2(const TYPE a, const TYPE b) {                                                    \
        int ra, ga, ba, rb, gb, bb;                                                                                    \
        SPLIT(a, &ra, &ga, &ba);                                                                                       \
        SPLIT(b, &rb, &gb, &bb);                                                                                       \
        return MERGE((ra + rb) >> 1, (ga + gb) >> 1, (ba + bb) >> 1);                                                  \
    }                                                                                                                  \
    static inline TYPE NAME##_interp4(const TYPE a, const TYPE b, const TYPE c, const TYPE d) {                        \
        int ra, ga, ba, rb, gb, bb, rc, gc, bc, rd, gd, bd;                                                            \
        SPLIT(a, &ra, &ga, &ba);                                                                                       \
        SPLIT(b, &rb, &gb, &bb);                                                                                       \
        SPLIT(c, &rc, &gc, &bc);                                                                                       \
        SPLIT(d, &rd, &gd, &bd);                                                                                       \
        return MERGE((ra + rb + rc + rd) >> 2, (ga + gb + gc + gd) >> 2, (ba + bb + bc + bd) >> 2);                    \
    }                                                                                                                  \
    static inline TYPE NAME##_interpo(const TYPE a, const TYPE b, const TYPE c) {                                      \
        int ra, ga, ba, rb, gb, bb, rc, gc, bc;                                                                        \
        SPLIT(a, &ra, &ga, &ba);                                                                                       \
        SPLIT(b, &rb, &gb, &bb);                                                                                       \
        SPLIT(c, &rc, &gc, &bc);                                                                                       \
        return MERGE(                                                                                                  \
            ((ra << 2) + (ra << 1) + rb + rc) >> 3, ((ga << 2) + (ga << 1) + gb + gc) >> 3,                            \
            ((ba << 2) + (ba << 1) + bb + bc) >> 3                                                                     \
        );                                                                                                             \
    }                                                                                                                  \
    static inline int NAME##_result1(const TYPE A, const TYPE B, const TYPE C, const TYPE D) {                         \
        int x = 0, y = 0, r = 0;                                                                                       \
        if (A == C)                                                                                                    \
            x += 1;                                                                                                    \
        else if (B == C)                                                                                               \
            y += 1;                                                                                                    \
        if (A == D)                                                                                                    \
            x += 1;                                                                                                    \
        else if (B == D)                                                                                               \
            y += 1;                                                                                                    \
        if (x <= 1) r += 1;                                                                                            \
        if (y <= 1) r -= 1;                                                                                            \
        return r;                                                                                                      \
    }                                                                                                                  \
    static inline int NAME##_result2(const TYPE A, const TYPE B, const TYPE C, const TYPE D) {                         \
        int x = 0, y = 0, r = 0;                                                                                       \
        if (A == C)                                                                                                    \
            x += 1;                                                                                                    \
        else if (B == C)                                                                                               \
            y += 1;                                                                                                    \
        if (A == D)                                                                                                    \
            x += 1;                                                                                                    \
        else if (B == D)                                                                                               \
            y += 1;                                                                                                    \
        if (x <= 1) r -= 1;                                                                                            \
        if (y <= 1) r += 1;                                                                                            \
        return r;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline void NAME##_px(                                                                                      \
        const TYPE color4, const TYPE color5, const TYPE color6, const TYPE color1, const TYPE color2,                 \
        const TYPE color3, const TYPE colorB1, const TYPE colorB2, const TYPE colorS1, const TYPE colorS2,             \
        const TYPE colorA1, const TYPE colorA2, TYPE *out0, TYPE *out1                                                 \
    ) {                                                                                                                \
        TYPE product1a, product1b, product2a, product2b;                                                               \
                                                                                                                       \
        if (color2 == color6 && color5 != color3) {                                                                    \
            product2a = color2;                                                                                        \
            product1b = product2a;                                                                                     \
                                                                                                                       \
            product1a = (color1 == color2 || color6 == colorB2) ? NAME##_interp4(color2, color2, color2, color5)       \
                                                                : NAME##_interp2(color6, color5);                      \
            product2b = (color6 == colorS2 || color2 == colorA1) ? NAME##_interp4(color2, color2, color2, color3)      \
                                                                 : NAME##_interp2(color2, color3);                     \
        } else if (color5 == color3 && color2 != color6) {                                                             \
            product1a = color5;                                                                                        \
            product2b = product1a;                                                                                     \
                                                                                                                       \
            product1b = (colorB1 == color5 || color3 == colorS1) ? NAME##_interp4(color5, color5, color5, color6)      \
                                                                 : NAME##_interp2(color5, color6);                     \
            product2a = (color3 == colorA2 || color4 == color5) ? NAME##_interp4(color2, color5, color5, color5)       \
                                                                : NAME##_interp2(color3, color2);                      \
        } else if (color5 == color3 && color2 == color6) {                                                             \
            const int r =                                                                                              \
                NAME##_result1(color5, color6, color4, colorB1) + NAME##_result2(color6, color5, colorA2, colorS1)     \
                + NAME##_result2(color6, color5, color1, colorA1) + NAME##_result1(color5, color6, colorB2, colorS2);  \
                                                                                                                       \
            if (r > 0) {                                                                                               \
                product2a = color2;                                                                                    \
                product1b = product2a;                                                                                 \
                product1a = NAME##_interp2(color5, color6);                                                            \
                product2b = product1a;                                                                                 \
            } else if (r < 0) {                                                                                        \
                product1a = color5;                                                                                    \
                product2b = product1a;                                                                                 \
                product1b = NAME##_interp2(color5, color6);                                                            \
                product2a = product1b;                                                                                 \
            } else {                                                                                                   \
                product1a = color5;                                                                                    \
                product2b = product1a;                                                                                 \
                product2a = color2;                                                                                    \
                product1b = product2a;                                                                                 \
            }                                                                                                          \
        } else {                                                                                                       \
            product2b = NAME##_interpo(color3, color2, color6);                                                        \
            product1a = NAME##_interpo(color5, color6, color2);                                                        \
            product2a = NAME##_interpo(color2, color5, color3);                                                        \
            product1b = NAME##_interpo(color6, color5, color3);                                                        \
        }                                                                                                              \
                                                                                                                       \
        out0[0] = product1a;                                                                                           \
        out0[1] = product1b;                                                                                           \
        out1[0] = product2a;                                                                                           \
        out1[1] = product2b;                                                                                           \
    }                                                                                                                  \
                                                                                                                       \
    void NAME(                                                                                                         \
        const TYPE *src, TYPE *dst, const int width, const int height, const int src_pitch_px, const int dst_pitch_px  \
    ) {                                                                                                                \
        for (int y = 0; y < height; y++) {                                                                             \
            const TYPE *row = src + (size_t) y * src_pitch_px;                                                         \
            const TYPE *row_prev = y == 0 ? row : row - src_pitch_px;                                                  \
            const TYPE *row_next1 = y >= height - 1 ? row : row + src_pitch_px;                                        \
            const TYPE *row_next2 = y >= height - 2 ? row_next1 : row_next1 + src_pitch_px;                            \
            TYPE *out0 = dst + (size_t) (y * 2) * dst_pitch_px;                                                        \
            TYPE *out1 = dst + (size_t) (y * 2 + 1) * dst_pitch_px;                                                    \
                                                                                                                       \
            for (int x = 0; x < width; x++) {                                                                          \
                const int xm1 = x == 0 ? x : x - 1;                                                                    \
                const int xp1 = x >= width - 1 ? x : x + 1;                                                            \
                const int xp2 = x >= width - 2 ? xp1 : x + 2;                                                          \
                                                                                                                       \
                NAME##_px(                                                                                             \
                    row[xm1], row[x], row[xp1], row_next1[xm1], row_next1[x], row_next1[xp1], row_prev[x],             \
                    row_prev[xp1], row_next1[xp2], row[xp2], row_next2[x], row_next2[xp1], out0 + x * 2, out1 + x * 2  \
                );                                                                                                     \
            }                                                                                                          \
        }                                                                                                              \
    }

SUPER_EAGLE_IMPL(super_eagle_16_565, uint16_t, split3_rgb565, merge3_rgb565)
SUPER_EAGLE_IMPL(super_eagle_16_1555, uint16_t, split3_argb1555, merge3_argb1555)
SUPER_EAGLE_IMPL(super_eagle_32, uint32_t, split3_argb8888, merge3_argb8888)
