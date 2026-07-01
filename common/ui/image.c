#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../init.h"
#include "common.h"
#include "image.h"
#include "../video.h"
#include "../config.h"
#include "../device.h"
#include "../theme.h"
#include "../../module/muxshare.h"
#include "../../lvgl/src/draw/sdl/lv_draw_sdl_texture_cache.h"

char current_wall[MAX_BUFFER_SIZE];

static void read_image_dims(const char *path, int *w, int *h);

int load_element_image_specifics(
    const char *mux_dim, const char *program, const char *image_type, const char *element, const char *element_fallback,
    const char *image_extension, char *image_path, const size_t path_size
) {
    const char *dims[] = {mux_dim, ""};
    const char *elements[] = {element, element_fallback};

    const char *curr_lang = config.settings.general.language;

    for (size_t i = 0; i < A_SIZE(dims); ++i) {
        const char *paths[] = {"%s/%simage/%s/%s/%s/%s.%s", "%s/%simage/%s/%s/%s.%s"};
        for (size_t j = 0; j < A_SIZE(paths); ++j) {
            for (size_t k = 0; k < A_SIZE(elements); ++k) {
                int written;

                switch (j) {
                    case 0:
                        written = snprintf(
                            image_path, path_size, paths[j], theme_base, dims[i], curr_lang, image_type, program,
                            elements[k], image_extension
                        );
                        break;
                    case 1:
                    default:
                        written = snprintf(
                            image_path, path_size, paths[j], theme_base, dims[i], image_type, program, elements[k],
                            image_extension
                        );
                        break;
                }

                if (written >= 0 && file_exist(image_path)) return 1;
            }
        }
    }

    return 0;
}

int load_image_specifics(
    const char *mux_dim, const char *program, const char *image_type, const char *image_extension, char *image_path,
    const size_t path_size
) {
    const char *paths[] = {
        "%s/%simage/%s.%s", "%s/%simage/%s/%s/%s.%s", "%s/%simage/%s/%s.%s", "%s/%simage/%s/%s/default.%s",
        "%s/%simage/%s/default.%s"
    };

    const char *curr_lang = config.settings.general.language;

    for (size_t i = 0; i < A_SIZE(paths); ++i) {
        int written;

        switch (i) {
            case 0:
                written = snprintf(image_path, path_size, paths[i], theme_base, mux_dim, image_type, image_extension);
                break;
            case 1:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, curr_lang, image_type, program,
                    image_extension
                );
                break;
            case 2:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, image_type, program, image_extension
                );
                break;
            case 3:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, curr_lang, image_type, image_extension
                );
                break;
            case 4:
            default:
                written = snprintf(image_path, path_size, paths[i], theme_base, mux_dim, image_type, image_extension);
                break;
        }

        if (written >= 0 && file_exist(image_path)) return 1;
    }

    return 0;
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, const int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    const char *element = "";
    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
        if (e_focused != NULL) {
            const char *ud = lv_obj_get_user_data(e_focused);
            if (ud) element = ud;
        }
    }

    static char cached_theme_base[MAX_BUFFER_SIZE];
    static int cached_wall_type = -1;
    static int cached_video_wallpaper = -1;
    static char cached_program[MAX_BUFFER_SIZE];
    static char cached_element[MAX_BUFFER_SIZE];

    if (strcmp(theme_base, cached_theme_base) == 0 && wall_type == cached_wall_type
        && config.visual.video_wallpaper == cached_video_wallpaper && strcmp(program, cached_program) == 0
        && strcmp(element, cached_element) == 0) {
        return wall_image_embed;
    }

    snprintf(cached_theme_base, sizeof(cached_theme_base), "%s", theme_base);
    cached_wall_type = wall_type;
    cached_video_wallpaper = config.visual.video_wallpaper;
    snprintf(cached_program, sizeof(cached_program), "%s", program);
    snprintf(cached_element, sizeof(cached_element), "%s", element);
    wall_image_embed[0] = '\0';

#define TRY_EMBED(path_buf)                                                                                            \
    do {                                                                                                               \
        int embed_len = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", (path_buf));                      \
        if (embed_len < 0 || (size_t) embed_len >= sizeof(wall_image_embed)) wall_image_embed[0] = '\0';               \
    } while (0)

    if (config.visual.video_wallpaper) {
        const char *ad_dims[] = {mux_dim, ""};
        for (size_t dim_idx = 0; dim_idx < 2; dim_idx++) {
            int mp4_w;
            if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
                mp4_w = snprintf(
                    wall_image_path, sizeof(wall_image_path), "%s/%simage/wall/%s.mp4", theme_base, ad_dims[dim_idx],
                    program
                );
                if (mp4_w > 0 && (size_t) mp4_w < sizeof(wall_image_path) && file_exist(wall_image_path)) {
                    TRY_EMBED(wall_image_path);
                    return wall_image_embed;
                }
            }
            mp4_w = snprintf(
                wall_image_path, sizeof(wall_image_path), "%s/%simage/background.mp4", theme_base, ad_dims[dim_idx]
            );
            if (mp4_w > 0 && (size_t) mp4_w < sizeof(wall_image_path) && file_exist(wall_image_path)) {
                TRY_EMBED(wall_image_path);
                return wall_image_embed;
            }
        }
    }

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        const char *catalogue = NULL;
        switch (wall_type) {
            case wall_application:
                catalogue = "Application";
                break;
            case wall_archive:
                catalogue = "Archive";
                break;
            case wall_task:
                catalogue = "Task";
                break;
            default:
                break;
        }
        if (catalogue
            && load_image_catalogue(
                catalogue, element, "", "default", mux_dim, "wall", wall_image_path, sizeof(wall_image_path)
            )) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }

        if (load_element_image_specifics(
                mux_dim, program, "wall", strcmp(program, "muxlaunch") == 0 ? element : "default", "default", "png",
                wall_image_path, sizeof(wall_image_path)
            )) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }
    }

    if (load_image_specifics(mux_dim, program, "wall", "png", wall_image_path, sizeof(wall_image_path))
        || load_image_specifics("", program, "wall", "png", wall_image_path, sizeof(wall_image_path))) {
        TRY_EMBED(wall_image_path);
    } else {
        if (load_image_specifics(mux_dim, program, "wall", "svg", wall_image_path, sizeof(wall_image_path))
            || load_image_specifics("", program, "wall", "svg", wall_image_path, sizeof(wall_image_path))) {
            TRY_EMBED(wall_image_path);
        }
    }

#undef TRY_EMBED

    return wall_image_embed;
}

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_img_wall, const int wall_type) {
    static char new_wall[MAX_BUFFER_SIZE];
    snprintf(new_wall, sizeof(new_wall), "%s", get_wallpaper_path(ui_screen, ui_group, wall_type));

    if (strcasecmp(new_wall, current_wall) != 0) {
        snprintf(current_wall, sizeof(current_wall), "%s", new_wall);
        if (strlen(new_wall) > 3) {
            const size_t wall_len = strlen(new_wall);
            const int wall_is_mp4 = wall_len > 6 && strcasecmp(new_wall + wall_len - 4, ".mp4") == 0;
            if (wall_is_mp4) {
                const char *mp4_path = new_wall + 2;
                video_wallpaper_play(mp4_path);
                lv_img_set_src(ui_img_wall, &ui_img_blank);
                lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                set_gradient_visible(0);
            } else {
                if (video_wallpaper_active()) {
                    video_wallpaper_stop();
                    set_gradient_visible(1);
                }
                const size_t wlen = strlen(new_wall);
                if (wlen > 4 && strcmp(new_wall + wlen - 4, ".svg") == 0) {
                    char svg_wall[MAX_BUFFER_SIZE];
                    snprintf(svg_wall, sizeof(svg_wall), "%s?%dx%d", new_wall, device.mux.width, device.mux.height);
                    lv_img_set_src(ui_img_wall, svg_wall);
                } else {
                    lv_img_set_zoom(ui_img_wall, LV_IMG_ZOOM_NONE);
                    lv_img_set_src(ui_img_wall, new_wall);
                    if (config.visual.background_scale > 0) {
                        int iw = 0, ih = 0;
                        read_image_dims(new_wall + 2, &iw, &ih);
                        if (iw > 0 && ih > 0) {
                            const float wr = (float) device.mux.width / (float) iw;
                            const float hr = (float) device.mux.height / (float) ih;
                            const float zr = config.visual.background_scale == 2 ? (wr > hr ? wr : hr)
                                             : wr < hr                           ? wr
                                                                                 : hr;
                            uint16_t zoom = (uint16_t) (zr * (float) LV_IMG_ZOOM_NONE);
                            if (zoom < 1) zoom = 1;
                            lv_img_set_zoom(ui_img_wall, zoom);
                            lv_img_set_pivot(ui_img_wall, iw / 2, ih / 2);
                            lv_obj_align(ui_img_wall, LV_ALIGN_CENTER, 0, 0);
                        }
                    }
                }
            }
        } else {
            if (video_wallpaper_active()) {
                video_wallpaper_stop();
                set_gradient_visible(1);
            }
            lv_img_set_src(ui_img_wall, &ui_img_blank);
        }
    }
}

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, const int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        const char *element = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        switch (wall_type) {
            case wall_application:
                if (grid_mode_enabled && config.visual.box_art_hide) {
                    return "";
                }
                if (load_image_catalogue(
                        "Application", element, "", "default", mux_dim, "box", static_image_path,
                        sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_archive:
                if (load_image_catalogue(
                        "Archive", element, "", "default", mux_dim, "box", static_image_path, sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_task:
                if (load_image_catalogue(
                        "Task", element, "", "default", mux_dim, "box", static_image_path, sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_general:
            default:
                if (load_element_image_specifics(
                        mux_dim, program, "static", strcmp(program, "muxlaunch") == 0 ? element : "default", "default",
                        "png", static_image_path, sizeof(static_image_path)
                    )) {

                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
        }
    }

    return "";
}

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image) {
    if (config.visual.overlay_image == 0) return;

    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    switch (config.visual.overlay_image) {
        case 1:
            if (load_image_specifics(mux_dim, program, "overlay", "png", static_image_path, sizeof(static_image_path))
                || load_image_specifics("", program, "overlay", "png", static_image_path, sizeof(static_image_path))) {
                const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            } else {
                return;
            }
            break;
        default: {
            snprintf(
                static_image_path, sizeof(static_image_path), "%s/%s%d.png", STORAGE_OVERLAY, mux_dim,
                config.visual.overlay_image
            );
            if (!file_exist(static_image_path)) {
                snprintf(
                    static_image_path, sizeof(static_image_path), "%s/standard/%d.png", STORAGE_OVERLAY,
                    config.visual.overlay_image
                );
            }
            const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
            if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            break;
        }
    }

    if (!file_exist(static_image_path)) return;

    lv_obj_set_size(overlay_image, device.screen.width, device.screen.height);
    lv_obj_set_pos(overlay_image, 0, 0);
    lv_obj_set_style_bg_opa(overlay_image, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_src(overlay_image, static_image_embed, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_tiled(overlay_image, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_opa(overlay_image, config.visual.overlay_transparency, MU_OBJ_MAIN_DEFAULT);
    mu_img_no_shadow(overlay_image);
    lv_obj_move_foreground(overlay_image);
}

void load_overlay_image_sdl(void) {
    if (config.visual.overlay_image == 0) {
        display_clear_theme_overlay();
        return;
    }

    const char *program = lv_obj_get_user_data(ui_screen);
    char image_path[MAX_BUFFER_SIZE];

    switch (config.visual.overlay_image) {
        case 1:
            if (!load_image_specifics(mux_dim, program, "overlay", "png", image_path, sizeof(image_path))
                && !load_image_specifics("", program, "overlay", "png", image_path, sizeof(image_path))) {
                return;
            }
            break;
        default:
            snprintf(
                image_path, sizeof(image_path), "%s/%s%d.png", STORAGE_OVERLAY, mux_dim, config.visual.overlay_image
            );
            if (!file_exist(image_path)) {
                snprintf(
                    image_path, sizeof(image_path), "%s/standard/%d.png", STORAGE_OVERLAY, config.visual.overlay_image
                );
            }
            break;
    }

    if (!file_exist(image_path)) return;

    SDL_Texture *tex = display_load_png_texture(image_path);
    if (tex) display_set_theme_overlay(tex, (uint8_t) config.visual.overlay_transparency);
}

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (load_image_specifics(mux_dim, program, "kiosk", "png", static_image_path, sizeof(static_image_path))
        || load_image_specifics("", program, "kiosk", "png", static_image_path, sizeof(static_image_path))) {

        const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
        if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;

        lv_img_set_src(kiosk_image, static_image_embed);
        mu_img_no_shadow(kiosk_image);
        lv_obj_move_foreground(kiosk_image);
    }
}

int load_terminal_resource(const char *resource, const char *extension, char *buffer, const size_t size) {
    const char *dims[] = {mux_dim, ""};

    for (size_t i = 0; i < 2; i++) {
        snprintf(buffer, size, "%s/%s%s/muterm.%s", theme_base, dims[i], resource, extension);
        if (file_exist(buffer)) return 1;
    }

    return 0;
}

void unload_image_animation(void) {
    if (video_wallpaper_active()) {
        video_wallpaper_stop();
        set_gradient_visible(1);
    }
    current_wall[0] = '\0';
}

static void read_image_dims(const char *path, int *w, int *h) {
    *w = 0;
    *h = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;

    unsigned char buf[25];
    const size_t n = fread(buf, 1, sizeof(buf), f);

    if (n >= 24 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        *w = buf[16] << 24 | buf[17] << 16 | buf[18] << 8 | buf[19];
        *h = buf[20] << 24 | buf[21] << 16 | buf[22] << 8 | buf[23];
    } else if (n >= 2 && buf[0] == 0xFF && buf[1] == 0xD8) {
        unsigned char mk[2];
        fseek(f, 2, SEEK_SET);
        while (!(fread(mk, 1, 2, f) != 2 || mk[0] != 0xFF)) {

            if (mk[1] == 0xD9 || mk[1] == 0xDA) break;
            if (mk[1] == 0x01 || (mk[1] >= 0xD0 && mk[1] <= 0xD9)) continue;
            unsigned char lb[2];

            if (fread(lb, 1, 2, f) != 2) break;
            const int seg = lb[0] << 8 | lb[1];

            if (mk[1] >= 0xC0 && mk[1] <= 0xC3) {
                unsigned char sof[5];
                if (fread(sof, 1, 5, f) == 5) {
                    *h = sof[1] << 8 | sof[2];
                    *w = sof[3] << 8 | sof[4];
                }
                break;
            }

            if (seg < 2 || fseek(f, seg - 2, SEEK_CUR) != 0) break;
        }
    }

    fclose(f);
}

static void free_scaled_raster(lv_obj_t *ui_img_obj) {
    lv_img_dsc_t *old_dsc = lv_obj_get_user_data(ui_img_obj);
    if (old_dsc) {
        lv_img_cache_invalidate_src(old_dsc);

        const lv_disp_t *disp = lv_disp_get_default();
        if (disp && disp->driver && disp->driver->draw_ctx) {
            lv_draw_sdl_texture_cache_remove_src((lv_draw_sdl_ctx_t *) disp->driver->draw_ctx, old_dsc);
        }

        lv_img_buf_free(old_dsc);
        lv_obj_set_user_data(ui_img_obj, NULL);
    }
}

static void scale_and_set_raster(
    lv_obj_t *ui_img_obj, const char *image_path, const int tw, const int th, const int align, const int pad_l,
    const int pad_r, const int pad_t, const int pad_b
) {
    char lvgl_path[MAX_BUFFER_SIZE];
    snprintf(lvgl_path, sizeof(lvgl_path), "M:%s", image_path);

    lv_img_decoder_dsc_t decode_dsc = {0};
    if (lv_img_decoder_open(&decode_dsc, lvgl_path, lv_color_white(), 0) != LV_RES_OK) return;

    const int sw = (int) decode_dsc.header.w;
    const int sh = (int) decode_dsc.header.h;
    if (sw <= 0 || sh <= 0) {
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    uint8_t *src_buf = NULL;
    int src_allocated = 0;
    if (decode_dsc.img_data) {
        src_buf = (uint8_t *) decode_dsc.img_data;
    } else {
        src_buf = lv_mem_alloc((size_t) sw * sh * LV_IMG_PX_SIZE_ALPHA_BYTE);
        if (!src_buf) {
            lv_img_decoder_close(&decode_dsc);
            return;
        }

        src_allocated = 1;
        for (int y = 0; y < sh; y++) {
            if (lv_img_decoder_read_line(&decode_dsc, 0, y, sw, src_buf + (size_t) y * sw * LV_IMG_PX_SIZE_ALPHA_BYTE)
                != LV_RES_OK) {
                lv_mem_free(src_buf);
                lv_img_decoder_close(&decode_dsc);
                return;
            }
        }
    }

    lv_img_dsc_t *scaled_dsc = lv_img_buf_alloc(tw, th, LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (!scaled_dsc) {
        if (src_allocated) lv_mem_free(src_buf);
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    uint8_t *dst = (uint8_t *) scaled_dsc->data;
    for (int dy = 0; dy < th; dy++) {
        for (int dx = 0; dx < tw; dx++) {
            const int sx0 = dx * sw / tw;
            const int sy0 = dy * sh / th;

            int sx1 = (dx + 1) * sw / tw;
            int sy1 = (dy + 1) * sh / th;

            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;

            uint32_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0, n = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                for (int sx = sx0; sx < sx1; sx++) {
                    const uint8_t *px = src_buf + ((size_t) sy * sw + sx) * LV_IMG_PX_SIZE_ALPHA_BYTE;

                    acc0 += px[0];
                    acc1 += px[1];
                    acc2 += px[2];
                    acc3 += px[3];

                    n++;
                }
            }

            uint8_t *out = dst + ((size_t) dy * tw + dx) * LV_IMG_PX_SIZE_ALPHA_BYTE;

            out[0] = (uint8_t) (acc0 / n);
            out[1] = (uint8_t) (acc1 / n);
            out[2] = (uint8_t) (acc2 / n);
            out[3] = (uint8_t) (acc3 / n);
        }
    }

    if (src_allocated) lv_mem_free(src_buf);
    lv_img_decoder_close(&decode_dsc);

    lv_obj_set_user_data(ui_img_obj, scaled_dsc);
    lv_img_set_size_mode(ui_img_obj, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_img_set_zoom(ui_img_obj, LV_IMG_ZOOM_NONE);

    if (align >= 0) lv_obj_set_align(ui_img_obj, align);

    lv_obj_set_style_pad_left(ui_img_obj, pad_l, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_img_obj, pad_r, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_img_obj, pad_t, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_img_obj, pad_b, MU_OBJ_MAIN_DEFAULT);

    lv_img_set_src(ui_img_obj, scaled_dsc);
    lv_obj_move_foreground(ui_img_obj);
}

void update_image(lv_obj_t *ui_img_obj, const struct image_settings image_settings) {
    free_scaled_raster(ui_img_obj);

    if (file_exist(image_settings.image_path)) {
        const size_t plen = strlen(image_settings.image_path);
        const int is_svg = plen > 4 && strcmp(image_settings.image_path + plen - 4, ".svg") == 0;

        if (!is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            int iw = 0, ih = 0;
            read_image_dims(image_settings.image_path, &iw, &ih);
            if (iw > 0 && ih > 0) {
                const float wr = (float) image_settings.max_width / (float) iw;
                const float hr = (float) image_settings.max_height / (float) ih;

                const float zr = wr < hr ? wr : hr;

                const int tw = (int) ((float) iw * zr);
                const int th = (int) ((float) ih * zr);

                if (tw > 0 && th > 0) {
                    scale_and_set_raster(
                        ui_img_obj, image_settings.image_path, tw, th, image_settings.align, image_settings.pad_left,
                        image_settings.pad_right, image_settings.pad_top, image_settings.pad_bottom
                    );
                    return;
                }
            }
        }

        char image_path[MAX_BUFFER_SIZE];
        if (is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            build_embed_path(
                image_path, sizeof(image_path), image_settings.image_path, image_settings.max_width,
                image_settings.max_height
            );
        } else {
            snprintf(image_path, sizeof(image_path), "M:%s", image_settings.image_path);
        }

        lv_img_set_size_mode(ui_img_obj, LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_zoom(ui_img_obj, LV_IMG_ZOOM_NONE);

        if (image_settings.align >= 0) lv_obj_set_align(ui_img_obj, image_settings.align);
        lv_obj_set_style_pad_left(ui_img_obj, image_settings.pad_left, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(ui_img_obj, image_settings.pad_right, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(ui_img_obj, image_settings.pad_top, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_img_obj, image_settings.pad_bottom, MU_OBJ_MAIN_DEFAULT);

        lv_img_set_src(ui_img_obj, image_path);
        lv_obj_move_foreground(ui_img_obj);
    } else {
        lv_img_set_src(ui_img_obj, &ui_img_blank);
    }
}
