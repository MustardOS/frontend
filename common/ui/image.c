#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../init.h"
#include "common.h"
#include "image.h"
#include "../anim.h"
#include "../log.h"
#include "../config.h"
#include "../device.h"
#include "../theme.h"
#include "../../module/muxshare.h"

char current_wall[MAX_BUFFER_SIZE];

static lv_obj_t *img_obj;
static char **img_paths = NULL;
static int img_paths_count = 0;
static lv_obj_t *wall_img = NULL;
static lv_anim_t animation;

int load_element_image_specifics(const char *mux_dim, const char *program, const char *image_type,
                                 const char *element, const char *element_fallback,
                                 const char *image_extension, char *image_path, size_t path_size) {
    const char *paths[] = {
            "%s/%simage/%s/%s/%s/%s.%s",
            "%s/%simage/%s/%s/%s.%s"
    };
    const char *dims[] = {mux_dim, ""};
    const char *elements[] = {element, element_fallback};

    const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;

    for (size_t i = 0; i < A_SIZE(dims); ++i) {
        for (size_t j = 0; j < A_SIZE(paths); ++j) {
            for (size_t k = 0; k < A_SIZE(elements); ++k) {
                int written;

                switch (j) {
                    case 0:
                        written = snprintf(image_path, path_size, paths[j], theme_base,
                                           dims[i], curr_lang, image_type, program, elements[k], image_extension);
                        break;
                    case 1:
                    default:
                        written = snprintf(image_path, path_size, paths[j], theme_base,
                                           dims[i], image_type, program, elements[k], image_extension);
                        break;
                }

                if (written >= 0 && file_exist(image_path)) return 1;
            }
        }
    }

    return 0;
}

int load_image_specifics(const char *mux_dim, const char *program, const char *image_type,
                         const char *image_extension, char *image_path, size_t path_size) {
    const char *paths[] = {
            "%s/%simage/%s.%s",
            "%s/%simage/%s/%s/%s.%s",
            "%s/%simage/%s/%s.%s",
            "%s/%simage/%s/%s/default.%s",
            "%s/%simage/%s/default.%s"
    };

    const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;

    for (size_t i = 0; i < A_SIZE(paths); ++i) {
        int written;

        switch (i) {
            case 0:
                written = snprintf(image_path, path_size, paths[i], theme_base,
                                   mux_dim, image_type, image_extension);
                break;
            case 1:
                written = snprintf(image_path, path_size, paths[i], theme_base,
                                   mux_dim, curr_lang, image_type, program, image_extension);
                break;
            case 2:
                written = snprintf(image_path, path_size, paths[i], theme_base,
                                   mux_dim, image_type, program, image_extension);
                break;
            case 3:
                written = snprintf(image_path, path_size, paths[i], theme_base,
                                   mux_dim, curr_lang, image_type, image_extension);
                break;
            case 4:
            default:
                written = snprintf(image_path, path_size, paths[i], theme_base,
                                   mux_dim, image_type, image_extension);
                break;
        }

        if (written >= 0 && file_exist(image_path)) return 1;
    }

    return 0;
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, int animated, int random, int wall_type) {
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

    static const char *cached_theme_base;
    static int cached_animated = -1, cached_random = -1, cached_wall_type = -1;
    static char cached_program[MAX_BUFFER_SIZE];
    static char cached_element[MAX_BUFFER_SIZE];

    if (theme_base == cached_theme_base &&
        animated == cached_animated &&
        random == cached_random &&
        wall_type == cached_wall_type &&
        strcmp(program, cached_program) == 0 &&
        strcmp(element, cached_element) == 0) {
        return wall_image_embed;
    }

    cached_theme_base = theme_base;
    cached_animated = animated;
    cached_random = random;
    cached_wall_type = wall_type;
    snprintf(cached_program, sizeof(cached_program), "%s", program);
    snprintf(cached_element, sizeof(cached_element), "%s", element);
    wall_image_embed[0] = '\0';

    const char *wall_extension = random ? "0.png" : (animated == 1 ? "gif" : (animated == 2 ? "0.png" : "png"));

#define TRY_EMBED(path_buf)                                                                \
    do {                                                                                   \
        int _w = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", (path_buf)); \
        if (_w < 0 || (size_t)_w >= sizeof(wall_image_embed)) wall_image_embed[0] = '\0';  \
    } while (0)

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        const char *catalogue = NULL;
        switch (wall_type) {
            case WALL_APPLICATION:
                catalogue = "Application";
                break;
            case WALL_ARCHIVE:
                catalogue = "Archive";
                break;
            case WALL_TASK:
                catalogue = "Task";
                break;
            default:
                break;
        }
        if (catalogue && load_image_catalogue(catalogue, element, "", "default", mux_dim, "wall", wall_image_path, sizeof(wall_image_path))) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }

        if (load_element_image_specifics(mux_dim, program, "wall", strcmp(program, "muxlaunch") == 0 ? element : "default",
                                         "default", wall_extension, wall_image_path, sizeof(wall_image_path))) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }
    }

    if (load_image_specifics(mux_dim, program, "wall", wall_extension, wall_image_path, sizeof(wall_image_path)) ||
        load_image_specifics("", program, "wall", wall_extension, wall_image_path, sizeof(wall_image_path))) {
        TRY_EMBED(wall_image_path);
    } else if (animated == 0 && !random) {
        if (load_image_specifics(mux_dim, program, "wall", "svg", wall_image_path, sizeof(wall_image_path)) ||
            load_image_specifics("", program, "wall", "svg", wall_image_path, sizeof(wall_image_path))) {
            TRY_EMBED(wall_image_path);
        }
    }

#undef TRY_EMBED

    return wall_image_embed;
}

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_pnlWall,
                    lv_obj_t *ui_imgWall, int wall_type) {
    static char new_wall[MAX_BUFFER_SIZE];
    snprintf(new_wall, sizeof(new_wall), "%s", get_wallpaper_path(
            ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND, wall_type));

    if (strcasecmp(new_wall, current_wall) != 0) {
        snprintf(current_wall, sizeof(current_wall), "%s", new_wall);
        if (strlen(new_wall) > 3) {
            if (theme.MISC.RANDOM_BACKGROUND) {
                load_image_random(ui_imgWall, new_wall);
            } else {
                switch (theme.MISC.ANIMATED_BACKGROUND) {
                    case 1:
                        wall_img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(wall_img, new_wall);
                        break;
                    case 2:
                        if (config.VISUAL.BACKGROUNDANIMATION) {
                            int fg = theme.ANIMATION.ANIMATION_FOREGROUND;
                            int pos = theme.ANIMATION.ANIMATION_POSITION;
                            int alpha = theme.ANIMATION.ANIMATION_ALPHA;
                            anim_request(new_wall, theme.ANIMATION.ANIMATION_DELAY, fg, pos, alpha);
                            lv_img_set_src(ui_imgWall, &ui_img_blank);
                            if (!fg) {
                                lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                                lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                                set_gradient_visible(0);
                            }
                        } else {
                            size_t wlen2 = strlen(new_wall);
                            if (wlen2 > 4 && strcmp(new_wall + wlen2 - 4, ".svg") == 0) {
                                char svg_wall[MAX_BUFFER_SIZE];
                                snprintf(svg_wall, sizeof(svg_wall), "%s?%dx%d", new_wall, device.MUX.WIDTH, device.MUX.HEIGHT);
                                lv_img_set_src(ui_imgWall, svg_wall);
                            } else {
                                lv_img_set_src(ui_imgWall, new_wall);
                            }
                        }
                        break;
                    default: {
                        size_t wlen = strlen(new_wall);
                        if (wlen > 4 && strcmp(new_wall + wlen - 4, ".svg") == 0) {
                            char svg_wall[MAX_BUFFER_SIZE];
                            snprintf(svg_wall, sizeof(svg_wall), "%s?%dx%d", new_wall, device.MUX.WIDTH, device.MUX.HEIGHT);
                            lv_img_set_src(ui_imgWall, svg_wall);
                        } else {
                            lv_img_set_src(ui_imgWall, new_wall);
                        }
                        break;
                    }
                }
            }
        } else {
            unload_image_animation();
            lv_img_set_src(ui_imgWall, &ui_img_blank);
        }
    }
}

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        const char *element = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        switch (wall_type) {
            case WALL_APPLICATION:
                if (grid_mode_enabled && config.VISUAL.BOX_ART_HIDE) {
                    return "";
                }
                if (load_image_catalogue("Application", element, "", "default", mux_dim, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case WALL_ARCHIVE:
                if (load_image_catalogue("Archive", element, "", "default", mux_dim, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case WALL_TASK:
                if (load_image_catalogue("Task", element, "", "default", mux_dim, "box",
                                         static_image_path, sizeof(static_image_path))) {
                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case WALL_GENERAL:
            default:
                if (load_element_image_specifics(mux_dim, program, "static",
                                                 strcmp(program, "muxlaunch") == 0 ? element : "default",
                                                 "default", "png", static_image_path,
                                                 sizeof(static_image_path))) {

                    int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s",
                                           static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
        }
    }

    return "";
}

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image) {
    if (config.VISUAL.OVERLAYIMAGE == 0) return;

    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    switch (config.VISUAL.OVERLAYIMAGE) {
        case 1:
            if (load_image_specifics(mux_dim, program, "overlay", "png",
                                     static_image_path, sizeof(static_image_path)) ||
                load_image_specifics("", program, "overlay", "png",
                                     static_image_path, sizeof(static_image_path))) {
                int written = snprintf(static_image_embed, sizeof(static_image_embed),
                                       "M:%s", static_image_path);
                if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            } else {
                return;
            }
            break;
        default: {
            snprintf(static_image_path, sizeof(static_image_path), "%s/%s%d.png",
                     STORAGE_OVERLAY, mux_dim, config.VISUAL.OVERLAYIMAGE);
            if (!file_exist(static_image_path)) {
                snprintf(static_image_path, sizeof(static_image_path), "%s/standard/%d.png",
                         STORAGE_OVERLAY, config.VISUAL.OVERLAYIMAGE);
            }
            int written = snprintf(static_image_embed, sizeof(static_image_embed),
                                   "M:%s", static_image_path);
            if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            break;
        }
    }

    if (!file_exist(static_image_path)) return;

    lv_obj_set_size(overlay_image, device.SCREEN.WIDTH, device.SCREEN.HEIGHT);
    lv_obj_set_pos(overlay_image, 0, 0);
    lv_obj_set_style_bg_opa(overlay_image, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_src(overlay_image, static_image_embed, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_tiled(overlay_image, true, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_opa(overlay_image, config.VISUAL.OVERLAYTRANSPARENCY, MU_OBJ_MAIN_DEFAULT);
    mu_img_no_shadow(overlay_image);
    lv_obj_move_foreground(overlay_image);
}

void load_overlay_image_sdl(void) {
    if (config.VISUAL.OVERLAYIMAGE == 0) {
        display_clear_theme_overlay();
        return;
    }

    const char *program = lv_obj_get_user_data(ui_screen);
    char image_path[MAX_BUFFER_SIZE];

    switch (config.VISUAL.OVERLAYIMAGE) {
        case 1:
            if (!load_image_specifics(mux_dim, program, "overlay", "png", image_path, sizeof(image_path)) &&
                !load_image_specifics("", program, "overlay", "png", image_path, sizeof(image_path))) {
                return;
            }
            break;
        default:
            snprintf(image_path, sizeof(image_path), "%s/%s%d.png",
                     STORAGE_OVERLAY, mux_dim, config.VISUAL.OVERLAYIMAGE);
            if (!file_exist(image_path)) {
                snprintf(image_path, sizeof(image_path), "%s/standard/%d.png",
                         STORAGE_OVERLAY, config.VISUAL.OVERLAYIMAGE);
            }
            break;
    }

    if (!file_exist(image_path)) return;

    SDL_Texture *tex = display_load_png_texture(image_path);
    if (tex) display_set_theme_overlay(tex, (uint8_t) config.VISUAL.OVERLAYTRANSPARENCY);
}

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (load_image_specifics(mux_dim, program, "kiosk", "png",
                             static_image_path, sizeof(static_image_path)) ||
        load_image_specifics("", program, "kiosk", "png",
                             static_image_path, sizeof(static_image_path))) {

        int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
        if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;

        lv_img_set_src(kiosk_image, static_image_embed);
        mu_img_no_shadow(kiosk_image);
        lv_obj_move_foreground(kiosk_image);
    }
}

int load_terminal_resource(const char *resource, const char *extension, char *buffer, size_t size) {
    const char *dims[] = {mux_dim, ""};

    for (size_t i = 0; i < 2; i++) {
        snprintf(buffer, size, "%s/%s%s/muterm.%s", theme_base, dims[i], resource, extension);
        if (file_exist(buffer)) return 1;
    }

    return 0;
}

static void image_anim_cb(void *var, int32_t img_idx) {
    lv_img_set_src(img_obj, img_paths[img_idx]);
}

void build_image_array(char *base_image_path) {
    char base_path[PATH_MAX];
    char path[PATH_MAX];
    size_t base_len = strlen(base_image_path) - 6;

    if (base_len >= PATH_MAX) {
        LOG_ERROR("image", "Base path exceeds maximum allowed length: %s", base_image_path);
        return;
    }

    memcpy(base_path, base_image_path, base_len);
    base_path[base_len] = '\0';

    int index = 0;
    int file_exists = 1;

    while (file_exists) {
        snprintf(path, sizeof(path), "%s.%d.png", base_path + 2, index);
        file_exists = file_exist(path);

        if (file_exists) {
            size_t needed_size = snprintf(NULL, 0, "%s.%d.png", base_path, index) + 1;
            char *path_embed = malloc(needed_size);
            if (!path_embed) {
                LOG_ERROR("image", "Failed to allocate memory for image: %s.%d.png", base_path, index);
                break;
            }

            snprintf(path_embed, needed_size, "%s.%d.png", base_path, index);

            char **img_temp = realloc(img_paths, (img_paths_count + 1) * sizeof(char *));
            if (!img_temp) {
                LOG_ERROR("image", "Failed to reallocate image path array");
                free(path_embed);
                break;
            }

            img_paths = img_temp;
            img_paths[img_paths_count] = path_embed;
            img_paths_count++;
        }

        index++;
    }
}

void load_image_random(lv_obj_t *ui_imgWall, char *base_image_path) {
    LOG_INFO(mux_module, "Load Image Random: %s", base_image_path);
    img_paths_count = 0;
    build_image_array(base_image_path);

    img_obj = ui_imgWall;

    if (img_paths_count > 0) {
        lv_img_set_src(ui_imgWall, img_paths[random() % img_paths_count]);
    } else {
        lv_img_set_src(ui_imgWall, &ui_img_blank);
    }
}

void load_image_animation(lv_obj_t *ui_imgWall, int animation_time, int repeat_count, char *base_image_path) {
    LOG_INFO(mux_module, "Load Image Animation: %s", base_image_path);
    img_paths_count = 0;
    build_image_array(base_image_path);

    img_obj = ui_imgWall;

    if (img_paths_count > 1 && config.VISUAL.BACKGROUNDANIMATION) {
        lv_obj_center(img_obj);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, img_obj);
        lv_anim_set_values(&animation, 0, img_paths_count - 1);
        lv_anim_set_exec_cb(&animation, (lv_anim_exec_xcb_t) image_anim_cb);
        lv_anim_set_time(&animation, animation_time * img_paths_count);
        lv_anim_set_repeat_count(&animation, repeat_count);

        lv_anim_start(&animation);
    } else {
        image_anim_cb(NULL, 0);
    }
}

void unload_image_animation(void) {
    if (anim_is_active()) {
        int was_background = !anim_is_foreground();
        anim_unload();
        if (was_background) {
            set_gradient_visible(1);
            lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_bg_opa(ui_screen, theme.SYSTEM.BACKGROUND_GRADIENT_DIRECTION == LV_GRAD_DIR_NONE
                                               ? theme.SYSTEM.BACKGROUND_ALPHA : 0, MU_OBJ_MAIN_DEFAULT);
        }
    }

    if (lv_obj_is_valid(wall_img)) lv_obj_del(wall_img);
    wall_img = NULL;

    if (lv_obj_is_valid(img_obj)) lv_anim_del(img_obj, NULL);
}

static void read_image_dims(const char *path, int *w, int *h) {
    *w = 0;
    *h = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;

    unsigned char buf[25];
    size_t n = fread(buf, 1, sizeof(buf), f);

    if (n >= 24 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        *w = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
        *h = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    } else if (n >= 2 && buf[0] == 0xFF && buf[1] == 0xD8) {
        unsigned char mk[2];
        fseek(f, 2, SEEK_SET);
        for (;;) {
            if (fread(mk, 1, 2, f) != 2 || mk[0] != 0xFF) break;

            if (mk[1] == 0xD9 || mk[1] == 0xDA) break;
            if (mk[1] == 0x01 || (mk[1] >= 0xD0 && mk[1] <= 0xD9)) continue;
            unsigned char lb[2];

            if (fread(lb, 1, 2, f) != 2) break;
            int seg = (lb[0] << 8) | lb[1];

            if (mk[1] >= 0xC0 && mk[1] <= 0xC3) {
                unsigned char sof[5];
                if (fread(sof, 1, 5, f) == 5) {
                    *h = (sof[1] << 8) | sof[2];
                    *w = (sof[3] << 8) | sof[4];
                }
                break;
            }

            if (seg < 2 || fseek(f, seg - 2, SEEK_CUR) != 0) break;
        }
    }

    fclose(f);
}

static void free_scaled_raster(lv_obj_t *ui_imgobj) {
    lv_img_dsc_t *old_dsc = lv_obj_get_user_data(ui_imgobj);
    if (old_dsc) {
        lv_img_cache_invalidate_src(old_dsc);
        lv_img_buf_free(old_dsc);
        lv_obj_set_user_data(ui_imgobj, NULL);
    }
}

static void scale_and_set_raster(lv_obj_t *ui_imgobj, const char *image_path,
                                 int tw, int th, int align,
                                 int pad_l, int pad_r, int pad_t, int pad_b) {
    char lvgl_path[MAX_BUFFER_SIZE];
    snprintf(lvgl_path, sizeof(lvgl_path), "M:%s", image_path);

    lv_img_decoder_dsc_t decode_dsc;
    memset(&decode_dsc, 0, sizeof(decode_dsc));
    if (lv_img_decoder_open(&decode_dsc, lvgl_path, lv_color_white(), 0) != LV_RES_OK) return;

    int sw = (int) decode_dsc.header.w;
    int sh = (int) decode_dsc.header.h;
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
            if (lv_img_decoder_read_line(&decode_dsc, 0, y, sw, src_buf + (size_t) y * sw * LV_IMG_PX_SIZE_ALPHA_BYTE) != LV_RES_OK) {
                lv_mem_free(src_buf);
                lv_img_decoder_close(&decode_dsc);
                return;
            }
        }
    }

    lv_img_dsc_t *scaled_dsc = lv_img_buf_alloc((lv_coord_t) tw, (lv_coord_t) th, LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (!scaled_dsc) {
        if (src_allocated) lv_mem_free(src_buf);
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    uint8_t *dst = (uint8_t *) scaled_dsc->data;
    for (int dy = 0; dy < th; dy++) {
        for (int dx = 0; dx < tw; dx++) {
            int sx0 = dx * sw / tw;
            int sy0 = dy * sh / th;

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

    lv_obj_set_user_data(ui_imgobj, scaled_dsc);
    lv_img_set_size_mode(ui_imgobj, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_img_set_zoom(ui_imgobj, LV_IMG_ZOOM_NONE);

    if (align >= 0) lv_obj_set_align(ui_imgobj, align);

    lv_obj_set_style_pad_left(ui_imgobj, pad_l, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_imgobj, pad_r, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_imgobj, pad_t, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_imgobj, pad_b, MU_OBJ_MAIN_DEFAULT);

    lv_img_set_src(ui_imgobj, scaled_dsc);
    lv_obj_move_foreground(ui_imgobj);
}

void update_image(lv_obj_t *ui_imgobj, struct ImageSettings image_settings) {
    free_scaled_raster(ui_imgobj);

    if (file_exist(image_settings.image_path)) {
        size_t plen = strlen(image_settings.image_path);
        int is_svg = plen > 4 && strcmp(image_settings.image_path + plen - 4, ".svg") == 0;

        if (!is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            int iw = 0, ih = 0;
            read_image_dims(image_settings.image_path, &iw, &ih);
            if (iw > 0 && ih > 0) {
                float wr = (float) image_settings.max_width / (float) iw;
                float hr = (float) image_settings.max_height / (float) ih;
                float zr = wr < hr ? wr : hr;
                int tw = (int) ((float) iw * zr);
                int th = (int) ((float) ih * zr);
                if (tw > 0 && th > 0) {
                    scale_and_set_raster(ui_imgobj, image_settings.image_path,
                                         tw, th, image_settings.align,
                                         image_settings.pad_left, image_settings.pad_right,
                                         image_settings.pad_top, image_settings.pad_bottom);
                    return;
                }
            }
        }

        char image_path[MAX_BUFFER_SIZE];
        if (is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            snprintf(image_path, sizeof(image_path), "M:%s?%dx%d", image_settings.image_path, image_settings.max_width, image_settings.max_height);
        } else {
            snprintf(image_path, sizeof(image_path), "M:%s", image_settings.image_path);
        }

        lv_img_set_size_mode(ui_imgobj, LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_zoom(ui_imgobj, LV_IMG_ZOOM_NONE);

        if (image_settings.align >= 0) lv_obj_set_align(ui_imgobj, image_settings.align);
        lv_obj_set_style_pad_left(ui_imgobj, image_settings.pad_left, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(ui_imgobj, image_settings.pad_right, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(ui_imgobj, image_settings.pad_top, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_imgobj, image_settings.pad_bottom, MU_OBJ_MAIN_DEFAULT);

        lv_img_set_src(ui_imgobj, image_path);
        lv_obj_move_foreground(ui_imgobj);
    } else {
        lv_img_set_src(ui_imgobj, &ui_img_blank);
    }
}
