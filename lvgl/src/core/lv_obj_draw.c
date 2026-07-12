/**
 * @file lv_obj_draw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_obj_draw.h"
#include "lv_obj.h"

extern int g_font_shadow_enabled;
extern lv_color_t g_shadow_colour_default;
extern lv_opa_t g_shadow_alpha_default;
extern int16_t g_shadow_x_offset_default;
extern int16_t g_shadow_y_offset_default;
extern lv_color_t g_shadow_colour_focus;
extern lv_opa_t g_shadow_alpha_focus;
extern int16_t g_shadow_x_offset_focus;
extern int16_t g_shadow_y_offset_focus;

extern int g_glyph_shadow_enabled;
extern lv_color_t g_glyph_shadow_colour_default;
extern lv_opa_t g_glyph_shadow_alpha_default;
extern int16_t g_glyph_shadow_x_offset_default;
extern int16_t g_glyph_shadow_y_offset_default;
extern lv_color_t g_glyph_shadow_colour_focus;
extern lv_opa_t g_glyph_shadow_alpha_focus;
extern int16_t g_glyph_shadow_x_offset_focus;
extern int16_t g_glyph_shadow_y_offset_focus;

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_obj_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

#define LV_SHADOW_ZONE_MAX 64

static lv_shadow_zone_t g_shadow_zones[LV_SHADOW_ZONE_MAX];
static int g_shadow_zone_count = 0;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

static void shadow_zone_delete_cb(lv_event_t *e) {
    lv_shadow_zone_unregister(lv_event_get_target(e));
}

void lv_shadow_zone_register(
    lv_obj_t *container, lv_color_t colour, lv_opa_t alpha, int8_t x, int8_t y, lv_color_t colour_focus,
    lv_opa_t alpha_focus, int8_t x_focus, int8_t y_focus
) {
    for (int i = 0; i < g_shadow_zone_count; i++) {
        if (g_shadow_zones[i].container == container) {
            g_shadow_zones[i].colour = colour;
            g_shadow_zones[i].alpha = alpha;
            g_shadow_zones[i].x_offset = x;
            g_shadow_zones[i].y_offset = y;
            g_shadow_zones[i].colour_focus = colour_focus;
            g_shadow_zones[i].alpha_focus = alpha_focus;
            g_shadow_zones[i].x_offset_focus = x_focus;
            g_shadow_zones[i].y_offset_focus = y_focus;
            return;
        }
    }
    if (g_shadow_zone_count < LV_SHADOW_ZONE_MAX) {
        g_shadow_zones[g_shadow_zone_count].container = container;
        g_shadow_zones[g_shadow_zone_count].colour = colour;
        g_shadow_zones[g_shadow_zone_count].alpha = alpha;
        g_shadow_zones[g_shadow_zone_count].x_offset = x;
        g_shadow_zones[g_shadow_zone_count].y_offset = y;
        g_shadow_zones[g_shadow_zone_count].colour_focus = colour_focus;
        g_shadow_zones[g_shadow_zone_count].alpha_focus = alpha_focus;
        g_shadow_zones[g_shadow_zone_count].x_offset_focus = x_focus;
        g_shadow_zones[g_shadow_zone_count].y_offset_focus = y_focus;
        g_shadow_zone_count++;

        lv_obj_add_event_cb(container, shadow_zone_delete_cb, LV_EVENT_DELETE, NULL);
    }
}

void lv_shadow_zone_unregister(lv_obj_t *container) {
    for (int i = 0; i < g_shadow_zone_count; i++) {
        if (g_shadow_zones[i].container == container) {
            g_shadow_zones[i] = g_shadow_zones[g_shadow_zone_count - 1];
            g_shadow_zone_count--;
            return;
        }
    }
}

const lv_shadow_zone_t *lv_shadow_zone_find(lv_obj_t *obj) {
    lv_obj_t *cur = obj;
    while (cur) {
        for (int i = 0; i < g_shadow_zone_count; i++) {
            if (g_shadow_zones[i].container == cur) return &g_shadow_zones[i];
        }
        cur = lv_obj_get_parent(cur);
    }
    return NULL;
}

void lv_obj_init_draw_rect_dsc(lv_obj_t *obj, uint32_t part, lv_draw_rect_dsc_t *draw_dsc) {
    lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, part);
    if (part != LV_PART_MAIN) {
        if (opa <= LV_OPA_MIN) {
            draw_dsc->bg_opa = LV_OPA_TRANSP;
            draw_dsc->bg_img_opa = LV_OPA_TRANSP;
            draw_dsc->border_opa = LV_OPA_TRANSP;
            draw_dsc->outline_opa = LV_OPA_TRANSP;
            draw_dsc->shadow_opa = LV_OPA_TRANSP;
            return;
        }
    }

#if LV_DRAW_COMPLEX
    if (part != LV_PART_MAIN) draw_dsc->blend_mode = lv_obj_get_style_blend_mode(obj, part);

    draw_dsc->radius = lv_obj_get_style_radius(obj, part);

    if (draw_dsc->bg_opa != LV_OPA_TRANSP) {
        draw_dsc->bg_opa = lv_obj_get_style_bg_opa(obj, part);
        if (draw_dsc->bg_opa > LV_OPA_MIN) {
            draw_dsc->bg_color = lv_obj_get_style_bg_color_filtered(obj, part);
            const lv_grad_dsc_t *grad = lv_obj_get_style_bg_grad(obj, part);
            if (grad && grad->dir != LV_GRAD_DIR_NONE) {
                lv_memcpy(&draw_dsc->bg_grad, grad, sizeof(*grad));
            } else {
                draw_dsc->bg_grad.dir = lv_obj_get_style_bg_grad_dir(obj, part);
                if (draw_dsc->bg_grad.dir != LV_GRAD_DIR_NONE) {
                    draw_dsc->bg_grad.stops[0].color = lv_obj_get_style_bg_color_filtered(obj, part);
                    draw_dsc->bg_grad.stops[1].color = lv_obj_get_style_bg_grad_color_filtered(obj, part);
                    draw_dsc->bg_grad.stops[0].frac = lv_obj_get_style_bg_main_stop(obj, part);
                    draw_dsc->bg_grad.stops[1].frac = lv_obj_get_style_bg_grad_stop(obj, part);
                }
                draw_dsc->bg_grad.dither = lv_obj_get_style_bg_dither_mode(obj, part);
            }
        }
    }

    draw_dsc->border_width = lv_obj_get_style_border_width(obj, part);
    if (draw_dsc->border_width) {
        if (draw_dsc->border_opa != LV_OPA_TRANSP) {
            draw_dsc->border_opa = lv_obj_get_style_border_opa(obj, part);
            if (draw_dsc->border_opa > LV_OPA_MIN) {
                draw_dsc->border_side = lv_obj_get_style_border_side(obj, part);
                draw_dsc->border_color = lv_obj_get_style_border_color_filtered(obj, part);
            }
        }
    }

    draw_dsc->outline_width = lv_obj_get_style_outline_width(obj, part);
    if (draw_dsc->outline_width) {
        if (draw_dsc->outline_opa != LV_OPA_TRANSP) {
            draw_dsc->outline_opa = lv_obj_get_style_outline_opa(obj, part);
            if (draw_dsc->outline_opa > LV_OPA_MIN) {
                draw_dsc->outline_pad = lv_obj_get_style_outline_pad(obj, part);
                draw_dsc->outline_color = lv_obj_get_style_outline_color_filtered(obj, part);
            }
        }
    }

    if (draw_dsc->bg_img_opa != LV_OPA_TRANSP) {
        draw_dsc->bg_img_src = lv_obj_get_style_bg_img_src(obj, part);
        if (draw_dsc->bg_img_src) {
            draw_dsc->bg_img_opa = lv_obj_get_style_bg_img_opa(obj, part);
            if (draw_dsc->bg_img_opa > LV_OPA_MIN) {
                if (lv_img_src_get_type(draw_dsc->bg_img_src) == LV_IMG_SRC_SYMBOL) {
                    draw_dsc->bg_img_symbol_font = lv_obj_get_style_text_font(obj, part);
                    draw_dsc->bg_img_recolor = lv_obj_get_style_text_color_filtered(obj, part);
                } else {
                    draw_dsc->bg_img_recolor = lv_obj_get_style_bg_img_recolor_filtered(obj, part);
                    draw_dsc->bg_img_recolor_opa = lv_obj_get_style_bg_img_recolor_opa(obj, part);
                    draw_dsc->bg_img_tiled = lv_obj_get_style_bg_img_tiled(obj, part);
                }
            }
        }
    }

    if (draw_dsc->shadow_opa) {
        draw_dsc->shadow_width = lv_obj_get_style_shadow_width(obj, part);
        if (draw_dsc->shadow_width) {
            if (draw_dsc->shadow_opa > LV_OPA_MIN) {
                draw_dsc->shadow_opa = lv_obj_get_style_shadow_opa(obj, part);
                if (draw_dsc->shadow_opa > LV_OPA_MIN) {
                    draw_dsc->shadow_ofs_x = lv_obj_get_style_shadow_ofs_x(obj, part);
                    draw_dsc->shadow_ofs_y = lv_obj_get_style_shadow_ofs_y(obj, part);
                    draw_dsc->shadow_spread = lv_obj_get_style_shadow_spread(obj, part);
                    draw_dsc->shadow_color = lv_obj_get_style_shadow_color_filtered(obj, part);
                }
            }
        }
    }

#else /*LV_DRAW_COMPLEX*/
    if (draw_dsc->bg_opa != LV_OPA_TRANSP) {
        draw_dsc->bg_opa = lv_obj_get_style_bg_opa(obj, part);
        if (draw_dsc->bg_opa > LV_OPA_MIN) {
            draw_dsc->bg_color = lv_obj_get_style_bg_color_filtered(obj, part);
        }
    }

    draw_dsc->border_width = lv_obj_get_style_border_width(obj, part);
    if (draw_dsc->border_width) {
        if (draw_dsc->border_opa != LV_OPA_TRANSP) {
            draw_dsc->border_opa = lv_obj_get_style_border_opa(obj, part);
            if (draw_dsc->border_opa > LV_OPA_MIN) {
                draw_dsc->border_color = lv_obj_get_style_border_color_filtered(obj, part);
                draw_dsc->border_side = lv_obj_get_style_border_side(obj, part);
            }
        }
    }

    draw_dsc->outline_width = lv_obj_get_style_outline_width(obj, part);
    if (draw_dsc->outline_width) {
        if (draw_dsc->outline_opa != LV_OPA_TRANSP) {
            draw_dsc->outline_opa = lv_obj_get_style_outline_opa(obj, part);
            if (draw_dsc->outline_opa > LV_OPA_MIN) {
                draw_dsc->outline_pad = lv_obj_get_style_outline_pad(obj, part);
                draw_dsc->outline_color = lv_obj_get_style_outline_color_filtered(obj, part);
            }
        }
    }

    if (draw_dsc->bg_img_opa != LV_OPA_TRANSP) {
        draw_dsc->bg_img_src = lv_obj_get_style_bg_img_src(obj, part);
        if (draw_dsc->bg_img_src) {
            draw_dsc->bg_img_opa = lv_obj_get_style_bg_img_opa(obj, part);
            if (draw_dsc->bg_img_opa > LV_OPA_MIN) {
                if (lv_img_src_get_type(draw_dsc->bg_img_src) == LV_IMG_SRC_SYMBOL) {
                    draw_dsc->bg_img_symbol_font = lv_obj_get_style_text_font(obj, part);
                    draw_dsc->bg_img_recolor = lv_obj_get_style_text_color_filtered(obj, part);
                } else {
                    draw_dsc->bg_img_recolor = lv_obj_get_style_bg_img_recolor_filtered(obj, part);
                    draw_dsc->bg_img_recolor_opa = lv_obj_get_style_bg_img_recolor_opa(obj, part);
                    draw_dsc->bg_img_tiled = lv_obj_get_style_bg_img_tiled(obj, part);
                }
            }
        }
    }
#endif
    if (opa < LV_OPA_MAX) {
        draw_dsc->bg_opa = (opa * draw_dsc->bg_opa) >> 8;
        draw_dsc->bg_img_opa = (opa * draw_dsc->bg_img_opa) >> 8;
        draw_dsc->border_opa = (opa * draw_dsc->border_opa) >> 8;
        draw_dsc->outline_opa = (opa * draw_dsc->outline_opa) >> 8;
        draw_dsc->shadow_opa = (opa * draw_dsc->shadow_opa) >> 8;
    }
}

void lv_obj_init_draw_label_dsc(lv_obj_t *obj, uint32_t part, lv_draw_label_dsc_t *draw_dsc) {
    draw_dsc->opa = lv_obj_get_style_text_opa(obj, part);
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, part);
    if (opa <= LV_OPA_MIN) {
        draw_dsc->opa = LV_OPA_TRANSP;
        return;
    }
    if (opa < LV_OPA_MAX) {
        draw_dsc->opa = (opa * draw_dsc->opa) >> 8;
    }
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    draw_dsc->color = lv_obj_get_style_text_color_filtered(obj, part);
    draw_dsc->letter_space = lv_obj_get_style_text_letter_space(obj, part);
    draw_dsc->line_space = lv_obj_get_style_text_line_space(obj, part);
    draw_dsc->decor = lv_obj_get_style_text_decor(obj, part);
#if LV_DRAW_COMPLEX
    if (part != LV_PART_MAIN) draw_dsc->blend_mode = lv_obj_get_style_blend_mode(obj, part);
#endif

    draw_dsc->font = lv_obj_get_style_text_font(obj, part);

#if LV_USE_BIDI
    draw_dsc->bidi_dir = lv_obj_get_style_base_dir(obj, LV_PART_MAIN);
#endif

    draw_dsc->align = lv_obj_get_style_text_align(obj, part);

    draw_dsc->effect_type = (int8_t) g_font_shadow_enabled;
    lv_state_t shadow_state = lv_obj_get_state(obj);

    if (!(shadow_state & LV_STATE_FOCUSED)) {
        lv_obj_t *shadow_parent = lv_obj_get_parent(obj);
        if (shadow_parent) shadow_state = lv_obj_get_state(shadow_parent);
    }

    if (shadow_state & LV_STATE_FOCUSED) {
        draw_dsc->effect_color = g_shadow_colour_focus;
        draw_dsc->effect_opa = g_shadow_alpha_focus;
        draw_dsc->effect_x_offset = (int8_t) g_shadow_x_offset_focus;
        draw_dsc->effect_y_offset = (int8_t) g_shadow_y_offset_focus;
    } else {
        draw_dsc->effect_color = g_shadow_colour_default;
        draw_dsc->effect_opa = g_shadow_alpha_default;
        draw_dsc->effect_x_offset = (int8_t) g_shadow_x_offset_default;
        draw_dsc->effect_y_offset = (int8_t) g_shadow_y_offset_default;
    }

    const lv_shadow_zone_t *zone = lv_shadow_zone_find(obj);
    if (zone) {
        if (lv_obj_get_state(obj) & LV_STATE_CHECKED) {
            draw_dsc->effect_color = zone->colour_focus;
            draw_dsc->effect_opa = zone->alpha_focus;
            draw_dsc->effect_x_offset = zone->x_offset_focus;
            draw_dsc->effect_y_offset = zone->y_offset_focus;
        } else {
            draw_dsc->effect_color = zone->colour;
            draw_dsc->effect_opa = zone->alpha;
            draw_dsc->effect_x_offset = zone->x_offset;
            draw_dsc->effect_y_offset = zone->y_offset;
        }
    }
}

void lv_obj_init_draw_img_dsc(lv_obj_t *obj, uint32_t part, lv_draw_img_dsc_t *draw_dsc) {
    draw_dsc->opa = lv_obj_get_style_img_opa(obj, part);
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, part);
    if (opa <= LV_OPA_MIN) {
        draw_dsc->opa = LV_OPA_TRANSP;
        return;
    }
    if (opa < LV_OPA_MAX) {
        draw_dsc->opa = (opa * draw_dsc->opa) >> 8;
    }
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    draw_dsc->angle = 0;
    draw_dsc->zoom = LV_IMG_ZOOM_NONE;
    draw_dsc->pivot.x = lv_area_get_width(&obj->coords) / 2;
    draw_dsc->pivot.y = lv_area_get_height(&obj->coords) / 2;

    draw_dsc->recolor_opa = lv_obj_get_style_img_recolor_opa(obj, part);
    if (draw_dsc->recolor_opa > 0) {
        draw_dsc->recolor = lv_obj_get_style_img_recolor_filtered(obj, part);
    }
#if LV_DRAW_COMPLEX
    if (part != LV_PART_MAIN) draw_dsc->blend_mode = lv_obj_get_style_blend_mode(obj, part);
#endif

    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1)) {
        draw_dsc->effect_type = 0;
    } else {
        draw_dsc->effect_type = (int8_t) g_glyph_shadow_enabled;
        lv_state_t shadow_state = lv_obj_get_state(obj);
        if (!(shadow_state & LV_STATE_FOCUSED)) {
            lv_obj_t *shadow_parent = lv_obj_get_parent(obj);
            if (shadow_parent) shadow_state = lv_obj_get_state(shadow_parent);
        }
        if (shadow_state & LV_STATE_FOCUSED) {
            draw_dsc->effect_color = g_glyph_shadow_colour_focus;
            draw_dsc->effect_opa = g_glyph_shadow_alpha_focus;
            draw_dsc->effect_x_offset = (int8_t) g_glyph_shadow_x_offset_focus;
            draw_dsc->effect_y_offset = (int8_t) g_glyph_shadow_y_offset_focus;
        } else {
            draw_dsc->effect_color = g_glyph_shadow_colour_default;
            draw_dsc->effect_opa = g_glyph_shadow_alpha_default;
            draw_dsc->effect_x_offset = (int8_t) g_glyph_shadow_x_offset_default;
            draw_dsc->effect_y_offset = (int8_t) g_glyph_shadow_y_offset_default;
        }
    }
}

void lv_obj_init_draw_line_dsc(lv_obj_t *obj, uint32_t part, lv_draw_line_dsc_t *draw_dsc) {
    draw_dsc->opa = lv_obj_get_style_line_opa(obj, part);
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, part);
    if (opa <= LV_OPA_MIN) {
        draw_dsc->opa = LV_OPA_TRANSP;
        return;
    }
    if (opa < LV_OPA_MAX) {
        draw_dsc->opa = (opa * draw_dsc->opa) >> 8;
    }
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    draw_dsc->width = lv_obj_get_style_line_width(obj, part);
    if (draw_dsc->width == 0) return;

    draw_dsc->color = lv_obj_get_style_line_color_filtered(obj, part);

    draw_dsc->dash_width = lv_obj_get_style_line_dash_width(obj, part);
    if (draw_dsc->dash_width) {
        draw_dsc->dash_gap = lv_obj_get_style_line_dash_gap(obj, part);
    }

    draw_dsc->round_start = lv_obj_get_style_line_rounded(obj, part);
    draw_dsc->round_end = draw_dsc->round_start;

#if LV_DRAW_COMPLEX
    if (part != LV_PART_MAIN) draw_dsc->blend_mode = lv_obj_get_style_blend_mode(obj, part);
#endif
}

void lv_obj_init_draw_arc_dsc(lv_obj_t *obj, uint32_t part, lv_draw_arc_dsc_t *draw_dsc) {
    draw_dsc->width = lv_obj_get_style_arc_width(obj, part);
    if (draw_dsc->width == 0) return;

    draw_dsc->opa = lv_obj_get_style_arc_opa(obj, part);
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    lv_opa_t opa = lv_obj_get_style_opa_recursive(obj, part);
    if (opa <= LV_OPA_MIN) {
        draw_dsc->opa = LV_OPA_TRANSP;
        return;
    }
    if (opa < LV_OPA_MAX) {
        draw_dsc->opa = (opa * draw_dsc->opa) >> 8;
    }
    if (draw_dsc->opa <= LV_OPA_MIN) return;

    draw_dsc->color = lv_obj_get_style_arc_color_filtered(obj, part);
    draw_dsc->img_src = lv_obj_get_style_arc_img_src(obj, part);

    draw_dsc->rounded = lv_obj_get_style_arc_rounded(obj, part);

#if LV_DRAW_COMPLEX
    if (part != LV_PART_MAIN) draw_dsc->blend_mode = lv_obj_get_style_blend_mode(obj, part);
#endif
}

lv_coord_t lv_obj_calculate_ext_draw_size(lv_obj_t *obj, uint32_t part) {
    lv_coord_t s = 0;

    lv_coord_t sh_width = lv_obj_get_style_shadow_width(obj, part);
    if (sh_width) {
        lv_opa_t sh_opa = lv_obj_get_style_shadow_opa(obj, part);
        if (sh_opa > LV_OPA_MIN) {
            sh_width = sh_width / 2 + 1; /*The blur adds only half width*/
            sh_width += lv_obj_get_style_shadow_spread(obj, part);
            lv_coord_t sh_ofs_x = lv_obj_get_style_shadow_ofs_x(obj, part);
            lv_coord_t sh_ofs_y = lv_obj_get_style_shadow_ofs_y(obj, part);
            sh_width += LV_MAX(LV_ABS(sh_ofs_x), LV_ABS(sh_ofs_y));
            s = LV_MAX(s, sh_width);
        }
    }

    lv_coord_t outline_width = lv_obj_get_style_outline_width(obj, part);
    if (outline_width) {
        lv_opa_t outline_opa = lv_obj_get_style_outline_opa(obj, part);
        if (outline_opa > LV_OPA_MIN) {
            lv_coord_t outline_pad = lv_obj_get_style_outline_pad(obj, part);
            s = LV_MAX(s, outline_pad + outline_width);
        }
    }

    lv_coord_t w = lv_obj_get_style_transform_width(obj, part);
    lv_coord_t h = lv_obj_get_style_transform_height(obj, part);
    lv_coord_t wh = LV_MAX(w, h);
    if (wh > 0) s += wh;

    return s;
}

void lv_obj_draw_dsc_init(lv_obj_draw_part_dsc_t *dsc, lv_draw_ctx_t *draw_ctx) {
    lv_memset_00(dsc, sizeof(lv_obj_draw_part_dsc_t));
    dsc->draw_ctx = draw_ctx;
}

int lv_obj_draw_part_check_type(lv_obj_draw_part_dsc_t *dsc, const lv_obj_class_t *class_p, uint32_t type) {
    if (dsc->class_p == class_p && dsc->type == type)
        return 1;
    else
        return 0;
}

void lv_obj_refresh_ext_draw_size(lv_obj_t *obj) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_coord_t s_old = _lv_obj_get_ext_draw_size(obj);
    lv_coord_t s_new = 0;
    lv_event_send(obj, LV_EVENT_REFR_EXT_DRAW_SIZE, &s_new);

    if (s_new != s_old) lv_obj_invalidate(obj);

    /*Store the result if the special attrs already allocated*/
    if (obj->spec_attr) {
        obj->spec_attr->ext_draw_size = s_new;
    }
    /*Allocate spec. attrs. only if the result is not zero.
     *Zero is the default value if the spec. attr. are not defined.*/
    else if (s_new != 0) {
        lv_obj_allocate_spec_attr(obj);
        obj->spec_attr->ext_draw_size = s_new;
    }

    if (s_new != s_old) lv_obj_invalidate(obj);
}

lv_coord_t _lv_obj_get_ext_draw_size(const lv_obj_t *obj) {
    if (obj->spec_attr)
        return obj->spec_attr->ext_draw_size;
    else
        return 0;
}

lv_layer_type_t _lv_obj_get_layer_type(const lv_obj_t *obj) {

    if (obj->spec_attr)
        return obj->spec_attr->layer_type;
    else
        return LV_LAYER_TYPE_NONE;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
