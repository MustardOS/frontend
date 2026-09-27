#include "lv_tiny_ttf.h"

#if LV_USE_TINY_TTF

#include <stdio.h>
#include <limits.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ot.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include "../../../misc/lv_lru.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void) (u), lv_mem_alloc(x))
#define STBTT_free(x, u)   ((void) (u), lv_mem_free(x))
#define TTF_MALLOC(x)      (lv_mem_alloc(x))
#define TTF_FREE(x)        (lv_mem_free(x))

#if LV_TINY_TTF_FILE_SUPPORT
/* a hydra stream that can be in memory or from a file*/
typedef struct ttf_cb_stream {
    lv_fs_file_t *file;
    const void *data;
    size_t size;
    size_t position;
} ttf_cb_stream_t;

static void ttf_cb_stream_read(ttf_cb_stream_t *stream, void *data, size_t to_read) {
    if (stream->file != NULL) {
        uint32_t br;
        lv_fs_read(stream->file, data, to_read, &br);
    } else {
        if (to_read + stream->position >= stream->size) {
            to_read = stream->size - stream->position;
        }
        lv_memcpy(data, ((const unsigned char *) stream->data + stream->position), to_read);
        stream->position += to_read;
    }
}
static void ttf_cb_stream_seek(ttf_cb_stream_t *stream, size_t position) {
    if (stream->file != NULL) {
        lv_fs_seek(stream->file, position, LV_FS_SEEK_SET);
    } else {
        if (position > stream->size) {
            stream->position = stream->size;
        } else {
            stream->position = position;
        }
    }
}

/* for stream support */
#define STBTT_STREAM_TYPE          ttf_cb_stream_t *
#define STBTT_STREAM_SEEK(s, x)    ttf_cb_stream_seek(s, x);
#define STBTT_STREAM_READ(s, x, y) ttf_cb_stream_read(s, x, y);
#endif /*LV_TINY_TTF_FILE_SUPPORT*/

#include <stb/stb_rect_pack.h>
#include <stb/stb_truetype.h>

typedef struct ttf_font_desc {
    lv_fs_file_t file;
#if LV_TINY_TTF_FILE_SUPPORT
    ttf_cb_stream_t stream;
#else
    const uint8_t *stream;
#endif
    stbtt_fontinfo info;
    float scale;
    int ascent;
    int descent;
    lv_lru_t *bitmap_cache;
    size_t bitmap_cache_size;
    hb_blob_t *hb_blob;
    hb_face_t *hb_face;
    hb_font_t *hb_font;
    hb_buffer_t *hb_buffer;
    lv_coord_t font_size;
    bool shaping_enabled;
    const void *font_data;
    size_t font_data_size;
    unsigned int face_index;
    FT_Face ft_face;
    bool stb_valid;
} ttf_font_desc_t;

static FT_Library ft_library;
static bool ft_library_ready;

typedef struct ttf_bitmap_cache_key {
    uint32_t unicode_letter;
    lv_coord_t line_height;
} ttf_bitmap_cache_key_t;

static bool ttf_shape(
    const ttf_font_desc_t *dsc, const char *text, uint32_t text_length, lv_tiny_ttf_shape_t *shape, bool require_enabled
);
static int ttf_glyph_index(const ttf_font_desc_t *dsc, uint32_t letter);

static bool ttf_ft_set_size(ttf_font_desc_t *dsc) {
    if (dsc->ft_face == NULL) return false;

    if (dsc->ft_face->num_fixed_sizes > 0) {
        FT_Int selected = 0;
        FT_Pos distance = LONG_MAX;
        const FT_Pos requested = (FT_Pos) dsc->font_size * 64;
        for (FT_Int i = 0; i < dsc->ft_face->num_fixed_sizes; i++) {
            const FT_Pos candidate = dsc->ft_face->available_sizes[i].y_ppem;
            const FT_Pos difference = candidate > requested ? candidate - requested : requested - candidate;
            if (difference < distance) {
                distance = difference;
                selected = i;
            }
        }
        return FT_Select_Size(dsc->ft_face, selected) == 0;
    }

    FT_Size_RequestRec request = {.type = FT_SIZE_REQUEST_TYPE_NOMINAL, .height = (FT_Long) dsc->font_size * 64};
    return FT_Request_Size(dsc->ft_face, &request) == 0;
}

static bool ttf_detect_shaping(ttf_font_desc_t *dsc) {
    static const char sample[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    lv_tiny_ttf_shape_t shape = {0};

    if (!ttf_shape(dsc, sample, (uint32_t) (sizeof(sample) - 1), &shape, false)) return false;

    uint32_t byte_index = 0;
    bool changed = false;
    for (uint32_t i = 0; i < shape.count; i++) {
        const uint32_t codepoint = (uint8_t) sample[byte_index++];
        const uint32_t nominal = (uint32_t) ttf_glyph_index(dsc, codepoint);
        if (shape.glyphs[i].glyph != nominal) {
            changed = true;
            break;
        }
    }

    lv_tiny_ttf_shape_destroy(&shape);
    return changed;
}

static bool ttf_freetype_init(ttf_font_desc_t *dsc) {
    if (dsc->ft_face != NULL) return true;
    if (dsc->font_data == NULL || dsc->font_data_size == 0 || dsc->font_data_size > LONG_MAX) return false;

    if (!ft_library_ready) {
        if (FT_Init_FreeType(&ft_library) != 0) return false;
        ft_library_ready = true;
    }

    if (FT_New_Memory_Face(
            ft_library, (const FT_Byte *) dsc->font_data, (FT_Long) dsc->font_data_size, (FT_Long) dsc->face_index,
            &dsc->ft_face
        )
        != 0)
        return false;

    if (dsc->ft_face->charmap == NULL && dsc->ft_face->num_charmaps > 0) {
        if (FT_Select_Charmap(dsc->ft_face, FT_ENCODING_UNICODE) != 0)
            FT_Set_Charmap(dsc->ft_face, dsc->ft_face->charmaps[0]);
    }

    if (!ttf_ft_set_size(dsc)) {
        FT_Done_Face(dsc->ft_face);
        dsc->ft_face = NULL;
        return false;
    }

    return true;
}

static int ttf_glyph_index(const ttf_font_desc_t *dsc, const uint32_t letter) {
    if (letter >= LV_TINY_TTF_GLYPH_ID_BASE) return (int) (letter - LV_TINY_TTF_GLYPH_ID_BASE);
    if (dsc->ft_face != NULL) return (int) FT_Get_Char_Index(dsc->ft_face, letter);
    return stbtt_FindGlyphIndex(&dsc->info, (int) letter);
}

static bool ttf_ft_load_glyph(ttf_font_desc_t *dsc, const int glyph, const FT_Int32 flags) {
    return dsc->ft_face != NULL && glyph > 0
           && FT_Load_Glyph(dsc->ft_face, (FT_UInt) glyph, flags | FT_LOAD_TARGET_NORMAL) == 0;
}

static void ttf_ft_copy_bitmap(uint8_t *destination, const FT_Bitmap *bitmap) {
    const int width = (int) bitmap->width;
    const int height = (int) bitmap->rows;
    const ptrdiff_t pitch = bitmap->pitch < 0 ? -(ptrdiff_t) bitmap->pitch : (ptrdiff_t) bitmap->pitch;

    for (int row = 0; row < height; row++) {
        const int source_row = bitmap->pitch < 0 ? height - row - 1 : row;
        const uint8_t *source = bitmap->buffer + (ptrdiff_t) source_row * pitch;
        uint8_t *target = destination + (size_t) row * (size_t) width;

        switch (bitmap->pixel_mode) {
            case FT_PIXEL_MODE_MONO:
                for (int column = 0; column < width; column++)
                    target[column] = (source[column >> 3] & (0x80u >> (column & 7))) ? 255 : 0;
                break;
            case FT_PIXEL_MODE_GRAY2:
                for (int column = 0; column < width; column++)
                    target[column] = (uint8_t) (((source[column >> 2] >> (6 - 2 * (column & 3))) & 3u) * 85u);
                break;
            case FT_PIXEL_MODE_GRAY4:
                for (int column = 0; column < width; column++)
                    target[column] = (uint8_t) (((source[column >> 1] >> (4 - 4 * (column & 1))) & 15u) * 17u);
                break;
            case FT_PIXEL_MODE_BGRA:
                for (int column = 0; column < width; column++)
                    target[column] = source[column * 4 + 3];
                break;
            default:
                if (bitmap->num_grays > 1 && bitmap->num_grays != 256) {
                    for (int column = 0; column < width; column++)
                        target[column] = (uint8_t) ((unsigned int) source[column] * 255u / (bitmap->num_grays - 1u));
                } else {
                    lv_memcpy(target, source, (size_t) width);
                }
                break;
        }
    }
}

static bool ttf_reset_bitmap_cache(ttf_font_desc_t *dsc) {
    lv_lru_t *replacement = lv_lru_create(
        dsc->bitmap_cache_size, (size_t) dsc->font_size * (size_t) dsc->font_size, lv_mem_free, lv_mem_free
    );
    if (replacement == NULL) return false;

    lv_lru_del(dsc->bitmap_cache);
    dsc->bitmap_cache = replacement;
    return true;
}

static int ttf_get_glyph_dsc_cb(
    const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next
) {
    if (unicode_letter < 0x20 || unicode_letter == 0xf8ff || /*LV_SYMBOL_DUMMY*/
        unicode_letter == 0x200c) {                          /*ZERO WIDTH NON-JOINER*/
        dsc_out->box_w = 0;
        dsc_out->adv_w = 0;
        dsc_out->box_h = 0; /*height of the bitmap in [px]*/
        dsc_out->ofs_x = 0; /*X offset of the bitmap in [pf]*/
        dsc_out->ofs_y = 0; /*Y offset of the bitmap in [pf]*/
        dsc_out->bpp = 0;
        dsc_out->is_placeholder = 0;
        return 1;
    }
    ttf_font_desc_t *dsc = (ttf_font_desc_t *) font->dsc;
    const int g1 = ttf_glyph_index(dsc, unicode_letter);
    if (g1 == 0) {
        /* Glyph not found */
        return 0;
    }

    const int g2 = unicode_letter_next != 0 ? ttf_glyph_index(dsc, unicode_letter_next) : 0;
    if (dsc->ft_face != NULL) {
        if (!ttf_ft_load_glyph(dsc, g1, FT_LOAD_DEFAULT)) return 0;

        FT_GlyphSlot slot = dsc->ft_face->glyph;
        FT_Vector kerning = {0};
        if (g2 > 0 && FT_HAS_KERNING(dsc->ft_face))
            FT_Get_Kerning(dsc->ft_face, (FT_UInt) g1, (FT_UInt) g2, FT_KERNING_DEFAULT, &kerning);

        const FT_Pos advance = slot->advance.x + kerning.x;
        dsc_out->adv_w = (uint16_t) ((advance + 32) >> 6);
        dsc_out->box_w = (lv_coord_t) ((slot->metrics.width + 63) >> 6);
        dsc_out->box_h = (lv_coord_t) ((slot->metrics.height + 63) >> 6);
        dsc_out->ofs_x = (lv_coord_t) (slot->metrics.horiBearingX >> 6);
        dsc_out->ofs_y = (lv_coord_t) (((slot->metrics.horiBearingY + 63) >> 6) - dsc_out->box_h);
        dsc_out->bpp = 8;
        dsc_out->is_placeholder = 0;
        return 1;
    }

    int x1, y1, x2, y2;

    stbtt_GetGlyphBitmapBox(&dsc->info, g1, dsc->scale, dsc->scale, &x1, &y1, &x2, &y2);
    int advw, lsb;
    stbtt_GetGlyphHMetrics(&dsc->info, g1, &advw, &lsb);
    int k = (g2 > 0) ? stbtt_GetGlyphKernAdvance(&dsc->info, g1, g2) : 0;
    dsc_out->adv_w = (uint16_t) floor(
        (((float) advw + (float) k) * dsc->scale) + 0.5f
    ); /*Horizontal space required by the glyph in [px]*/

    dsc_out->box_w = (x2 - x1 + 1); /*width of the bitmap in [px]*/
    dsc_out->box_h = (y2 - y1 + 1); /*height of the bitmap in [px]*/
    dsc_out->ofs_x = x1;            /*X offset of the bitmap in [pf]*/
    dsc_out->ofs_y = -y2;           /*Y offset of the bitmap measured from the as line*/
    dsc_out->bpp = 8;               /*Bits per pixel: 1/2/4/8*/
    dsc_out->is_placeholder = 0;
    return 1; /*1: glyph found; 0: glyph was not found*/
}

static const uint8_t *ttf_get_glyph_bitmap_cb(const lv_font_t *font, uint32_t unicode_letter) {
    ttf_font_desc_t *dsc = (ttf_font_desc_t *) font->dsc;
    const stbtt_fontinfo *info = (const stbtt_fontinfo *) &dsc->info;
    const int g1 = ttf_glyph_index(dsc, unicode_letter);
    if (g1 == 0) {
        /* Glyph not found */
        return NULL;
    }
    /*Try to load from cache*/
    ttf_bitmap_cache_key_t cache_key;
    lv_memset(&cache_key, 0, sizeof(cache_key)); /*Zero padding*/
    cache_key.unicode_letter = unicode_letter;
    cache_key.line_height = font->line_height;
    uint8_t *buffer = NULL;
    lv_lru_get(dsc->bitmap_cache, &cache_key, sizeof(cache_key), (void **) &buffer);
    if (buffer) {
        return buffer;
    }
    LV_LOG_TRACE("cache miss for letter: %u", unicode_letter);
    int x1, y1, x2, y2;
    int w, h;
    if (dsc->ft_face != NULL) {
        if (!ttf_ft_load_glyph(dsc, g1, FT_LOAD_RENDER)) return NULL;
        w = (int) dsc->ft_face->glyph->bitmap.width;
        h = (int) dsc->ft_face->glyph->bitmap.rows;
    } else {
        stbtt_GetGlyphBitmapBox(info, g1, dsc->scale, dsc->scale, &x1, &y1, &x2, &y2);
        w = x2 - x1 + 1;
        h = y2 - y1 + 1;
    }
    if (w <= 0 || h <= 0) return NULL;
    const uint32_t stride = (uint32_t) w;
    /*Prepare space in cache*/
    size_t szb = h * stride;
    buffer = lv_mem_alloc(szb);
    if (!buffer) {
        LV_LOG_ERROR("failed to allocate cache value");
        return NULL;
    }
    lv_memset(buffer, 0, szb);
    if (LV_LRU_OK != lv_lru_set(dsc->bitmap_cache, &cache_key, sizeof(cache_key), buffer, szb)) {
        LV_LOG_ERROR("failed to add cache value");
        lv_mem_free(buffer);
        return NULL;
    }
    /*Render into cache*/
    if (dsc->ft_face != NULL) {
        const FT_Bitmap *bitmap = &dsc->ft_face->glyph->bitmap;
        ttf_ft_copy_bitmap(buffer, bitmap);
    } else {
        stbtt_MakeGlyphBitmap(info, buffer, w, h, stride, dsc->scale, dsc->scale, g1);
    }
    return buffer;
}

static lv_font_t *
lv_tiny_ttf_create(
    const char *path, const void *data, size_t data_size, lv_coord_t font_size, size_t cache_size,
    const unsigned int face_index
) {
    if ((path == NULL && data == NULL) || 0 >= font_size) {
        LV_LOG_ERROR("tiny_ttf: invalid argument\n");
        return NULL;
    }
    ttf_font_desc_t *dsc = (ttf_font_desc_t *) TTF_MALLOC(sizeof(ttf_font_desc_t));
    if (dsc == NULL) {
        LV_LOG_ERROR("tiny_ttf: out of memory\n");
        return NULL;
    }
    lv_memset(dsc, 0, sizeof(*dsc));
    dsc->font_data = data;
    dsc->font_data_size = data_size;
    dsc->face_index = face_index;
#if LV_TINY_TTF_FILE_SUPPORT
    if (path != NULL) {
        if (LV_FS_RES_OK != lv_fs_open(&dsc->file, path, LV_FS_MODE_RD)) {
            LV_LOG_ERROR("tiny_ttf: unable to open %s\n", path);
            goto err_after_dsc;
        }
        dsc->stream.file = &dsc->file;
    } else {
        dsc->stream.file = NULL;
        dsc->stream.data = (const uint8_t *) data;
        dsc->stream.size = data_size;
        dsc->stream.position = 0;
    }
    const int font_offset = stbtt_GetFontOffsetForIndex(&dsc->stream, (int) face_index);
    dsc->stb_valid = font_offset >= 0 && stbtt_InitFont(&dsc->info, &dsc->stream, font_offset) != 0;

#else
    dsc->stream = (const uint8_t *) data;
    LV_UNUSED(data_size);
    const int font_offset = stbtt_GetFontOffsetForIndex(dsc->stream, (int) face_index);
    dsc->stb_valid = font_offset >= 0 && stbtt_InitFont(&dsc->info, dsc->stream, font_offset) != 0;
#endif

    dsc->font_size = font_size;
    if (!dsc->stb_valid && data != NULL && data_size > 0) ttf_freetype_init(dsc);
    if (!dsc->stb_valid && dsc->ft_face == NULL) {
        LV_LOG_ERROR("tiny_ttf: init failed\n");
        goto err_after_dsc;
    }

    dsc->bitmap_cache_size = cache_size;
    dsc->bitmap_cache = lv_lru_create(cache_size, font_size * font_size, lv_mem_free, lv_mem_free);
    if (dsc->bitmap_cache == NULL) {
        LV_LOG_ERROR("failed to create lru cache");
        goto err_after_dsc;
    }

    if (data != NULL && data_size > 0 && data_size <= UINT_MAX) {
        dsc->hb_blob =
            hb_blob_create((const char *) data, (unsigned int) data_size, HB_MEMORY_MODE_READONLY, NULL, NULL);
        dsc->hb_face = hb_face_create(dsc->hb_blob, face_index);
        dsc->hb_font = hb_font_create(dsc->hb_face);
        dsc->hb_buffer = hb_buffer_create();
        if (hb_blob_get_length(dsc->hb_blob) > 0 && hb_face_get_glyph_count(dsc->hb_face) > 0) {
            hb_ot_font_set_funcs(dsc->hb_font);
        } else {
            hb_buffer_destroy(dsc->hb_buffer);
            hb_font_destroy(dsc->hb_font);
            hb_face_destroy(dsc->hb_face);
            hb_blob_destroy(dsc->hb_blob);
            dsc->hb_buffer = NULL;
            dsc->hb_font = NULL;
            dsc->hb_face = NULL;
            dsc->hb_blob = NULL;
        }
    }

    lv_font_t *out_font = (lv_font_t *) TTF_MALLOC(sizeof(lv_font_t));
    if (out_font == NULL) {
        LV_LOG_ERROR("tiny_ttf: out of memory\n");
        goto err_after_bitmap_cache;
    }
    lv_memset(out_font, 0, sizeof(lv_font_t));
    out_font->get_glyph_dsc = ttf_get_glyph_dsc_cb;
    out_font->get_glyph_bitmap = ttf_get_glyph_bitmap_cb;
    out_font->dsc = dsc;
    lv_tiny_ttf_set_size(out_font, font_size);
    dsc->shaping_enabled = ttf_detect_shaping(dsc);
    if (!dsc->shaping_enabled) {
        if (dsc->hb_buffer != NULL) hb_buffer_destroy(dsc->hb_buffer);
        if (dsc->hb_font != NULL) hb_font_destroy(dsc->hb_font);
        if (dsc->hb_face != NULL) hb_face_destroy(dsc->hb_face);
        if (dsc->hb_blob != NULL) hb_blob_destroy(dsc->hb_blob);
        dsc->hb_buffer = NULL;
        dsc->hb_font = NULL;
        dsc->hb_face = NULL;
        dsc->hb_blob = NULL;
    }
    return out_font;
err_after_bitmap_cache:
    if (dsc->hb_buffer != NULL) hb_buffer_destroy(dsc->hb_buffer);
    if (dsc->hb_font != NULL) hb_font_destroy(dsc->hb_font);
    if (dsc->hb_face != NULL) hb_face_destroy(dsc->hb_face);
    if (dsc->hb_blob != NULL) hb_blob_destroy(dsc->hb_blob);
    lv_lru_del(dsc->bitmap_cache);
err_after_dsc:
    if (dsc->ft_face != NULL) FT_Done_Face(dsc->ft_face);
    TTF_FREE(dsc);
    return NULL;
}

#if LV_TINY_TTF_FILE_SUPPORT
lv_font_t *lv_tiny_ttf_create_file_ex(const char *path, lv_coord_t font_size, size_t cache_size) {
    return lv_tiny_ttf_create(path, NULL, 0, font_size, cache_size, 0);
}
lv_font_t *lv_tiny_ttf_create_file_face_ex(
    const char *path, const lv_coord_t font_size, const size_t cache_size, const unsigned int face_index
) {
    return lv_tiny_ttf_create(path, NULL, 0, font_size, cache_size, face_index);
}
lv_font_t *lv_tiny_ttf_create_file(const char *path, lv_coord_t font_size) {
    return lv_tiny_ttf_create_file_ex(path, font_size, 4096);
}
#endif /*LV_TINY_TTF_FILE_SUPPORT*/

lv_font_t *lv_tiny_ttf_create_data_ex(const void *data, size_t data_size, lv_coord_t font_size, size_t cache_size) {
    return lv_tiny_ttf_create(NULL, data, data_size, font_size, cache_size, 0);
}

lv_font_t *lv_tiny_ttf_create_data_face_ex(
    const void *data, const size_t data_size, const lv_coord_t font_size, const size_t cache_size,
    const unsigned int face_index
) {
    return lv_tiny_ttf_create(NULL, data, data_size, font_size, cache_size, face_index);
}

lv_font_t *lv_tiny_ttf_create_data(const void *data, size_t data_size, lv_coord_t font_size) {
    return lv_tiny_ttf_create_data_ex(data, data_size, font_size, 4096);
}

void lv_tiny_ttf_set_size(lv_font_t *font, lv_coord_t font_size) {
    if (font_size <= 0) {
        LV_LOG_ERROR("invalid font size: %" PRIx32, font_size);
        return;
    }
    ttf_font_desc_t *dsc = (ttf_font_desc_t *) font->dsc;
    dsc->font_size = font_size;
    int line_gap = 0;
    if (dsc->ft_face != NULL) {
        if (!ttf_ft_set_size(dsc)) return;
        dsc->ascent = dsc->ft_face->ascender;
        dsc->descent = dsc->ft_face->descender;
        line_gap = dsc->ft_face->height - dsc->ascent + dsc->descent;
        dsc->scale = dsc->ft_face->units_per_EM > 0 ? (float) font_size / (float) dsc->ft_face->units_per_EM : 1.0f;
    } else {
        stbtt_GetFontVMetrics(&dsc->info, &dsc->ascent, &dsc->descent, &line_gap);
        dsc->scale = stbtt_ScaleForMappingEmToPixels(&dsc->info, font_size);
    }
    if (dsc->hb_font != NULL) {
        const unsigned int units_per_em = hb_face_get_upem(dsc->hb_face);
        int x_scale;
        int y_scale;
        if (dsc->ft_face != NULL && dsc->ft_face->size != NULL) {
            x_scale = (int) FT_MulFix((FT_Long) units_per_em, dsc->ft_face->size->metrics.x_scale);
            y_scale = (int) FT_MulFix((FT_Long) units_per_em, dsc->ft_face->size->metrics.y_scale);
        } else {
            const double scaled_size = (double) dsc->scale * units_per_em * 64.0;
            x_scale = y_scale = scaled_size < INT_MAX ? (int) (scaled_size + 0.5) : INT_MAX;
        }
        hb_font_set_scale(dsc->hb_font, x_scale, y_scale);
        hb_font_set_ppem(dsc->hb_font, (unsigned int) font_size, (unsigned int) font_size);
    }
    if (dsc->ft_face != NULL && dsc->ft_face->size != NULL) {
        font->line_height = (lv_coord_t) ((dsc->ft_face->size->metrics.height + 32) >> 6);
        if (font->line_height <= 0)
            font->line_height =
                (lv_coord_t) ((dsc->ft_face->size->metrics.ascender - dsc->ft_face->size->metrics.descender + 32) >> 6);
        const lv_coord_t ascender = (lv_coord_t) ((dsc->ft_face->size->metrics.ascender + 32) >> 6);
        font->base_line = font->line_height - ascender;
    } else {
        font->line_height = (lv_coord_t) (dsc->scale * (dsc->ascent - dsc->descent + line_gap));
        font->base_line = (lv_coord_t) (dsc->scale * (line_gap - dsc->descent));
    }
    if (font->base_line < 0) font->base_line = 0;
    if (font->base_line > font->line_height) font->base_line = font->line_height;
}

bool lv_tiny_ttf_set_variations(lv_font_t *font, const lv_tiny_ttf_axis_t *axes, const uint32_t axis_count) {
    if (font == NULL || font->dsc == NULL || axes == NULL || axis_count == 0) return false;

    ttf_font_desc_t *dsc = (ttf_font_desc_t *) font->dsc;
    if (!ttf_freetype_init(dsc) || !FT_HAS_MULTIPLE_MASTERS(dsc->ft_face)) return false;

    FT_MM_Var *variation = NULL;
    if (FT_Get_MM_Var(dsc->ft_face, &variation) != 0 || variation == NULL) return false;

    FT_Fixed *coordinates = lv_mem_alloc((size_t) variation->num_axis * sizeof(*coordinates));
    hb_variation_t *hb_axes = lv_mem_alloc((size_t) axis_count * sizeof(*hb_axes));
    if (coordinates == NULL || hb_axes == NULL) {
        lv_mem_free(coordinates);
        lv_mem_free(hb_axes);
        FT_Done_MM_Var(ft_library, variation);
        return false;
    }

    for (FT_UInt i = 0; i < variation->num_axis; i++)
        coordinates[i] = variation->axis[i].def;

    for (uint32_t requested = 0; requested < axis_count; requested++) {
        hb_axes[requested].tag = axes[requested].tag;
        hb_axes[requested].value = axes[requested].value;

        for (FT_UInt available = 0; available < variation->num_axis; available++) {
            if (variation->axis[available].tag != axes[requested].tag) continue;

            float value = axes[requested].value;
            const float minimum = (float) variation->axis[available].minimum / 65536.0f;
            const float maximum = (float) variation->axis[available].maximum / 65536.0f;
            if (value < minimum) value = minimum;
            if (value > maximum) value = maximum;
            coordinates[available] = (FT_Fixed) (value * 65536.0f);
            hb_axes[requested].value = value;
            break;
        }
    }

    const bool applied = FT_Set_Var_Design_Coordinates(dsc->ft_face, variation->num_axis, coordinates) == 0;
    if (applied) {
        if (dsc->hb_font != NULL) hb_font_set_variations(dsc->hb_font, hb_axes, axis_count);
        lv_tiny_ttf_set_size(font, dsc->font_size);
        ttf_reset_bitmap_cache(dsc);
    }

    lv_mem_free(coordinates);
    lv_mem_free(hb_axes);
    FT_Done_MM_Var(ft_library, variation);
    return applied;
}

static bool ttf_shape(
    const ttf_font_desc_t *dsc, const char *text, const uint32_t text_length, lv_tiny_ttf_shape_t *shape,
    const bool require_enabled
) {
    if (shape == NULL) return false;
    shape->glyphs = NULL;
    shape->count = 0;

    if (dsc == NULL || dsc->hb_font == NULL || dsc->hb_buffer == NULL || text == NULL || text_length == 0) return false;
    if (require_enabled && !dsc->shaping_enabled) return false;

    hb_buffer_t *buffer = dsc->hb_buffer;
    hb_buffer_reset(buffer);
    if (hb_buffer_allocation_successful(buffer) == 0) {
        return false;
    }

    hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
    hb_buffer_add_utf8(buffer, text, (int) text_length, 0, (int) text_length);
    hb_buffer_guess_segment_properties(buffer);

    if (hb_buffer_get_direction(buffer) != HB_DIRECTION_LTR) {
        return false;
    }

    hb_shape(dsc->hb_font, buffer, NULL, 0);

    unsigned int glyph_count = 0;
    const hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
    const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, NULL);
    if (glyph_count == 0 || info == NULL || positions == NULL || glyph_count > SIZE_MAX / sizeof(*shape->glyphs)) {
        return false;
    }

    uint32_t character_count = 0;
    for (uint32_t i = 0; i < text_length;) {
        const unsigned char lead = (unsigned char) text[i];
        uint32_t width = 1;
        if ((lead & 0xe0u) == 0xc0u)
            width = 2;
        else if ((lead & 0xf0u) == 0xe0u)
            width = 3;
        else if ((lead & 0xf8u) == 0xf0u)
            width = 4;
        if (width > text_length - i) {
            return false;
        }
        i += width;
        character_count++;
    }

    if (glyph_count != character_count) {
        return false;
    }

    lv_tiny_ttf_shaped_glyph_t *glyphs = lv_mem_alloc((size_t) glyph_count * sizeof(*glyphs));
    if (glyphs == NULL) {
        return false;
    }

    uint32_t byte_index = 0;
    bool valid = true;
    for (unsigned int i = 0; i < glyph_count; i++) {
        if (info[i].codepoint == 0 || info[i].cluster != byte_index || positions[i].x_advance < 0
            || positions[i].y_advance != 0 || positions[i].x_offset != 0 || positions[i].y_offset != 0) {
            valid = false;
            break;
        }

        glyphs[i].cluster = info[i].cluster;
        glyphs[i].glyph = info[i].codepoint;
        glyphs[i].advance = (lv_coord_t) ((positions[i].x_advance + 32) / 64);

        const unsigned char lead = (unsigned char) text[byte_index];
        if ((lead & 0xe0u) == 0xc0u)
            byte_index += 2;
        else if ((lead & 0xf0u) == 0xe0u)
            byte_index += 3;
        else if ((lead & 0xf8u) == 0xf0u)
            byte_index += 4;
        else
            byte_index++;
    }

    if (!valid) {
        lv_mem_free(glyphs);
        return false;
    }

    shape->glyphs = glyphs;
    shape->count = glyph_count;
    return true;
}

bool lv_tiny_ttf_shape_text(
    const lv_font_t *font, const char *text, const uint32_t text_length, lv_tiny_ttf_shape_t *shape
) {
    if (shape == NULL) return false;
    shape->glyphs = NULL;
    shape->count = 0;

    if (font == NULL || font->get_glyph_dsc != ttf_get_glyph_dsc_cb
        || font->get_glyph_bitmap != ttf_get_glyph_bitmap_cb) {
        return false;
    }

    return ttf_shape((const ttf_font_desc_t *) font->dsc, text, text_length, shape, true);
}

static const lv_tiny_ttf_shaped_glyph_t *ttf_shape_find(const lv_tiny_ttf_shape_t *shape, const uint32_t cluster) {
    if (shape == NULL || shape->glyphs == NULL) return NULL;

    uint32_t first = 0;
    uint32_t count = shape->count;
    while (count > 0) {
        const uint32_t step = count / 2;
        const uint32_t current = first + step;
        if (shape->glyphs[current].cluster < cluster) {
            first = current + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    return first < shape->count && shape->glyphs[first].cluster == cluster ? &shape->glyphs[first] : NULL;
}

uint32_t lv_tiny_ttf_shape_glyph_at(const lv_tiny_ttf_shape_t *shape, const uint32_t cluster) {
    const lv_tiny_ttf_shaped_glyph_t *glyph = ttf_shape_find(shape, cluster);
    return glyph ? LV_TINY_TTF_GLYPH_ID_BASE + glyph->glyph : 0;
}

lv_coord_t lv_tiny_ttf_shape_advance_at(const lv_tiny_ttf_shape_t *shape, const uint32_t cluster) {
    const lv_tiny_ttf_shaped_glyph_t *glyph = ttf_shape_find(shape, cluster);
    return glyph ? glyph->advance : 0;
}

void lv_tiny_ttf_shape_destroy(lv_tiny_ttf_shape_t *shape) {
    if (shape == NULL) return;
    lv_mem_free(shape->glyphs);
    shape->glyphs = NULL;
    shape->count = 0;
}

void lv_tiny_ttf_destroy(lv_font_t *font) {
    if (font != NULL) {
        if (font->dsc != NULL) {
            ttf_font_desc_t *ttf = (ttf_font_desc_t *) font->dsc;
#if LV_TINY_TTF_FILE_SUPPORT
            if (ttf->stream.file != NULL) {
                lv_fs_close(&ttf->file);
            }
#endif
            if (ttf->hb_buffer != NULL) hb_buffer_destroy(ttf->hb_buffer);
            if (ttf->hb_font != NULL) hb_font_destroy(ttf->hb_font);
            if (ttf->hb_face != NULL) hb_face_destroy(ttf->hb_face);
            if (ttf->hb_blob != NULL) hb_blob_destroy(ttf->hb_blob);
            if (ttf->ft_face != NULL) FT_Done_Face(ttf->ft_face);
            lv_lru_del(ttf->bitmap_cache);
            TTF_FREE(ttf);
        }
        TTF_FREE(font);
    }
}

#endif /*LV_USE_TINY_TTF*/
