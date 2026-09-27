/**
 * @file lv_tiny_ttf.h
 *
 */

#ifndef LV_TINY_TTF_H
#define LV_TINY_TTF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include "../../../lvgl.h"

#if LV_USE_TINY_TTF

/*********************
 *      DEFINES
 *********************/

#define LV_TINY_TTF_GLYPH_ID_BASE 0x110000u

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    uint32_t cluster;
    uint32_t glyph;
    lv_coord_t advance;
} lv_tiny_ttf_shaped_glyph_t;

typedef struct {
    lv_tiny_ttf_shaped_glyph_t *glyphs;
    uint32_t count;
} lv_tiny_ttf_shape_t;

typedef struct {
    uint32_t tag;
    float value;
} lv_tiny_ttf_axis_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

#if LV_TINY_TTF_FILE_SUPPORT
/* create a font from the specified file or path with the specified line height.*/
lv_font_t *lv_tiny_ttf_create_file(const char *path, lv_coord_t font_size);

/* create a font from the specified file or path with the specified line height with the specified cache size.*/
lv_font_t *lv_tiny_ttf_create_file_ex(const char *path, lv_coord_t font_size, size_t cache_size);

lv_font_t *
lv_tiny_ttf_create_file_face_ex(const char *path, lv_coord_t font_size, size_t cache_size, unsigned int face_index);
#endif /*LV_TINY_TTF_FILE_SUPPORT*/

/* create a font from the specified data pointer with the specified line height.*/
lv_font_t *lv_tiny_ttf_create_data(const void *data, size_t data_size, lv_coord_t font_size);

/* create a font from the specified data pointer with the specified line height and the specified cache size.*/
lv_font_t *lv_tiny_ttf_create_data_ex(const void *data, size_t data_size, lv_coord_t font_size, size_t cache_size);

lv_font_t *lv_tiny_ttf_create_data_face_ex(
    const void *data, size_t data_size, lv_coord_t font_size, size_t cache_size, unsigned int face_index
);

/* set the size of the font to a new font_size*/
void lv_tiny_ttf_set_size(lv_font_t *font, lv_coord_t font_size);

bool lv_tiny_ttf_set_variations(lv_font_t *font, const lv_tiny_ttf_axis_t *axes, uint32_t axis_count);

bool lv_tiny_ttf_shape_text(const lv_font_t *font, const char *text, uint32_t text_length, lv_tiny_ttf_shape_t *shape);

uint32_t lv_tiny_ttf_shape_glyph_at(const lv_tiny_ttf_shape_t *shape, uint32_t cluster);

lv_coord_t lv_tiny_ttf_shape_advance_at(const lv_tiny_ttf_shape_t *shape, uint32_t cluster);

void lv_tiny_ttf_shape_destroy(lv_tiny_ttf_shape_t *shape);

/* destroy a font previously created with lv_tiny_ttf_create_xxxx()*/
void lv_tiny_ttf_destroy(lv_font_t *font);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_TINY_TTF*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_TINY_TTF_H*/
