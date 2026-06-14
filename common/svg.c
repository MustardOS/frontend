#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../plutosvg/source/plutosvg.h"
#include "../plutosvg/plutovg/include/plutovg.h"
#include "../lvgl/lvgl.h"
#include "svg.h"
#include "theme.h"

#define SVG_PATH_MAX 4096

// Since we are sending the LVGL path and it is set to root
// we can simply strip the ':' character within the string
static const char *svg_real_path(const void *src) {
    const char *path = (const char *) src;
    return (path[1] == ':') ? path + 2 : path;
}

// Are we really who we say we are?
// Only claim genuine .svg file sources so the SVG decoder doesn't intercept
// PNG (or other) images, which would otherwise fail to render and show nothing!
static int svg_src_is_svg(const void *src) {
    if (lv_img_src_get_type(src) != LV_IMG_SRC_FILE) return 0;

    const char *path = svg_real_path(src);
    const char *q = strchr(path, '?');
    size_t len = q ? (size_t) (q - path) : strlen(path);

    return len >= 4 && strncasecmp(path + len - 4, ".svg", 4) == 0;
}

// Parse an SVG decoder string into a file path and optional hint dims.
// Handles "M:/path/to/file.svg" or "M:/path/to/file.svg?WxH"
// The file_path receives the path without the drive prefix and without
// any "?WxH" suffix and hint_w/hint_h are set from the "?WxH" suffix if
// found or it'll just return zero, better than global variables!
static void svg_parse_src(const void *src, char *file_path, int *hint_w, int *hint_h) {
    const char *path = svg_real_path(src);
    const char *q = strchr(path, '?');

    *hint_w = 0;
    *hint_h = 0;

    if (q) {
        size_t len = (size_t) (q - path);
        if (len >= SVG_PATH_MAX) len = SVG_PATH_MAX - 1;

        memcpy(file_path, path, len);
        file_path[len] = '\0';

        const char *p = q + 1;
        char *end;
        long w = strtol(p, &end, 10);

        if (end != p && *end == 'x') {
            p = end + 1;
            long h = strtol(p, &end, 10);
            if (end != p) {
                *hint_w = (int) w;
                *hint_h = (int) h;
            }
        }
    } else {
        snprintf(file_path, SVG_PATH_MAX, "%s", path);
    }
}

// Resolves the glyph render size.
// If hint_w/h are non-zero (i.e grid or carousel mode), scale the glyph proportionally
// to fit within that box. Otherwise uses the config/theme glyph size
// -1 = native SVG size, 0 = auto-fit to item height, positive = explicit px etc.
static void svg_target_size(int native_w, int native_h, int hint_w, int hint_h, int *out_w, int *out_h) {
    if (hint_w < 0 || hint_h < 0) {
        *out_w = native_w;
        *out_h = native_h;
        return;
    }

    if (hint_w > 0 || hint_h > 0) {
        if (native_w <= 0 || native_h <= 0) {
            *out_w = hint_w > 0 ? hint_w : native_w;
            *out_h = hint_h > 0 ? hint_h : native_h;
            return;
        }

        int target_w = hint_w > 0 ? hint_w : native_w;
        int target_h = hint_h > 0 ? hint_h : native_h;

        // Scale proportionally to fit within target box width and height
        int sw, sh;
        if ((long) native_w * target_h >= (long) native_h * target_w) {
            sw = target_w;
            sh = (native_h * target_w + native_w / 2) / native_w;
        } else {
            sh = target_h;
            sw = (native_w * target_h + native_h / 2) / native_h;
        }

        *out_w = (sw < 1) ? 1 : sw;
        *out_h = (sh < 1) ? 1 : sh;

        return;
    }

    int target = (int) theme.MUX.ITEM.HEIGHT * 3 / 4;
    if (target <= 0 || native_w <= 0 || native_h <= 0) {
        *out_w = native_w;
        *out_h = native_h;
        return;
    }

    // Scale proportionally, fitting the longest dimension to target
    int sw, sh;
    if (native_w >= native_h) {
        sw = target;
        sh = (native_h * target + native_w / 2) / native_w;
    } else {
        sh = target;
        sw = (native_w * target + native_h / 2) / native_h;
    }

    *out_w = (sw < 1) ? 1 : sw;
    *out_h = (sh < 1) ? 1 : sh;
}

static lv_res_t svg_info_cb(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header) {
    LV_UNUSED(decoder);

    if (!svg_src_is_svg(src)) return LV_RES_INV;

    char file_path[SVG_PATH_MAX];
    int hint_w, hint_h;
    svg_parse_src(src, file_path, &hint_w, &hint_h);

    plutosvg_document_t *doc = plutosvg_document_load_from_file(file_path, -1, -1);
    if (!doc) return LV_RES_INV;

    int native_w = (int) plutosvg_document_get_width(doc);
    int native_h = (int) plutosvg_document_get_height(doc);
    plutosvg_document_destroy(doc);

    if (native_w <= 0 || native_h <= 0) return LV_RES_INV;

    int w, h;
    svg_target_size(native_w, native_h, hint_w, hint_h, &w, &h);

    header->always_zero = 0;
    header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    header->w = (lv_coord_t) w;
    header->h = (lv_coord_t) h;

    return LV_RES_OK;
}

static lv_res_t svg_open_cb(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    LV_UNUSED(decoder);

    if (!svg_src_is_svg(dsc->src)) return LV_RES_INV;

    char file_path[SVG_PATH_MAX];
    int hint_w, hint_h;
    svg_parse_src(dsc->src, file_path, &hint_w, &hint_h);

    plutosvg_document_t *doc = plutosvg_document_load_from_file(file_path, -1, -1);
    if (!doc) return LV_RES_INV;

    int native_w = (int) plutosvg_document_get_width(doc);
    int native_h = (int) plutosvg_document_get_height(doc);

    if (native_w <= 0 || native_h <= 0) {
        plutosvg_document_destroy(doc);
        return LV_RES_INV;
    }

    int w, h;
    svg_target_size(native_w, native_h, hint_w, hint_h, &w, &h);

    plutovg_surface_t *surface = plutosvg_document_render_to_surface(doc, NULL, w, h, NULL, NULL, NULL);
    plutosvg_document_destroy(doc);
    if (!surface) return LV_RES_INV;

    int stride = plutovg_surface_get_stride(surface);
    const uint8_t *pixels = plutovg_surface_get_data(surface);

    uint8_t *buf = lv_mem_alloc((size_t) w * h * 4);
    if (!buf) {
        plutovg_surface_destroy(surface);
        return LV_RES_INV;
    }

    // PlutoVG uses premultiplied BGRA so we'll just reverse it for LVGL
    for (int y = 0; y < h; y++) {
        const uint8_t *row = pixels + y * stride;
        uint8_t *dst = buf + y * w * 4;

        for (int x = 0; x < w; x++) {
            uint8_t b = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2], a = row[x * 4 + 3];

            if (a > 0 && a < 255) {
                b = (uint8_t) ((b * 255u + a / 2u) / a);
                g = (uint8_t) ((g * 255u + a / 2u) / a);
                r = (uint8_t) ((r * 255u + a / 2u) / a);
            }

            dst[x * 4] = b;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = r;
            dst[x * 4 + 3] = a;
        }
    }

    plutovg_surface_destroy(surface);
    dsc->img_data = buf;

    return LV_RES_OK;
}

static void svg_close_cb(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    LV_UNUSED(decoder);

    if (dsc->img_data) {
        lv_mem_free((void *) dsc->img_data);
        dsc->img_data = NULL;
    }
}

void svg_init(void) {
    lv_img_decoder_t *dec = lv_img_decoder_create();
    if (!dec) return;

    lv_img_decoder_set_info_cb(dec, svg_info_cb);
    lv_img_decoder_set_open_cb(dec, svg_open_cb);
    lv_img_decoder_set_close_cb(dec, svg_close_cb);
}
