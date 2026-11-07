#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <lvgl/src/draw/sdl/lv_draw_sdl.h>
#include <lvgl/src/draw/sdl/lv_draw_sdl_priv.h>
#include <common/base/options.h>
#include <common/display/theme.h>
#include <common/runtime/init.h>
#include <common/ui/image.h>
#include <common/ui/nav/cell_art.h>

#define CELL_ART_PANELS  16
#define CELL_ART_SOURCES 12

typedef struct {
    char item[64];
    int height;
    int found;
    int w, h;
    uint32_t *pixels;
    SDL_Texture *texture;
} art_source;

typedef struct {
    lv_obj_t *panel;
    char item[64];
    int w, h, slant, focused, native;
    char normal_path[MAX_BUFFER_SIZE];
    char focused_path[MAX_BUFFER_SIZE];
} art_panel;

static art_source sources[CELL_ART_SOURCES];
static art_panel panels[CELL_ART_PANELS];

static int find_art_path(const char *item, char *path, const size_t size) {
    const char *types[] = {"menu", "wall"};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (load_element_image_specifics(mux_dim, mux_module, types[i], item, item, "png", path, size)) return 1;
    }
    return 0;
}

static const art_source *source_for(const char *item, const int height) {
    art_source *free_slot = NULL;
    for (int i = 0; i < CELL_ART_SOURCES; i++) {
        art_source *source = &sources[i];
        if (!source->item[0]) {
            if (!free_slot) free_slot = source;
            continue;
        }
        if (source->height == height && strcmp(source->item, item) == 0) return source;
    }

    art_source *source = free_slot ? free_slot : &sources[0];
    if (source->texture) SDL_DestroyTexture(source->texture);
    free(source->pixels);
    memset(source, 0, sizeof(*source));
    snprintf(source->item, sizeof(source->item), "%s", item);
    source->height = height;

    char path[MAX_BUFFER_SIZE];
    if (!find_art_path(item, path, sizeof(path))) return source;

    SDL_Surface *loaded = IMG_Load(path);
    SDL_Surface *argb = loaded ? SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0) : NULL;
    SDL_FreeSurface(loaded);
    if (!argb || argb->h <= 0) {
        SDL_FreeSurface(argb);
        return source;
    }

    const int width = argb->w * height / argb->h;
    SDL_Surface *scaled =
        width > 0 ? SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (scaled) {
        SDL_SetSurfaceBlendMode(argb, SDL_BLENDMODE_NONE);
        SDL_BlitScaled(argb, NULL, scaled, NULL);
        source->pixels = malloc((size_t) width * height * sizeof(uint32_t));
        if (source->pixels) {
            for (int y = 0; y < height; y++) {
                memcpy(
                    source->pixels + (size_t) y * width, (uint8_t *) scaled->pixels + (size_t) y * scaled->pitch,
                    (size_t) width * sizeof(uint32_t)
                );
            }
            source->w = width;
            source->h = height;
            source->found = 1;
        }
        SDL_FreeSurface(scaled);
    }
    SDL_FreeSurface(argb);
    return source;
}

static void hide_glyphs(lv_obj_t *panel, const int hide) {
    for (int child = 0; child <= 2; child += 2) {
        lv_obj_t *image = lv_obj_get_child(panel, child);
        if (!image) continue;

        const lv_opa_t normal = (lv_opa_t) (child ? 0 : theme.grid.cell_default.image_alpha);
        lv_obj_set_style_img_opa(image, hide ? LV_OPA_TRANSP : normal, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_opa(
            image, hide ? LV_OPA_TRANSP : (lv_opa_t) theme.grid.cell_focus.image_alpha, MU_OBJ_MAIN_FOCUS
        );
    }
}

static void refresh(art_panel *art) {
    const art_source *source = art->item[0] && art->h > 0 ? source_for(art->item, art->h) : NULL;
    hide_glyphs(art->panel, source && source->found);
    lv_obj_invalidate(art->panel);
}

static SDL_Texture *source_texture(art_source *source, SDL_Renderer *renderer) {
    if (source->texture) return source->texture;

    source->texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, source->w, source->h);
    if (!source->texture) return NULL;

    SDL_UpdateTexture(source->texture, NULL, source->pixels, source->w * (int) sizeof(uint32_t));
    SDL_SetTextureBlendMode(source->texture, SDL_BLENDMODE_BLEND);
    return source->texture;
}

static void draw_slice(lv_draw_ctx_t *ctx, const art_panel *art, const lv_area_t *area, const lv_opa_t opa) {
    SDL_Renderer *renderer = ((lv_draw_sdl_ctx_t *) ctx)->renderer;
    const int w = lv_area_get_width(area);
    const int h = lv_area_get_height(area);
    if (!renderer || w <= 0 || h <= 0) return;

    art_source *source = art->item[0] ? (art_source *) source_for(art->item, art->h) : NULL;
    const int found = source && source->found;
    SDL_Texture *texture = found ? source_texture(source, renderer) : NULL;
    if (found && !texture) return;

    const uint32_t fill = art->focused ? theme.grid.cell_focus.background : theme.grid.cell_default.background;
    const int fill_alpha =
        art->focused ? theme.grid.cell_focus.background_alpha : theme.grid.cell_default.background_alpha;
    const SDL_Color colour =
        found ? (SDL_Color) {255, 255, 255, opa}
              : (SDL_Color) {(Uint8) (fill >> 16), (Uint8) (fill >> 8), (Uint8) fill, (Uint8) (fill_alpha * opa / 255)};

    const lv_draw_sdl_ctx_t *sdl = (const lv_draw_sdl_ctx_t *) ctx;
    const int layered = sdl->internals && sdl->internals->transform_count > 0;
    const lv_coord_t ox = layered ? ctx->buf_area->x1 : 0;
    const lv_coord_t oy = layered ? ctx->buf_area->y1 : 0;
    const float x0 = (float) (area->x1 - ox);
    const float y0 = (float) (area->y1 - oy);
    const float s = (float) (abs(art->slant) < w ? abs(art->slant) : w - 1);
    const float top = art->slant > 0 ? s : 0.0f;
    const float bottom = art->slant > 0 ? 0.0f : s;
    const float face = (float) w - s;

    const float crop = found ? (float) (source->w - w) / 2.0f : 0.0f;
    const float span = found ? (float) source->w : 1.0f;
    const float xs[] = {top, top + face, bottom + face, bottom};
    const float ys[] = {0.0f, 0.0f, (float) h, (float) h};
    SDL_Vertex vertices[4];
    for (int i = 0; i < 4; i++) {
        const float u = (crop + xs[i]) / span;
        vertices[i].position = (SDL_FPoint) {x0 + xs[i], y0 + ys[i]};
        vertices[i].color = colour;
        vertices[i].tex_coord = (SDL_FPoint) {u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u, ys[i] / (float) h};
    }

    SDL_Rect previous;
    const SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &previous);
    const SDL_Rect clip = {
        ctx->clip_area->x1 - ox, ctx->clip_area->y1 - oy, lv_area_get_width(ctx->clip_area),
        lv_area_get_height(ctx->clip_area)
    };
    SDL_RenderSetClipRect(renderer, &clip);

    static const int indices[] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, texture, vertices, 4, indices, 6);
    SDL_RenderSetClipRect(renderer, clipped ? &previous : NULL);
}

static void art_draw_cb(lv_event_t *e) {
    const art_panel *art = lv_event_get_user_data(e);
    if (!art || art->panel != lv_event_get_target(e)) return;

    lv_area_t area;
    lv_obj_get_coords(art->panel, &area);

    const lv_opa_t opa = lv_obj_get_style_opa_recursive(art->panel, LV_PART_MAIN);
    if (opa <= LV_OPA_MIN) return;

    lv_draw_img_dsc_t dsc;
    lv_draw_img_dsc_init(&dsc);
    dsc.opa = opa;
    if (art->native) {
        const int focused = lv_obj_has_state(art->panel, LV_STATE_FOCUSED) && art->focused_path[0];
        const char *src = focused ? art->focused_path : art->normal_path;
        if (src[0]) lv_draw_img(lv_event_get_draw_ctx(e), &dsc, &area, src);
        return;
    }

    draw_slice(lv_event_get_draw_ctx(e), art, &area, opa);
}

static int native_path(const char *element, char *embed) {
    char path[MAX_BUFFER_SIZE];
    if (!element[0]
        || !load_element_image_specifics(mux_dim, mux_module, "menu", element, element, "png", path, sizeof(path))) {
        embed[0] = '\0';
        return 0;
    }
    snprintf(embed, MAX_BUFFER_SIZE, "M:%s", path);
    return 1;
}

static int native_paths(art_panel *art) {
    char focused[MAX_BUFFER_SIZE];
    snprintf(focused, sizeof(focused), "%s_focused", art->item);
    native_path(focused, art->focused_path);
    return native_path(art->item, art->normal_path);
}

static void native_fit(art_panel *art) {
    lv_img_header_t header;
    if (lv_img_decoder_get_info(art->normal_path, &header) != LV_RES_OK || !header.w || !header.h) return;

    const lv_coord_t left = lv_obj_get_style_x(art->panel, LV_PART_MAIN);
    const lv_coord_t top = lv_obj_get_style_y(art->panel, LV_PART_MAIN);
    lv_obj_set_size(art->panel, (lv_coord_t) header.w, (lv_coord_t) header.h);
    const lv_coord_t x = (lv_coord_t) (left + (art->w - (int) header.w) / 2);
    lv_obj_set_pos(art->panel, x, (lv_coord_t) (top + (art->h - (int) header.h) / 2));
    hide_glyphs(art->panel, 1);

    lv_obj_t *label = lv_obj_get_child(art->panel, 1);
    lv_obj_t *focused_glyph = lv_obj_get_child(art->panel, 2);
    if (label) {
        lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, MU_OBJ_MAIN_FOCUS);
    }
    if (focused_glyph) lv_obj_add_flag(focused_glyph, LV_OBJ_FLAG_HIDDEN);
}

static art_panel *find_panel(const lv_obj_t *panel) {
    for (int i = 0; i < CELL_ART_PANELS; i++) {
        if (panels[i].panel == panel) return &panels[i];
    }
    return NULL;
}

void cell_art_apply(
    lv_obj_t *panel, const char *item, const int w, const int h, const int slant, const int focused, const int native
) {
    if (native) {
        art_panel probe = {0};
        snprintf(probe.item, sizeof(probe.item), "%s", item ? item : "");
        if (!native_paths(&probe)) return;
    }

    art_panel *art = find_panel(panel);
    for (int i = 0; !art && i < CELL_ART_PANELS; i++) {
        if (!panels[i].panel) art = &panels[i];
    }
    if (!art) return;

    if (art->panel != panel) lv_obj_add_event_cb(panel, art_draw_cb, LV_EVENT_DRAW_MAIN, art);
    art->panel = panel;

    const lv_opa_t layered = lv_obj_get_style_opa_layered(panel, LV_PART_MAIN);
    lv_obj_set_style_opa(panel, layered, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa_layered(panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    snprintf(art->item, sizeof(art->item), "%s", item ? item : "");
    art->w = w;
    art->h = h;
    art->slant = slant;
    art->focused = focused;
    art->native = native;

    lv_obj_set_style_radius(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_shadow_width(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(panel, 0, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(panel, 0, MU_OBJ_MAIN_FOCUS);
    if (native) {
        native_paths(art);
        native_fit(art);
        lv_obj_invalidate(panel);
        return;
    }
    refresh(art);
}

void cell_art_item(lv_obj_t *panel, const char *item) {
    art_panel *art = find_panel(panel);
    if (!art || strcmp(art->item, item ? item : "") == 0) return;

    snprintf(art->item, sizeof(art->item), "%s", item ? item : "");
    if (art->native) {
        native_paths(art);
        lv_obj_invalidate(panel);
        return;
    }
    refresh(art);
}

void cell_art_resize(lv_obj_t *panel, const int w, const int h) {
    art_panel *art = find_panel(panel);
    if (!art || art->native || (art->w == w && art->h == h)) return;

    art->w = w;
    art->h = h;
    lv_obj_invalidate(panel);
}

lv_opa_t cell_opa(const lv_obj_t *panel) {
    return find_panel(panel) ? lv_obj_get_style_opa(panel, LV_PART_MAIN)
                             : lv_obj_get_style_opa_layered(panel, LV_PART_MAIN);
}

void cell_set_opa(lv_obj_t *panel, const lv_opa_t opa) {
    if (find_panel(panel)) {
        lv_obj_set_style_opa(panel, opa, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_opa_layered(panel, opa, MU_OBJ_MAIN_DEFAULT);
    }
}

void cell_art_forget(void) {
    memset(panels, 0, sizeof(panels));
}
