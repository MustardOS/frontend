/**
 * @file lv_draw_img.h
 *
 */

#ifndef LV_DRAW_IMG_H
#define LV_DRAW_IMG_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_img_decoder.h"
#include "lv_img_buf.h"
#include "../misc/lv_style.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {

    int16_t angle;
    uint16_t zoom;
    lv_point_t pivot;

    lv_color_t recolor;
    lv_opa_t recolor_opa;

    lv_opa_t opa;
    lv_blend_mode_t blend_mode : 4;

    int32_t frame_id;
    uint8_t antialias : 1;

    int8_t effect_type;
    lv_color_t effect_color;
    lv_opa_t effect_opa;
    int8_t effect_x_offset;
    int8_t effect_y_offset;
} lv_draw_img_dsc_t;

struct _lv_draw_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_draw_img_dsc_init(lv_draw_img_dsc_t *dsc);

void lv_glyph_shadow_set(
    int enabled, lv_color_t def_colour, lv_opa_t def_alpha, int16_t def_x, int16_t def_y, lv_color_t focus_colour,
    lv_opa_t focus_alpha, int16_t focus_x, int16_t focus_y
);

/**
 * Draw an image
 * @param coords the coordinates of the image
 * @param mask the image will be drawn only in this area
 * @param src pointer to a lv_color_t array which contains the pixels of the image
 * @param dsc pointer to an initialized `lv_draw_img_dsc_t` variable
 */
void lv_draw_img(
    struct _lv_draw_ctx_t *draw_ctx, const lv_draw_img_dsc_t *dsc, const lv_area_t *coords, const void *src
);

void lv_draw_img_decoded(
    struct _lv_draw_ctx_t *draw_ctx, const lv_draw_img_dsc_t *dsc, const lv_area_t *coords, const uint8_t *map_p,
    lv_img_cf_t color_format
);

/**
 * Get the type of an image source
 * @param src pointer to an image source:
 *  - pointer to an 'lv_img_t' variable (image stored internally and compiled into the code)
 *  - a path to a file (e.g. "S:/folder/image.bin")
 *  - or a symbol (e.g. LV_SYMBOL_CLOSE)
 * @return type of the image source LV_IMG_SRC_VARIABLE/FILE/SYMBOL/UNKNOWN
 */
lv_img_src_t lv_img_src_get_type(const void *src);

/**
 * Get the pixel size of a color format in bits
 * @param cf a color format (`LV_IMG_CF_...`)
 * @return the pixel size in bits
 */
uint8_t lv_img_cf_get_px_size(lv_img_cf_t cf);

/**
 * Check if a color format is chroma keyed or not
 * @param cf a color format (`LV_IMG_CF_...`)
 * @return 1: chroma keyed; 0: not chroma keyed
 */
int lv_img_cf_is_chroma_keyed(lv_img_cf_t cf);

/**
 * Check if a color format has alpha channel or not
 * @param cf a color format (`LV_IMG_CF_...`)
 * @return 1: has alpha channel; 0: doesn't have alpha channel
 */
int lv_img_cf_has_alpha(lv_img_cf_t cf);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_IMG_H*/
