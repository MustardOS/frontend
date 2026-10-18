#include <stddef.h>
#include <stdint.h>
#include "scale2x.h"

#define SCALE2_X_IMPL(NAME, TYPE)                                                                                      \
    static inline void NAME##_px(                                                                                      \
        const TYPE b, const TYPE d, const TYPE e, const TYPE f, const TYPE h, TYPE *out0, TYPE *out1                   \
    ) {                                                                                                                \
        TYPE e0 = e, e1 = e, e2 = e, e3 = e;                                                                           \
        if (b != h && d != f) {                                                                                        \
            e0 = (d == b) ? d : e;                                                                                     \
            e1 = (b == f) ? f : e;                                                                                     \
            e2 = (d == h) ? d : e;                                                                                     \
            e3 = (h == f) ? f : e;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        out0[0] = e0;                                                                                                  \
        out0[1] = e1;                                                                                                  \
        out1[0] = e2;                                                                                                  \
        out1[1] = e3;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    void NAME(                                                                                                         \
        const TYPE *src, TYPE *dst, const int width, const int height, const int src_pitch_px, const int dst_pitch_px  \
    ) {                                                                                                                \
        for (int y = 0; y < height; y++) {                                                                             \
            const TYPE *row = src + (size_t) y * src_pitch_px;                                                         \
            const TYPE *row_up = src + (size_t) (y > 0 ? y - 1 : y) * src_pitch_px;                                    \
            const TYPE *row_down = src + (size_t) (y < height - 1 ? y + 1 : y) * src_pitch_px;                         \
            TYPE *out0 = dst + (size_t) (y * 2) * dst_pitch_px;                                                        \
            TYPE *out1 = dst + (size_t) (y * 2 + 1) * dst_pitch_px;                                                    \
                                                                                                                       \
            NAME##_px(row_up[0], row[0], row[0], row[width > 1 ? 1 : 0], row_down[0], out0, out1);                     \
                                                                                                                       \
            for (int x = 1; x + 1 < width; x++)                                                                        \
                NAME##_px(row_up[x], row[x - 1], row[x], row[x + 1], row_down[x], out0 + x * 2, out1 + x * 2);         \
                                                                                                                       \
            if (width > 1) {                                                                                           \
                const int x = width - 1;                                                                               \
                NAME##_px(row_up[x], row[x - 1], row[x], row[x], row_down[x], out0 + x * 2, out1 + x * 2);             \
            }                                                                                                          \
        }                                                                                                              \
    }

SCALE2_X_IMPL(scale2_x_16, uint16_t)
SCALE2_X_IMPL(scale2_x_32, uint32_t)

#define SCALE3_X_IMPL(NAME, TYPE)                                                                                      \
    static inline void NAME##_px(                                                                                      \
        const TYPE a, const TYPE b, const TYPE c, const TYPE d, const TYPE e, const TYPE f, const TYPE g,              \
        const TYPE h, const TYPE ii, TYPE *out0, TYPE *out1, TYPE *out2                                                \
    ) {                                                                                                                \
        TYPE e0 = e, e1 = e, e2 = e, e3 = e, e5 = e, e6 = e, e7 = e, e8 = e;                                           \
        if (b != h && d != f) {                                                                                        \
            e0 = (d == b) ? d : e;                                                                                     \
            e1 = ((d == b && e != c) || (b == f && e != a)) ? b : e;                                                   \
            e2 = (b == f) ? f : e;                                                                                     \
            e3 = ((d == b && e != g) || (d == h && e != a)) ? d : e;                                                   \
            e5 = ((b == f && e != ii) || (h == f && e != c)) ? f : e;                                                  \
            e6 = (d == h) ? d : e;                                                                                     \
            e7 = ((d == h && e != ii) || (h == f && e != g)) ? h : e;                                                  \
            e8 = (h == f) ? f : e;                                                                                     \
        }                                                                                                              \
                                                                                                                       \
        out0[0] = e0;                                                                                                  \
        out0[1] = e1;                                                                                                  \
        out0[2] = e2;                                                                                                  \
        out1[0] = e3;                                                                                                  \
        out1[1] = e;                                                                                                   \
        out1[2] = e5;                                                                                                  \
        out2[0] = e6;                                                                                                  \
        out2[1] = e7;                                                                                                  \
        out2[2] = e8;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    void NAME(                                                                                                         \
        const TYPE *src, TYPE *dst, const int width, const int height, const int src_pitch_px, const int dst_pitch_px  \
    ) {                                                                                                                \
        for (int y = 0; y < height; y++) {                                                                             \
            const TYPE *row = src + (size_t) y * src_pitch_px;                                                         \
            const TYPE *row_up = src + (size_t) (y > 0 ? y - 1 : y) * src_pitch_px;                                    \
            const TYPE *row_down = src + (size_t) (y < height - 1 ? y + 1 : y) * src_pitch_px;                         \
            TYPE *out0 = dst + (size_t) (y * 3) * dst_pitch_px;                                                        \
            TYPE *out1 = dst + (size_t) (y * 3 + 1) * dst_pitch_px;                                                    \
            TYPE *out2 = dst + (size_t) (y * 3 + 2) * dst_pitch_px;                                                    \
                                                                                                                       \
            {                                                                                                          \
                const int xr = width > 1 ? 1 : 0;                                                                      \
                NAME##_px(                                                                                             \
                    row_up[0], row_up[0], row_up[xr], row[0], row[0], row[xr], row_down[0], row_down[0], row_down[xr], \
                    out0, out1, out2                                                                                   \
                );                                                                                                     \
            }                                                                                                          \
                                                                                                                       \
            for (int x = 1; x + 1 < width; x++)                                                                        \
                NAME##_px(                                                                                             \
                    row_up[x - 1], row_up[x], row_up[x + 1], row[x - 1], row[x], row[x + 1], row_down[x - 1],          \
                    row_down[x], row_down[x + 1], out0 + x * 3, out1 + x * 3, out2 + x * 3                             \
                );                                                                                                     \
                                                                                                                       \
            if (width > 1) {                                                                                           \
                const int x = width - 1;                                                                               \
                NAME##_px(                                                                                             \
                    row_up[x - 1], row_up[x], row_up[x], row[x - 1], row[x], row[x], row_down[x - 1], row_down[x],     \
                    row_down[x], out0 + x * 3, out1 + x * 3, out2 + x * 3                                              \
                );                                                                                                     \
            }                                                                                                          \
        }                                                                                                              \
    }

SCALE3_X_IMPL(scale3_x_16, uint16_t)
SCALE3_X_IMPL(scale3_x_32, uint32_t)
