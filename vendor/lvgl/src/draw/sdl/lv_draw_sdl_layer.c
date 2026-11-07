/**
 * @file lv_draw_sdl_refr.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_conf_internal.h"

#if LV_USE_GPU_SDL

#include "../../core/lv_refr.h"

#include "lv_draw_sdl.h"
#include "lv_draw_sdl_priv.h"
#include "lv_draw_sdl_composite.h"
#include "lv_draw_sdl_utils.h"
#include "lv_draw_sdl_layer.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

#define LAYER_3D_COLS_MAX 64
#define LAYER_3D_ROWS_MAX 16
#define LAYER_3D_STEP 24
#define LAYER_3D_REFLECT_ROWS 8
#define LAYER_3D_REFLECT_HEIGHT 0.4f
#define LAYER_3D_FOLD_FADE 0.45f

typedef struct {
    float cos_x, sin_x, cos_y, sin_y, cos_z, sin_z;
    float cx, cy;
    float z_shift, focal;
    float ox, oy;
    float fold_half, fold_sin, fold_side;
    float zoom;
} layer_3d_view_t;

static float layer_3d_depth(const layer_3d_view_t *view, float x, float y) {
    const float px = (x - view->cx) * view->cos_z - (y - view->cy) * view->sin_z;
    const float py = (x - view->cx) * view->sin_z + (y - view->cy) * view->cos_z;
    const float z1 = px * view->sin_y;
    return z1 * view->cos_x - py * view->sin_x;
}

static SDL_FPoint layer_3d_project(const layer_3d_view_t *view, float x, float y) {
    const float px = (x - view->cx) * view->cos_z - (y - view->cy) * view->sin_z;
    const float py = (x - view->cx) * view->sin_z + (y - view->cy) * view->cos_z;

    if (view->fold_half > 0.0f) {
        /* Each half is a plane hinged at its outer edge and turned away, so text keeps its true proportions */
        const float from_edge = view->fold_side < 0.0f ? px + view->fold_half : view->fold_half - px;
        const float world_x = view->fold_side < 0.0f ? -view->fold_half + from_edge * view->cos_y
                                                     : view->fold_half - from_edge * view->cos_y;

        float distance = view->focal + from_edge * view->fold_sin + view->z_shift;
        if (distance < 1.0f) distance = 1.0f;
        const float scale = view->focal / distance * view->zoom;

        SDL_FPoint folded = {view->ox + view->cx + world_x * scale, view->oy + view->cy + py * scale};
        return folded;
    }

    const float x1 = px * view->cos_y;
    const float z1 = px * view->sin_y;
    const float y2 = py * view->cos_x + z1 * view->sin_x;
    const float z2 = z1 * view->cos_x - py * view->sin_x + view->z_shift;

    float distance = view->focal + z2;
    if (distance < 1.0f) distance = 1.0f;
    const float scale = view->focal / distance * view->zoom;

    SDL_FPoint out = {view->ox + view->cx + x1 * scale, view->oy + view->cy + y2 * scale};
    return out;
}

/* Draw a grid of small triangles so the affine texture mapping of each one stays close to true perspective.
 * The source rows run from src_y1 to src_y2, and are placed from dst_y1 to dst_y2 in object space, which lets the
 * reflection mirror the image. The alpha fades from alpha_top on the first row to alpha_bottom on the last. */
static void layer_3d_mesh(
    SDL_Renderer *renderer, SDL_Texture *texture, const layer_3d_view_t *view, float x1, float x2, float src_y1,
    float src_y2, float dst_y1, float dst_y2, int tex_w, int tex_h, int cols, int rows, Uint8 alpha_top,
    Uint8 alpha_bottom
) {
    static SDL_Vertex vertices[(LAYER_3D_COLS_MAX + 1) * (LAYER_3D_ROWS_MAX + 1)];
    static int indices[LAYER_3D_COLS_MAX * LAYER_3D_ROWS_MAX * 6];

    if (cols < 1) cols = 1;
    if (cols > LAYER_3D_COLS_MAX) cols = LAYER_3D_COLS_MAX;
    if (rows < 1) rows = 1;
    if (rows > LAYER_3D_ROWS_MAX) rows = LAYER_3D_ROWS_MAX;

    int v = 0;
    for (int row = 0; row <= rows; row++) {
        const float t = (float) row / (float) rows;
        const float src_y = src_y1 + (src_y2 - src_y1) * t;
        const float dst_y = dst_y1 + (dst_y2 - dst_y1) * t;
        const Uint8 alpha = (Uint8) ((float) alpha_top + ((float) alpha_bottom - (float) alpha_top) * t);

        for (int col = 0; col <= cols; col++) {
            const float x = x1 + (x2 - x1) * (float) col / (float) cols;
            vertices[v].position = layer_3d_project(view, x, dst_y);
            vertices[v].color.r = 0xFF;
            vertices[v].color.g = 0xFF;
            vertices[v].color.b = 0xFF;
            vertices[v].color.a = alpha;

            /* A fold fades into its crease so each side reads as its own column */
            if (view->fold_half > 0.0f) {
                const float crease = SDL_fabsf(x - view->cx) / (view->fold_half * LAYER_3D_FOLD_FADE);
                if (crease < 1.0f) vertices[v].color.a = (Uint8) ((float) alpha * crease);
            }
            vertices[v].tex_coord.x = x / (float) tex_w;
            vertices[v].tex_coord.y = src_y / (float) tex_h;
            v++;
        }
    }

    int i = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const int a = row * (cols + 1) + col;
            const int b = a + 1;
            const int c = a + cols + 1;
            const int d = c + 1;
            indices[i++] = a;
            indices[i++] = b;
            indices[i++] = c;
            indices[i++] = b;
            indices[i++] = d;
            indices[i++] = c;
        }
    }

    SDL_RenderGeometry(renderer, texture, vertices, v, indices, i);
}

/* A fold is drawn as two meshes, because the crease vertex sits at a different place on each side */
static void layer_3d_draw(
    SDL_Renderer *renderer, SDL_Texture *texture, layer_3d_view_t *view, float x1, float x2, float src_y1,
    float src_y2, float dst_y1, float dst_y2, int tex_w, int tex_h, int cols, int rows, Uint8 alpha_top,
    Uint8 alpha_bottom
) {
    if (view->fold_half <= 0.0f) {
        layer_3d_mesh(
            renderer, texture, view, x1, x2, src_y1, src_y2, dst_y1, dst_y2, tex_w, tex_h, cols, rows, alpha_top,
            alpha_bottom
        );
        return;
    }

    const float crease = view->cx < x1 ? x1 : view->cx > x2 ? x2 : view->cx;
    const int half_cols = cols / 2 > 0 ? cols / 2 : 1;

    view->fold_side = -1.0f;
    layer_3d_mesh(
        renderer, texture, view, x1, crease, src_y1, src_y2, dst_y1, dst_y2, tex_w, tex_h, half_cols, rows,
        alpha_top, alpha_bottom
    );

    view->fold_side = 1.0f;
    layer_3d_mesh(
        renderer, texture, view, crease, x2, src_y1, src_y2, dst_y1, dst_y2, tex_w, tex_h, half_cols, rows,
        alpha_top, alpha_bottom
    );
}

static void layer_3d_blend(
    SDL_Renderer *renderer, lv_draw_sdl_layer_ctx_t *transform_ctx, const lv_draw_img_dsc_t *draw_dsc,
    const SDL_Rect *dest
) {
    int tex_w = 0;
    int tex_h = 0;
    if (SDL_QueryTexture(transform_ctx->target, NULL, NULL, &tex_w, &tex_h) != 0 || tex_w <= 0 || tex_h <= 0) return;

    const lv_area_t *coords = &draw_dsc->coords_3d;
    const float obj_x1 = (float) coords->x1;
    const float obj_y1 = (float) coords->y1;
    const float obj_x2 = (float) coords->x2 + 1.0f;
    const float obj_y2 = (float) coords->y2 + 1.0f;
    const float obj_w = obj_x2 - obj_x1;
    const float obj_h = obj_y2 - obj_y1;

    const float rad_x = (float) draw_dsc->rotate_x * 0.1f * (float) M_PI / 180.0f;
    const float rad_y = (float) draw_dsc->rotate_y * 0.1f * (float) M_PI / 180.0f;

    layer_3d_view_t view;
    view.cos_x = SDL_cosf(rad_x);
    view.sin_x = SDL_sinf(rad_x);
    view.cos_y = SDL_cosf(rad_y);
    view.sin_y = SDL_sinf(rad_y);
    const float rad_z = (float) draw_dsc->angle * 0.1f * (float) M_PI / 180.0f;
    view.cos_z = SDL_cosf(rad_z);
    view.sin_z = SDL_sinf(rad_z);
    view.cx = (obj_x1 + obj_x2) / 2.0f;
    view.cy = (obj_y1 + obj_y2) / 2.0f;
    view.ox = (float) dest->x;
    view.oy = (float) dest->y;
    view.focal = draw_dsc->perspective > 0 ? (float) draw_dsc->perspective : 2.0f * (obj_w > obj_h ? obj_w : obj_h);
    view.fold_half = 0.0f;
    view.fold_sin = 0.0f;
    view.fold_side = 0.0f;
    view.zoom = draw_dsc->zoom > 0 ? (float) draw_dsc->zoom / LV_IMG_ZOOM_NONE : 1.0f;

    /* Keep the nearest corner of the object on the screen plane so the projection never grows past its own area */
    float nearest = layer_3d_depth(&view, obj_x1, obj_y1);
    const float corners[3][2] = {{obj_x2, obj_y1}, {obj_x1, obj_y2}, {obj_x2, obj_y2}};
    for (int c = 0; c < 3; c++) {
        const float z = layer_3d_depth(&view, corners[c][0], corners[c][1]);
        if (z < nearest) nearest = z;
    }
    view.z_shift = (float) draw_dsc->depth - nearest;

    if (draw_dsc->anchor_3d == 2) {
        /* Fold down the middle: both side edges stay on the screen plane and the centre recedes */
        view.fold_half = obj_w / 2.0f;
        view.fold_sin = view.sin_y < 0.0f ? -view.sin_y : view.sin_y;
        view.z_shift = (float) draw_dsc->depth;
    } else if (draw_dsc->anchor_3d) {
        const float ref_x = view.sin_y > 0.0f ? obj_x1 : view.sin_y < 0.0f ? obj_x2 : view.cx;
        const float ref_y = view.sin_x > 0.0f ? obj_y2 : view.sin_x < 0.0f ? obj_y1 : view.cy;
        const SDL_FPoint projected = layer_3d_project(&view, ref_x, ref_y);
        view.ox += view.ox + ref_x - projected.x;
        view.oy += view.oy + ref_y - projected.y;
    }

    SDL_ScaleMode scale_mode = SDL_ScaleModeNearest;
    SDL_GetTextureScaleMode(transform_ctx->target, &scale_mode);
    SDL_SetTextureScaleMode(transform_ctx->target, SDL_ScaleModeLinear);
    SDL_SetTextureAlphaMod(transform_ctx->target, 0xFF);

    const float layer_w = (float) transform_ctx->target_rect.w;
    const float layer_h = (float) transform_ctx->target_rect.h;

    /* Perspective changes along the turned axis, so only that axis needs a fine mesh */
    const int cols = (draw_dsc->rotate_y ? (int) (layer_w / LAYER_3D_STEP) : 4) & ~1;
    const int rows = draw_dsc->rotate_x ? (int) (layer_h / LAYER_3D_STEP) : 4;

    if (draw_dsc->reflect) {
        const float reflect_h = obj_h * LAYER_3D_REFLECT_HEIGHT;
        const Uint8 reflect_opa = (Uint8) ((draw_dsc->reflect * draw_dsc->opa) / 255);
        layer_3d_draw(
            renderer, transform_ctx->target, &view, obj_x1, obj_x2, obj_y2, obj_y2 - reflect_h, obj_y2,
            obj_y2 + reflect_h, tex_w, tex_h, cols, LAYER_3D_REFLECT_ROWS, reflect_opa, 0
        );
    }

    float flat = (float) draw_dsc->flat_top_3d;
    if (flat > layer_h) flat = layer_h;
    if (flat > 0.0f) {
        const SDL_Rect flat_src = {0, 0, transform_ctx->target_rect.w, (int) flat};
        const SDL_Rect flat_dst = {dest->x, dest->y, transform_ctx->target_rect.w, (int) flat};
        SDL_SetTextureAlphaMod(transform_ctx->target, draw_dsc->opa);
        SDL_RenderCopy(renderer, transform_ctx->target, &flat_src, &flat_dst);
        SDL_SetTextureAlphaMod(transform_ctx->target, 0xFF);
    }

    layer_3d_draw(
        renderer, transform_ctx->target, &view, 0.0f, layer_w, flat, layer_h, flat, layer_h, tex_w, tex_h, cols,
        rows < 8 && draw_dsc->rotate_x ? 8 : rows, draw_dsc->opa, draw_dsc->opa
    );

    /* Rendering is batched, so draw now while the texture still samples smoothly */
    SDL_RenderFlush(renderer);
    SDL_SetTextureScaleMode(transform_ctx->target, scale_mode);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_draw_layer_ctx_t *
lv_draw_sdl_layer_init(lv_draw_ctx_t *draw_ctx, lv_draw_layer_ctx_t *layer_ctx, lv_draw_layer_flags_t flags) {
    lv_draw_sdl_ctx_t *ctx = (lv_draw_sdl_ctx_t *) draw_ctx;
    SDL_Renderer *renderer = ctx->renderer;

    lv_draw_sdl_layer_ctx_t *transform_ctx = (lv_draw_sdl_layer_ctx_t *) layer_ctx;

    transform_ctx->flags = flags;
    transform_ctx->orig_target = SDL_GetRenderTarget(renderer);

    lv_coord_t target_w = lv_area_get_width(&layer_ctx->area_full);
    lv_coord_t target_h = lv_area_get_height(&layer_ctx->area_full);

    enum lv_draw_sdl_composite_texture_id_t texture_id =
        LV_DRAW_SDL_COMPOSITE_TEXTURE_ID_TRANSFORM0 + ctx->internals->transform_count;
    transform_ctx->target =
        lv_draw_sdl_composite_texture_obtain(ctx, texture_id, target_w, target_h, &transform_ctx->target_in_cache);
    transform_ctx->target_rect.x = 0;
    transform_ctx->target_rect.y = 0;
    transform_ctx->target_rect.w = target_w;
    transform_ctx->target_rect.h = target_h;

    layer_ctx->max_row_with_alpha = target_h;
    layer_ctx->max_row_with_no_alpha = target_h;

    SDL_SetTextureBlendMode(transform_ctx->target, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, transform_ctx->target);

    /* SDL_RenderClear is not working properly, so we overwrite the target with solid color */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    /* Set proper drawing context for transform layer */
    ctx->internals->transform_count += 1;
    draw_ctx->buf_area = &layer_ctx->area_full;
    draw_ctx->clip_area = &layer_ctx->area_full;

    return layer_ctx;
}

void lv_draw_sdl_layer_blend(
    lv_draw_ctx_t *draw_ctx, lv_draw_layer_ctx_t *layer_ctx, const lv_draw_img_dsc_t *draw_dsc
) {
    lv_draw_sdl_ctx_t *ctx = (lv_draw_sdl_ctx_t *) draw_ctx;
    lv_draw_sdl_layer_ctx_t *transform_ctx = (lv_draw_sdl_layer_ctx_t *) layer_ctx;

    SDL_Renderer *renderer = ctx->renderer;

    SDL_Rect trans_rect;

    if (transform_ctx->flags & LV_DRAW_LAYER_FLAG_CAN_SUBDIVIDE) {
        lv_area_zoom_to_sdl_rect(&layer_ctx->area_act, &trans_rect, draw_dsc->zoom, &draw_dsc->pivot);
    } else {
        lv_area_zoom_to_sdl_rect(&layer_ctx->area_full, &trans_rect, draw_dsc->zoom, &draw_dsc->pivot);
    }

    SDL_SetRenderTarget(renderer, transform_ctx->orig_target);

    /*Render off-screen texture, transformed*/
    SDL_Rect clip_rect;
    lv_area_to_sdl_rect(layer_ctx->original.clip_area, &clip_rect);
    SDL_Point center = {.x = draw_dsc->pivot.x, .y = draw_dsc->pivot.y};

    if (draw_dsc->rotate_x || draw_dsc->rotate_y || draw_dsc->depth || draw_dsc->reflect) {
        lv_area_t clip_3d;
        if (_lv_area_intersect(&clip_3d, layer_ctx->original.clip_area, &layer_ctx->area_full)) {
            lv_area_to_sdl_rect(&layer_ctx->area_full, &trans_rect);
            lv_area_to_sdl_rect(&clip_3d, &clip_rect);
            SDL_RenderSetClipRect(renderer, &clip_rect);
            layer_3d_blend(renderer, transform_ctx, draw_dsc, &trans_rect);
            SDL_RenderSetClipRect(renderer, NULL);
        }
        return;
    }

    SDL_RenderSetClipRect(renderer, &clip_rect);
    SDL_SetTextureAlphaMod(transform_ctx->target, draw_dsc->opa);
    SDL_RenderCopyEx(
        renderer, transform_ctx->target, &transform_ctx->target_rect, &trans_rect, draw_dsc->angle / 10.0, &center,
        SDL_FLIP_NONE
    );
    SDL_RenderSetClipRect(renderer, NULL);
}

void lv_draw_sdl_layer_destroy(lv_draw_ctx_t *draw_ctx, lv_draw_layer_ctx_t *layer_ctx) {
    lv_draw_sdl_ctx_t *ctx = (lv_draw_sdl_ctx_t *) draw_ctx;
    lv_draw_sdl_layer_ctx_t *transform_ctx = (lv_draw_sdl_layer_ctx_t *) layer_ctx;
    if (!transform_ctx->target_in_cache && transform_ctx->target != NULL) {
        LV_LOG_WARN("Texture is not cached, this will impact performance.");
        SDL_DestroyTexture(transform_ctx->target);
    }
    ctx->internals->transform_count -= 1;
}

void lv_draw_sdl_transform_areas_offset(
    lv_draw_sdl_ctx_t *ctx, int has_composite, lv_area_t *apply_area, lv_area_t *coords, lv_area_t *clip
) {
    if (ctx->internals->transform_count == 0) {
        return;
    }
    lv_area_t *area = ctx->base_draw.buf_area;
    lv_area_move(coords, -area->x1, -area->y1);
    lv_area_move(clip, -area->x1, -area->y1);
    if (has_composite) {
        lv_area_move(apply_area, -area->x1, -area->y1);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_GPU_SDL*/
