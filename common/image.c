#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../lvgl/lvgl.h"
#include "image.h"

static lv_res_t decoder_info(struct _lv_img_decoder_t *, const void *, lv_img_header_t *);

static lv_res_t decoder_open(lv_img_decoder_t *, lv_img_decoder_dsc_t *);

static void decoder_close(lv_img_decoder_t *, lv_img_decoder_dsc_t *);

static const uint8_t png_sig[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};

static const char *image_exts[] = {"svg", "png", "jpg", "jpeg", "webp", "qoi", "tga", "gif", "bmp", "pcx"};
static const int image_exts_n = sizeof(image_exts) / sizeof(image_exts[0]);

int is_image_ext(const char *ext) {
    for (int i = 0; i < image_exts_n; i++) {
        if (strcmp(ext, image_exts[i]) == 0) return 1;
    }
    return 0;
}

const char **image_ext_list(int *count) {
    *count = image_exts_n;
    return image_exts;
}

void img_init(void) {
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);

    lv_img_decoder_t *dec = lv_img_decoder_create();
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
}

static lv_res_t dims_png(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 24 || memcmp(d, png_sig, 8) != 0) return LV_RES_INV;

    *w = (lv_coord_t) ((uint32_t) d[16] << 24 | (uint32_t) d[17] << 16 | (uint32_t) d[18] << 8 | d[19]);
    *h = (lv_coord_t) ((uint32_t) d[20] << 24 | (uint32_t) d[21] << 16 | (uint32_t) d[22] << 8 | d[23]);

    return *w && *h ? LV_RES_OK : LV_RES_INV;
}

static lv_res_t dims_gif(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 10 || memcmp(d, "GIF8", 4) != 0) return LV_RES_INV;

    *w = (uint16_t) d[6] | (uint16_t) d[7] << 8;
    *h = (uint16_t) d[8] | (uint16_t) d[9] << 8;

    return *w && *h ? LV_RES_OK : LV_RES_INV;
}

static lv_res_t dims_bmp(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 26 || d[0] != 'B' || d[1] != 'M') return LV_RES_INV;

    const int32_t bw =
        (int32_t) ((uint32_t) d[18] | (uint32_t) d[19] << 8 | (uint32_t) d[20] << 16 | (uint32_t) d[21] << 24);
    const int32_t bh =
        (int32_t) ((uint32_t) d[22] | (uint32_t) d[23] << 8 | (uint32_t) d[24] << 16 | (uint32_t) d[25] << 24);

    if (bw <= 0 || bh == 0) return LV_RES_INV;

    *w = (lv_coord_t) bw;
    *h = bh < 0 ? -bh : bh;

    return LV_RES_OK;
}

static lv_res_t dims_webp(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 30 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WEBP", 4) != 0) return LV_RES_INV;

    const uint8_t *type = d + 12;
    if (memcmp(type, "VP8X", 4) == 0) {
        *w = (lv_coord_t) ((uint32_t) d[24] | (uint32_t) d[25] << 8 | (uint32_t) d[26] << 16) + 1;
        *h = (lv_coord_t) ((uint32_t) d[27] | (uint32_t) d[28] << 8 | (uint32_t) d[29] << 16) + 1;

        return LV_RES_OK;
    }

    if (memcmp(type, "VP8L", 4) == 0 && d[20] == 0x2F) {
        const uint32_t b = (uint32_t) d[21] | (uint32_t) d[22] << 8 | (uint32_t) d[23] << 16 | (uint32_t) d[24] << 24;

        *w = (lv_coord_t) (b & 0x3FFF) + 1;
        *h = (lv_coord_t) (b >> 14 & 0x3FFF) + 1;

        return LV_RES_OK;
    }

    if (memcmp(type, "VP8 ", 4) == 0 && (d[20] & 0x01) == 0 && d[23] == 0x9D && d[24] == 0x01 && d[25] == 0x2A) {
        *w = ((uint16_t) d[26] | (uint16_t) d[27] << 8) & 0x3FFF;
        *h = ((uint16_t) d[28] | (uint16_t) d[29] << 8) & 0x3FFF;

        return *w && *h ? LV_RES_OK : LV_RES_INV;
    }

    return LV_RES_INV;
}

static lv_res_t dims_pcx(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 12 || d[0] != 0x0A || d[1] > 5) return LV_RES_INV;

    const uint16_t xmin = (uint16_t) d[4] | (uint16_t) d[5] << 8;
    const uint16_t ymin = (uint16_t) d[6] | (uint16_t) d[7] << 8;

    const uint16_t xmax = (uint16_t) d[8] | (uint16_t) d[9] << 8;
    const uint16_t ymax = (uint16_t) d[10] | (uint16_t) d[11] << 8;

    if (xmax < xmin || ymax < ymin) return LV_RES_INV;

    *w = xmax - xmin + 1;
    *h = ymax - ymin + 1;

    return *w && *h ? LV_RES_OK : LV_RES_INV;
}

static lv_res_t dims_qoi(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 12 || memcmp(d, "qoif", 4) != 0) return LV_RES_INV;

    *w = (lv_coord_t) ((uint32_t) d[4] << 24 | (uint32_t) d[5] << 16 | (uint32_t) d[6] << 8 | d[7]);
    *h = (lv_coord_t) ((uint32_t) d[8] << 24 | (uint32_t) d[9] << 16 | (uint32_t) d[10] << 8 | d[11]);

    return *w && *h ? LV_RES_OK : LV_RES_INV;
}

static lv_res_t dims_tga(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 16 || d[1] > 1) return LV_RES_INV;

    const uint8_t t = d[2];
    if (!((t >= 1 && t <= 3) || (t >= 9 && t <= 11))) return LV_RES_INV;

    *w = (uint16_t) d[12] | (uint16_t) d[13] << 8;
    *h = (uint16_t) d[14] | (uint16_t) d[15] << 8;

    return *w && *h ? LV_RES_OK : LV_RES_INV;
}

static lv_res_t dims_jpeg(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return LV_RES_INV;
    uint32_t pos = 2;

    while (pos + 3 < n) {
        if (d[pos] != 0xFF) return LV_RES_INV;

        pos++;
        while (pos < n && d[pos] == 0xFF)
            pos++;

        if (pos >= n) return LV_RES_INV;
        const uint8_t marker = d[pos++];

        if (marker == 0xD9) return LV_RES_INV;
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) continue;
        if (pos + 2 > n) return LV_RES_INV;

        const uint16_t seg = (uint16_t) ((uint16_t) d[pos] << 8 | d[pos + 1]);
        if (seg < 2) return LV_RES_INV;

        pos += 2;
        if ((marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7)
            || (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF)) {
            if (pos + 4 >= n) return LV_RES_INV;

            *h = (lv_coord_t) d[pos + 1] << 8 | d[pos + 2];
            *w = (lv_coord_t) d[pos + 3] << 8 | d[pos + 4];

            return *w && *h ? LV_RES_OK : LV_RES_INV;
        }

        if (pos + (uint32_t) (seg - 2) > n) return LV_RES_INV;
        pos += (uint32_t) (seg - 2);
    }

    return LV_RES_INV;
}

typedef lv_res_t (*dims_fn_t)(const uint8_t *, uint32_t, lv_coord_t *, lv_coord_t *);

static int ext_to_dims(const char *ext, dims_fn_t *fn, uint32_t *hdr_len) {
    static const struct {
        const char *ext;
        dims_fn_t fn;
        uint32_t hdr_len;
    } table[] = {
        {"png", dims_png, 24},   {"gif", dims_gif, 10},    {"bmp", dims_bmp, 26},
        {"webp", dims_webp, 30}, {"jpg", dims_jpeg, 8192}, {"jpeg", dims_jpeg, 8192},
        {"pcx", dims_pcx, 12},   {"qoi", dims_qoi, 12},    {"tga", dims_tga, 16},
    };

    for (int i = 0; i < (int) (sizeof(table) / sizeof(table[0])); i++) {
        if (strcmp(ext, table[i].ext) == 0) {
            *fn = table[i].fn;
            *hdr_len = table[i].hdr_len;
            return 1;
        }
    }
    return 0;
}

static int supports_magic(const uint8_t *d, const uint32_t n) {
    if (n >= 8 && memcmp(d, png_sig, 8) == 0) return 1;

    if (n >= 4 && memcmp(d, "GIF8", 4) == 0) return 1;

    if (n >= 2 && d[0] == 'B' && d[1] == 'M') return 1;

    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8) return 1;

    if (n >= 12 && memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WEBP", 4) == 0) return 1;

    if (n >= 3 && d[0] == 0x0A && d[1] <= 5 && d[2] <= 1) return 1;

    if (n >= 4 && memcmp(d, "qoif", 4) == 0) return 1;

    return 0;
}

static lv_res_t dims_for_file(const char *path, lv_coord_t *w, lv_coord_t *h) {
    dims_fn_t fn;
    uint32_t hdr_len;
    if (!ext_to_dims(lv_fs_get_ext(path), &fn, &hdr_len)) return LV_RES_INV;

    uint8_t buf[8192];
    lv_fs_file_t f;
    if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return LV_RES_INV;

    uint32_t rn = 0;
    lv_fs_read(&f, buf, hdr_len, &rn);
    lv_fs_close(&f);

    return fn(buf, rn, w, h);
}

static lv_res_t dims_for_data(const uint8_t *d, const uint32_t n, lv_coord_t *w, lv_coord_t *h) {
    if (n >= 8 && memcmp(d, png_sig, 8) == 0) return dims_png(d, n, w, h);

    if (n >= 4 && memcmp(d, "GIF8", 4) == 0) return dims_gif(d, n, w, h);

    if (n >= 2 && d[0] == 'B' && d[1] == 'M') return dims_bmp(d, n, w, h);

    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8) return dims_jpeg(d, n, w, h);

    if (n >= 12 && memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WEBP", 4) == 0) return dims_webp(d, n, w, h);

    if (n >= 3 && d[0] == 0x0A && d[1] <= 5) return dims_pcx(d, n, w, h);

    if (n >= 4 && memcmp(d, "qoif", 4) == 0) return dims_qoi(d, n, w, h);

    return LV_RES_INV;
}

static uint8_t *surface_to_lv_pixels(SDL_Surface *src) {
    SDL_Surface *bgra = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_BGRA32, 0);
    SDL_FreeSurface(src);
    if (!bgra) return NULL;

    const uint32_t row = (uint32_t) bgra->w * 4;
    const uint32_t total = row * (uint32_t) bgra->h;
    uint8_t *pixels = lv_mem_alloc(total);

    if (pixels) {
        if (bgra->pitch == (int) row) {
            memcpy(pixels, bgra->pixels, total);
        } else {
            for (int y = 0; y < bgra->h; y++)
                memcpy(pixels + (uint32_t) y * row, (uint8_t *) bgra->pixels + y * bgra->pitch, row);
        }
    }

    SDL_FreeSurface(bgra);
    return pixels;
}

static lv_res_t decoder_info(struct _lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header) {
    (void) decoder;
    lv_coord_t w = 0, h = 0;
    lv_res_t res = LV_RES_INV;

    const lv_img_src_t src_type = lv_img_src_get_type(src);
    if (src_type == LV_IMG_SRC_FILE) {
        res = dims_for_file(src, &w, &h);
    } else if (src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *dsc = src;
        res = dims_for_data(dsc->data, dsc->data_size, &w, &h);
    }

    if (res == LV_RES_OK) {
        header->always_zero = 0;
        header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        header->w = w;
        header->h = h;
    }

    return res;
}

static lv_res_t decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    (void) decoder;
    SDL_Surface *surf = NULL;

    if (dsc->src_type == LV_IMG_SRC_FILE) {
        const char *path = dsc->src;
        dims_fn_t fn;
        uint32_t hdr_len;
        if (!ext_to_dims(lv_fs_get_ext(path), &fn, &hdr_len)) return LV_RES_INV;

        lv_fs_file_t f;
        if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return LV_RES_INV;

        uint32_t size = 0;
        lv_fs_seek(&f, 0, LV_FS_SEEK_END);
        lv_fs_tell(&f, &size);
        lv_fs_seek(&f, 0, LV_FS_SEEK_SET);

        void *buf = SDL_malloc(size);
        if (!buf) {
            lv_fs_close(&f);
            return LV_RES_INV;
        }

        uint32_t br = 0;
        lv_fs_read(&f, buf, size, &br);
        lv_fs_close(&f);

        if (br != size) {
            SDL_free(buf);
            return LV_RES_INV;
        }

        SDL_RWops *rw = SDL_RWFromMem(buf, (int) size);
        if (rw) {
            surf = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
        }
        SDL_free(buf);

    } else if (dsc->src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *img_dsc = dsc->src;
        if (!supports_magic(img_dsc->data, img_dsc->data_size)) return LV_RES_INV;

        SDL_RWops *rw = SDL_RWFromConstMem(img_dsc->data, (int) img_dsc->data_size);
        if (rw) surf = IMG_Load_RW(rw, 1);

    } else {
        return LV_RES_INV;
    }

    if (!surf) return LV_RES_INV;
    dsc->img_data = surface_to_lv_pixels(surf);

    return dsc->img_data ? LV_RES_OK : LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc) {
    (void) decoder;

    if (dsc->img_data) {
        lv_mem_free((uint8_t *) dsc->img_data);
        dsc->img_data = NULL;
    }
}
