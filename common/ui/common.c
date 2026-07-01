#include <stdlib.h>
#include <SDL2/SDL.h>
#include "common.h"
#include "nav.h"
#include "grid.h"
#include "glyph.h"
#include "image.h"
#include "../sysinfo.h"
#include "../anim.h"
#include "../input/list_nav.h"
#include "../strutil.h"
#include "../fileio.h"
#include "../inotify.h"
#include "../language.h"
#include "../config.h"
#include "../theme.h"
#include "../device.h"
#include "../battery.h"
#include "../display.h"
#include "../log.h"
#include "../video.h"

const lv_img_dsc_t ui_img_blank = {
    .header.always_zero = 0,
    .header.w = 1,
    .header.h = 1,
    .data_size = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .data = NULL
};

lv_obj_t *ui_screen_container;
lv_obj_t *ui_screen_temp;
lv_obj_t *ui_blank;
lv_obj_t *ui_black;
lv_obj_t *ui_screen;
lv_obj_t *ui_anim_tick;
lv_obj_t *ui_pnl_wall;
lv_obj_t *ui_img_wall;
lv_obj_t *ui_pnl_content;
lv_obj_t *ui_pnl_grid;
lv_obj_t *ui_pnl_box;
lv_obj_t *ui_img_box;
lv_obj_t *ui_pnl_header;
lv_obj_t *ui_lbl_datetime;
lv_obj_t *ui_lbl_title;
lv_obj_t *ui_con_glyphs;
lv_obj_t *ui_sta_bluetooth;
lv_obj_t *ui_sta_network;
lv_obj_t *ui_lbl_battery_percent;
lv_obj_t *ui_sta_capacity;
lv_obj_t *ui_pnl_footer;
lv_obj_t *ui_pnl_grid_current_item;
lv_obj_t *ui_lbl_grid_current_item;
lv_obj_t *ui_lbl_nav_lr_glyph;
lv_obj_t *ui_lbl_nav_lr;
lv_obj_t *ui_lbl_nav_a_glyph;
lv_obj_t *ui_lbl_nav_a;
lv_obj_t *ui_lbl_nav_b_glyph;
lv_obj_t *ui_lbl_nav_b;
lv_obj_t *ui_lbl_nav_c_glyph;
lv_obj_t *ui_lbl_nav_c;
lv_obj_t *ui_lbl_nav_x_glyph;
lv_obj_t *ui_lbl_nav_x;
lv_obj_t *ui_lbl_nav_y_glyph;
lv_obj_t *ui_lbl_nav_y;
lv_obj_t *ui_lbl_nav_z_glyph;
lv_obj_t *ui_lbl_nav_z;
lv_obj_t *ui_lbl_nav_menu_glyph;
lv_obj_t *ui_lbl_nav_menu;
lv_obj_t *ui_lbl_screen_message;
lv_obj_t *ui_pnl_message;
lv_obj_t *ui_lbl_message;
lv_obj_t *ui_pnl_help;
lv_obj_t *ui_pnl_help_message;
lv_obj_t *ui_lbl_help_header;
lv_obj_t *ui_pnl_help_content;
lv_obj_t *ui_lbl_help_content;
lv_obj_t *ui_pnl_help_extra;
lv_obj_t *ui_lbl_help_nav_ud_glyph;
lv_obj_t *ui_lbl_help_nav_ud;
lv_obj_t *ui_lbl_help_nav_b_glyph;
lv_obj_t *ui_lbl_help_nav_b;
lv_obj_t *ui_lbl_preview_header_glyph;
lv_obj_t *ui_lbl_preview_header;
lv_obj_t *ui_pnl_help_preview;
lv_obj_t *ui_lbl_help_preview_header;
lv_obj_t *ui_pnl_help_preview_image;
lv_obj_t *ui_img_help_preview_image;
lv_obj_t *ui_pnl_help_preview_info;
lv_obj_t *ui_lbl_help_preview_info_glyph;
lv_obj_t *ui_lbl_help_preview_info_message;
lv_obj_t *ui_lbl_help_preview_nav_b_glyph;
lv_obj_t *ui_lbl_help_preview_nav_b;
lv_obj_t *ui_pnl_progress_brightness;
lv_obj_t *ui_ico_progress_brightness;
lv_obj_t *ui_bar_progress_brightness;
lv_obj_t *ui_pnl_progress_volume;
lv_obj_t *ui_ico_progress_volume;
lv_obj_t *ui_bar_progress_volume;
lv_obj_t *ui_pnl_progress;
lv_obj_t *ui_bar_progress;
lv_obj_t *ui_lbl_progress;
lv_obj_t *ui_lbl_counter_explore;

lv_timer_t *toast_timer = NULL;
lv_timer_t *counter_timer = NULL;

int brightness_changed = 0;
int volume_changed = 0;
int last_brightness = -1;
int last_volume = -1;
int fade_in_done = 0;

static lv_obj_t *canvas;

static lv_style_t grid_cell_shadow_style;
static int grid_cell_shadow_style_ready = 0;

static lv_coord_t *grid_col_dsc = NULL;
static lv_coord_t *grid_row_dsc = NULL;

static int grid_col_alloc = 0;
static int grid_row_alloc = 0;

// Global buffer for the canvas
static lv_color_t *canvas_buffer;

// 4x4 Bayer Ordered Dithering Matrix (Normalized to 0-16)
static const uint8_t bayer_matrix[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

// Improved dithering function using Bayer matrix
static lv_color_t dither_color(const lv_color_t color, const int x, const int y) {
    // Get matrix value (normalized to range -4 to +4)
    const int bayer_value = bayer_matrix[((y & 3) << 2) | (x & 3)] - 7;

    int r = LV_COLOR_GET_R(color) + bayer_value;
    int g = LV_COLOR_GET_G(color) + bayer_value;
    int b = LV_COLOR_GET_B(color) + bayer_value;

    // Clamp values
    r = LV_CLAMP(0, r, 255);
    g = LV_CLAMP(0, g, 255);
    b = LV_CLAMP(0, b, 255);

    return lv_color_make(r, g, b);
}

void blur_gradient(lv_color_t *buf, const int width, const int height, const int blur_strength) {
    if (!buf || blur_strength <= 0 || width < 3 || height < 3) return;

    const size_t size = (size_t) width * height;
    lv_color_t *blur_grad = lv_mem_alloc(size * sizeof(lv_color_t));
    if (!blur_grad) return;

    for (int pass = 0; pass < blur_strength; pass++) {
        memcpy(blur_grad, buf, size * sizeof(lv_color_t));

        for (int y = 1; y < height - 1; y++) {
            const lv_color_t *row_up = blur_grad + (y - 1) * width;
            const lv_color_t *row = blur_grad + y * width;
            const lv_color_t *row_dn = blur_grad + (y + 1) * width;

            lv_color_t *dst = buf + y * width + 1;

            for (int x = 1; x < width - 1; x++, dst++) {
                const int r = LV_COLOR_GET_R(row_up[x - 1]) + LV_COLOR_GET_R(row_up[x]) + LV_COLOR_GET_R(row_up[x + 1])
                              + LV_COLOR_GET_R(row[x - 1]) + LV_COLOR_GET_R(row[x]) + LV_COLOR_GET_R(row[x + 1])
                              + LV_COLOR_GET_R(row_dn[x - 1]) + LV_COLOR_GET_R(row_dn[x])
                              + LV_COLOR_GET_R(row_dn[x + 1]);

                const int g = LV_COLOR_GET_G(row_up[x - 1]) + LV_COLOR_GET_G(row_up[x]) + LV_COLOR_GET_G(row_up[x + 1])
                              + LV_COLOR_GET_G(row[x - 1]) + LV_COLOR_GET_G(row[x]) + LV_COLOR_GET_G(row[x + 1])
                              + LV_COLOR_GET_G(row_dn[x - 1]) + LV_COLOR_GET_G(row_dn[x])
                              + LV_COLOR_GET_G(row_dn[x + 1]);

                const int b = LV_COLOR_GET_B(row_up[x - 1]) + LV_COLOR_GET_B(row_up[x]) + LV_COLOR_GET_B(row_up[x + 1])
                              + LV_COLOR_GET_B(row[x - 1]) + LV_COLOR_GET_B(row[x]) + LV_COLOR_GET_B(row[x + 1])
                              + LV_COLOR_GET_B(row_dn[x - 1]) + LV_COLOR_GET_B(row_dn[x])
                              + LV_COLOR_GET_B(row_dn[x + 1]);

                *dst = lv_color_make(r / 9, g / 9, b / 9);
            }
        }
    }

    lv_mem_free(blur_grad);
}

void generate_gradient_with_bayer_dither(
    lv_color_t *buf, const int width, const int height, const lv_color_t start_color, const lv_color_t end_color,
    const int apply_dither, const int vertical, const uint8_t main_stop, const uint8_t grad_stop
) {
    const int span = vertical ? height : width;

    // Convert gradient stop values (0-255) to pixel positions
    int start_pos = main_stop * span / 255;
    int end_pos = grad_stop * span / 255;

    // Prevent invalid cases where start is beyond end
    if (start_pos < 0) start_pos = 0;
    if (end_pos < 0) end_pos = 0;
    if (start_pos > span) start_pos = span;
    if (end_pos > span) end_pos = span;

    if (start_pos >= end_pos) {
        start_pos = 0;
        end_pos = span;
    }

    int ov_pos = end_pos - start_pos;
    if (ov_pos <= 0) ov_pos = 1;

    // Iterate over each pixel
    const uint8_t sr = LV_COLOR_GET_R(start_color);
    const uint8_t sg = LV_COLOR_GET_G(start_color);
    const uint8_t sb = LV_COLOR_GET_B(start_color);

    const uint8_t er = LV_COLOR_GET_R(end_color);
    const uint8_t eg = LV_COLOR_GET_G(end_color);
    const uint8_t eb = LV_COLOR_GET_B(end_color);

    const int dr = (int) er - sr;
    const int dg = (int) eg - sg;
    const int db = (int) eb - sb;

    lv_color_t *p = buf;

    for (int y = 0; y < height; y++) {
        // Determine pixel position in gradient
        const int pos = vertical ? y : 0;
        int grad = (pos - start_pos) * 255 / ov_pos;

        if (grad < 0) grad = 0;
        if (grad > 255) grad = 255;

        // Compute interpolated color values
        int r = sr + ((dr * grad) >> 8);
        int g = sg + ((dg * grad) >> 8);
        int b = sb + ((db * grad) >> 8);

        const int r_step = vertical ? 0 : (dr * 255 / ov_pos) >> 8;
        const int g_step = vertical ? 0 : (dg * 255 / ov_pos) >> 8;
        const int b_step = vertical ? 0 : (db * 255 / ov_pos) >> 8;

        for (int x = 0; x < width; x++, p++) {
            // Ensure valid LVGL color format
            lv_color_t final_color = lv_color_make(r, g, b);

            // Apply Bayer dithering if enabled
            if (apply_dither) final_color = dither_color(final_color, x, y);

            *p = final_color;
            if (!vertical) {
                r += r_step;
                g += g_step;
                b += b_step;
            }
        }
    }
}

void apply_gradient_to_ui_screen(
    lv_obj_t *ui_screen, const struct theme_config *theme, const struct mux_device *device
) {
    if (theme->system.background_gradient_direction == LV_GRAD_DIR_NONE) return;

    const size_t buf_size = (size_t) device->mux.width * (size_t) device->mux.height * sizeof(lv_color_t);

    static size_t canvas_buf_size = 0;

    // Allocate memory for the canvas buffer and reuse
    if (!canvas_buffer || canvas_buf_size != buf_size) {
        lv_mem_free(canvas_buffer);
        canvas_buffer = lv_mem_alloc(buf_size);

        if (!canvas_buffer) {
            LV_LOG_ERROR("Canvas buffer alloc failed");
            return;
        }

        canvas_buf_size = buf_size;
    }

    if (!canvas_buffer) {
        LV_LOG_ERROR("Failed to allocate memory for canvas buffer!");
        return;
    }

    // Create a canvas
    canvas = lv_canvas_create(ui_screen);
    lv_canvas_set_buffer(canvas, canvas_buffer, device->mux.width, device->mux.height, LV_IMG_CF_TRUE_COLOR);

    // Set size and position to cover the full screen
    lv_obj_set_size(canvas, device->mux.width, device->mux.height);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0); // Center on the screen
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    // Define gradient colors
    const lv_color_t start_color = lv_color_hex(theme->system.background);              // Black
    const lv_color_t end_color = lv_color_hex(theme->system.background_gradient_color); // Dark Gray

    // Generate the gradient with dithering
    generate_gradient_with_bayer_dither(
        canvas_buffer, device->mux.width, device->mux.height, start_color, end_color,
        theme->system.background_gradient_dither == 1, theme->system.background_gradient_direction == LV_GRAD_DIR_VER,
        theme->system.background_gradient_start, theme->system.background_gradient_stop
    );

    // Refresh the canvas
    if (theme->system.background_gradient_blur > 0)
        blur_gradient(canvas_buffer, device->mux.width, device->mux.height, theme->system.background_gradient_blur);

    lv_obj_invalidate(canvas);
}

void set_gradient_visible(const int visible) {
    if (!lv_obj_is_valid(canvas)) return;

    if (visible) {
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_common_get_gradient_buffer(void **buf, int *w, int *h) {
    if (!canvas_buffer || !lv_obj_is_valid(canvas)) {
        *buf = NULL;

        *w = 0;
        *h = 0;

        return;
    }

    *buf = canvas_buffer;

    *w = lv_obj_get_width(canvas);
    *h = lv_obj_get_height(canvas);
}

void fade_reset(void) {
    fade_in_done = 0;
    if (ui_black && !lv_obj_is_valid(ui_black)) ui_black = NULL;
}

static void gen_black(const int opacity) {
    if (!ui_screen_container || !lv_obj_is_valid(ui_screen_container)) return;

    ui_black = lv_obj_create(ui_screen_container);

    lv_obj_set_size(ui_black, device.mux.width, device.mux.height);
    lv_obj_set_style_radius(ui_black, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_black, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_black, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_black, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_black, opacity, MU_OBJ_MAIN_DEFAULT);

    lv_obj_center(ui_black);
    lv_obj_invalidate(ui_black);
}

static void fade_step(const int start, const int end, const int keep) {
    if (!ui_black || !lv_obj_is_valid(ui_black)) return;

    for (int i = 0; i <= FADE_STEP; i++) {
        if (!ui_black || !lv_obj_is_valid(ui_black)) return;

        const lv_opa_t opa = (lv_opa_t) (start + (end - start) * i / FADE_STEP);
        lv_obj_set_style_bg_opa(ui_black, opa, MU_OBJ_MAIN_DEFAULT);

        lv_obj_invalidate(ui_black);
        lv_refr_now(NULL);

        usleep(FADE_TIME * 1000 / FADE_STEP);
    }

    if (!keep && ui_black && lv_obj_is_valid(ui_black)) {
        lv_obj_del(ui_black);
        ui_black = NULL;
    }
}

void fade_in_screen(void) {
    if (!config.visual.blackfade || fade_in_done) return;
    if (!ui_black || !lv_obj_is_valid(ui_black)) {
        ui_black = NULL;
        return;
    }

    anim_process();

    if (anim_is_active()) {
        display_set_fade_alpha(255);
        lv_obj_del(ui_black);
        ui_black = NULL;

        lv_obj_invalidate(ui_screen);
        lv_refr_now(NULL);

        const uint32_t deadline = SDL_GetTicks() + 2000;
        while (anim_frames_ready() == 0 && SDL_GetTicks() < deadline) {
            lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
        }

        fade_in_done = 1;

        for (int i = 0; i <= FADE_STEP; i++) {
            const uint8_t alpha = (uint8_t) (255 - 255 * i / FADE_STEP);
            display_set_fade_alpha(alpha);
            lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
            usleep(FADE_TIME * 1000 / FADE_STEP);
        }

        display_set_fade_alpha(0);
    } else {
        fade_in_done = 1;
        lv_obj_move_foreground(ui_black);
        lv_refr_now(NULL);
        fade_step(LV_OPA_COVER, LV_OPA_TRANSP, 0);
    }
}

void fade_out_screen(void) {
    if (!config.visual.blackfade) {
        unload_image_animation();
        return;
    }

    if (!ui_screen_container || !lv_obj_is_valid(ui_screen_container)) {
        unload_image_animation();
        return;
    }

    if (ui_black && lv_obj_is_valid(ui_black)) {
        lv_obj_del(ui_black);
        ui_black = NULL;
    }

    if (anim_is_active()) {
        for (int i = 0; i <= FADE_STEP; i++) {
            const uint8_t alpha = (uint8_t) (255 * i / FADE_STEP);
            display_set_fade_alpha(alpha);
            lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
            usleep(FADE_TIME * 1000 / FADE_STEP);
        }

        unload_image_animation();
        gen_black(LV_OPA_COVER);

        if (ui_black && lv_obj_is_valid(ui_black)) lv_obj_move_foreground(ui_black);
    } else {
        unload_image_animation();
        gen_black(LV_OPA_TRANSP);

        if (!ui_black || !lv_obj_is_valid(ui_black)) return;

        lv_obj_move_foreground(ui_black);
        lv_refr_now(NULL);

        fade_step(LV_OPA_TRANSP, LV_OPA_COVER, 1);
    }
}

void init_ui_common_screen(
    const struct theme_config *theme, const struct mux_device *device, const struct mux_lang *lang, const char *title
) {
    ui_screen_container = lv_obj_create(NULL);
    lv_obj_set_style_border_width(ui_screen_container, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_screen_container, 0, MU_OBJ_MAIN_DEFAULT);

    if (ui_screen_temp == NULL) ui_screen_temp = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_container, LV_OBJ_FLAG_SCROLLABLE);

    apply_gradient_to_ui_screen(ui_screen_container, theme, device);
    /* GPU SDL renders the background via SDL compositing; keep LVGL root transparent */
    lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    ui_blank = lv_obj_create(ui_screen_container);
    lv_obj_set_width(ui_blank, device->mux.width);
    lv_obj_set_height(ui_blank, device->mux.height);

    lv_obj_set_align(ui_blank, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_blank, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_blank, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    ui_screen = lv_obj_create(ui_screen_container);
    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(ui_screen, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_screen, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(theme->system.background), MU_OBJ_MAIN_DEFAULT);
    /* Background is rendered by SDL compositor; keep LVGL screen transparent */
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_width(ui_screen, device->mux.width);
    lv_obj_set_height(ui_screen, device->mux.height);
    lv_obj_set_align(ui_screen, LV_ALIGN_CENTER);

    ui_anim_tick = lv_obj_create(ui_screen);
    lv_obj_set_size(ui_anim_tick, 1, 1);
    lv_obj_set_pos(ui_anim_tick, 0, 0);
    lv_obj_set_style_bg_opa(ui_anim_tick, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_anim_tick, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_anim_tick, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_anim_tick, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    ui_pnl_wall = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_wall, device->mux.width);
    lv_obj_set_height(ui_pnl_wall, device->mux.height);
    lv_obj_set_align(ui_pnl_wall, LV_ALIGN_CENTER);

    lv_obj_clear_flag(ui_pnl_wall, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnl_wall, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnl_wall, LV_DIR_VER);

    lv_obj_set_style_bg_color(ui_pnl_wall, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_wall, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_border_width(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnl_wall, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_wall, LV_OPA_TRANSP, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    ui_img_wall = lv_img_create(ui_pnl_wall);
    lv_img_set_src(ui_img_wall, &ui_img_blank);
    lv_obj_set_width(ui_img_wall, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_img_wall, LV_SIZE_CONTENT);

    lv_obj_set_align(ui_img_wall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_img_wall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_wall, LV_OBJ_FLAG_SCROLLABLE);

    mu_img_no_shadow(ui_img_wall);

    ui_pnl_grid = lv_obj_create(ui_screen);

    ui_pnl_grid_current_item = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(ui_pnl_grid_current_item);
    lv_obj_clear_flag(ui_pnl_grid_current_item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_pnl_grid_current_item, MU_OBJ_FLAG_HIDE_FLOAT);

    ui_lbl_grid_current_item = lv_label_create(ui_pnl_grid_current_item);
    lv_label_set_text(ui_lbl_grid_current_item, "");
    lv_obj_set_style_bg_opa(ui_lbl_grid_current_item, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lbl_grid_current_item, 0, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_content = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_content, device->mux.width);
    lv_obj_set_height(ui_pnl_content, theme->misc.content.height);

    lv_obj_set_x(ui_pnl_content, 0);
    lv_obj_set_y(ui_pnl_content, theme->header.height + 2 + theme->misc.content.padding_top);

    lv_obj_set_flex_flow(ui_pnl_content, LV_FLEX_FLOW_COLUMN);

    if (theme->misc.content.alignment == 1) {
        lv_obj_set_flex_align(ui_pnl_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(ui_pnl_content, theme->misc.content.padding_left * 2, MU_OBJ_MAIN_DEFAULT);
    } else if (theme->misc.content.alignment == 2) {
        lv_obj_set_flex_align(ui_pnl_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_right(ui_pnl_content, -theme->misc.content.padding_left, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_flex_align(ui_pnl_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_left(ui_pnl_content, theme->misc.content.padding_left, MU_OBJ_MAIN_DEFAULT);
    }

    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnl_content, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnl_content, LV_DIR_VER);

    lv_obj_set_style_bg_color(ui_pnl_content, lv_color_hex(0x0D0803), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_content, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_border_width(ui_pnl_content, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_top(ui_pnl_content, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_content, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_content, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_content, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnl_content, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_content, LV_OPA_TRANSP, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_content, lv_color_hex(0xFFFFFF), LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
    lv_obj_set_style_bg_opa(ui_pnl_content, LV_OPA_TRANSP, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);

    ui_pnl_box = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_box, device->mux.width);
    lv_obj_set_height(ui_pnl_box, device->mux.height - theme->header.height - theme->footer.height - 4);

    lv_obj_set_x(ui_pnl_box, 0);
    lv_obj_set_y(ui_pnl_box, theme->header.height + 2);

    lv_obj_set_align(ui_pnl_box, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnl_box, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnl_box, LV_DIR_VER);

    lv_obj_set_style_bg_color(ui_pnl_box, lv_color_hex(0x0D0803), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_box, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnl_box, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_box, LV_OPA_TRANSP, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    ui_img_box = lv_img_create(ui_pnl_box);
    lv_img_set_src(ui_img_box, &ui_img_blank);

    lv_obj_set_width(ui_img_box, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_img_box, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_img_box, LV_ALIGN_TOP_RIGHT);

    lv_obj_add_flag(ui_img_box, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(ui_img_box, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_img_box, theme->image_list.radius, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_img_opa(ui_img_box, theme->image_list.alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(ui_img_box, lv_color_hex(theme->image_list.recolour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_img_box, theme->image_list.recolour_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_img_box, theme->image_list.pad_left, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_img_box, theme->image_list.pad_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_img_box, theme->image_list.pad_top, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_img_box, theme->image_list.pad_bottom, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_header = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_header, device->mux.width);
    lv_obj_set_height(ui_pnl_header, theme->header.height);

    lv_obj_set_align(ui_pnl_header, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnl_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnl_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_header, lv_color_hex(theme->header.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, theme->header.background_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_border_width(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_datetime = lv_label_create(ui_pnl_header);
    lv_obj_set_width(ui_lbl_datetime, device->mux.width);
    lv_obj_set_height(ui_lbl_datetime, LV_SIZE_CONTENT);

    lv_label_set_long_mode(ui_lbl_datetime, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lbl_datetime, "");
    lv_obj_clear_flag(ui_lbl_datetime, LV_OBJ_FLAG_SCROLLABLE);
    if (!config.visual.clock) lv_obj_add_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_align(ui_lbl_datetime, theme->datetime.align, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_datetime, lv_color_hex(theme->datetime.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_datetime, theme->datetime.alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_border_width(ui_lbl_datetime, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_lbl_datetime, theme->datetime.padding_left, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_datetime, theme->datetime.padding_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_datetime, theme->font.header_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_datetime, theme->font.header_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_title = lv_label_create(ui_pnl_header);
    lv_obj_set_width(ui_lbl_title, device->mux.width);
    lv_obj_set_height(ui_lbl_title, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_title, LV_ALIGN_TOP_MID);

    lv_label_set_long_mode(ui_lbl_title, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lbl_title, title);
    if (!config.visual.header_title) lv_obj_add_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(ui_lbl_title, lv_color_hex(theme->header.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_title, theme->header.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_title, theme->header.text_align, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_lbl_title, theme->header.padding_left, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_title, theme->header.padding_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_title, theme->font.header_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_title, theme->font.header_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    ui_con_glyphs = lv_obj_create(ui_pnl_header);
    lv_obj_set_width(ui_con_glyphs, device->mux.width);
    lv_obj_set_height(ui_con_glyphs, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_con_glyphs, LV_ALIGN_TOP_MID);

    lv_obj_set_flex_flow(ui_con_glyphs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_con_glyphs, theme->status.align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_con_glyphs, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_pad_left(ui_con_glyphs, theme->status.padding_left, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_con_glyphs, theme->status.padding_right, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_con_glyphs, theme->font.header_icon_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_con_glyphs, theme->font.header_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    ui_sta_bluetooth = create_header_glyph(ui_con_glyphs, theme);
    update_bluetooth_status(ui_sta_bluetooth, theme);
    if (!config.visual.bluetooth) lv_obj_add_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);

    ui_sta_network = create_header_glyph(ui_con_glyphs, theme);
    update_network_status(ui_sta_network, theme, 0);
    if (!config.visual.network) lv_obj_add_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);

    ui_lbl_battery_percent = lv_label_create(ui_con_glyphs);
    lv_obj_set_width(ui_lbl_battery_percent, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_battery_percent, LV_SIZE_CONTENT);
    lv_label_set_text(ui_lbl_battery_percent, "");
    lv_obj_set_style_text_color(
        ui_lbl_battery_percent, lv_color_hex(theme->status.battery.normal), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_text_opa(ui_lbl_battery_percent, theme->status.battery.normal_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_battery_percent, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_battery_percent, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);

    ui_sta_capacity = create_header_glyph(ui_con_glyphs, theme);
    battery_update();
    update_battery_capacity(ui_sta_capacity, theme);
    update_battery_percent_label(ui_lbl_battery_percent, theme);
    if (config.visual.battery == 1) {
        lv_obj_add_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    }

    ui_pnl_footer = lv_obj_create(ui_screen);

    lv_obj_set_width(ui_pnl_footer, device->mux.width);
    lv_obj_set_height(ui_pnl_footer, theme->footer.height);
    lv_obj_set_align(ui_pnl_footer, LV_ALIGN_BOTTOM_MID);

    lv_obj_set_flex_flow(ui_pnl_footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnl_footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_footer, lv_color_hex(theme->footer.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme->footer.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_footer, 0, MU_OBJ_MAIN_DEFAULT);

    lv_flex_align_t e_align;
    switch (theme->nav.alignment) {
        case 1:
            e_align = LV_FLEX_ALIGN_CENTER;
            break;
        case 2:
            lv_obj_set_style_pad_right(ui_pnl_footer, 12, MU_OBJ_MAIN_DEFAULT);
            e_align = LV_FLEX_ALIGN_END;
            break;
        case 3:
            e_align = LV_FLEX_ALIGN_SPACE_AROUND;
            break;
        case 4:
            e_align = LV_FLEX_ALIGN_SPACE_BETWEEN;
            break;
        case 5:
            e_align = LV_FLEX_ALIGN_SPACE_EVENLY;
            break;
        default:
            lv_obj_set_style_pad_left(ui_pnl_footer, 12, MU_OBJ_MAIN_DEFAULT);
            e_align = LV_FLEX_ALIGN_START;
            break;
    }
    lv_obj_set_style_flex_main_place(ui_pnl_footer, e_align, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_nav_lr_glyph = create_footer_glyph(ui_pnl_footer, theme, "lr", theme->nav.lr, 1);
    ui_lbl_nav_lr = create_footer_text(ui_pnl_footer, theme, theme->nav.lr.text, theme->nav.lr.text_alpha, 1);

    ui_lbl_nav_a_glyph =
        create_footer_glyph(ui_pnl_footer, theme, config.settings.remap.layout ? "b" : "a", theme->nav.a, 1);
    ui_lbl_nav_a = create_footer_text(ui_pnl_footer, theme, theme->nav.a.text, theme->nav.a.text_alpha, 1);

    ui_lbl_nav_b_glyph =
        create_footer_glyph(ui_pnl_footer, theme, config.settings.remap.layout ? "a" : "b", theme->nav.b, 1);
    ui_lbl_nav_b = create_footer_text(ui_pnl_footer, theme, theme->nav.b.text, theme->nav.b.text_alpha, 1);

    ui_lbl_nav_c_glyph = create_footer_glyph(ui_pnl_footer, theme, "c", theme->nav.c, 1);
    ui_lbl_nav_c = create_footer_text(ui_pnl_footer, theme, theme->nav.c.text, theme->nav.c.text_alpha, 1);

    ui_lbl_nav_x_glyph =
        create_footer_glyph(ui_pnl_footer, theme, config.settings.remap.layout ? "y" : "x", theme->nav.x, 1);
    ui_lbl_nav_x = create_footer_text(ui_pnl_footer, theme, theme->nav.x.text, theme->nav.x.text_alpha, 1);

    ui_lbl_nav_y_glyph =
        create_footer_glyph(ui_pnl_footer, theme, config.settings.remap.layout ? "x" : "y", theme->nav.y, 1);
    ui_lbl_nav_y = create_footer_text(ui_pnl_footer, theme, theme->nav.y.text, theme->nav.y.text_alpha, 1);

    ui_lbl_nav_z_glyph = create_footer_glyph(ui_pnl_footer, theme, "z", theme->nav.z, 1);
    ui_lbl_nav_z = create_footer_text(ui_pnl_footer, theme, theme->nav.z.text, theme->nav.z.text_alpha, 1);

    ui_lbl_nav_menu_glyph = create_footer_glyph(ui_pnl_footer, theme, "menu", theme->nav.menu, 1);
    ui_lbl_nav_menu = create_footer_text(ui_pnl_footer, theme, theme->nav.menu.text, theme->nav.menu.text_alpha, 1);

    ui_lbl_screen_message = lv_label_create(ui_screen);
    lv_label_set_text(ui_lbl_screen_message, "");
    lv_obj_set_width(ui_lbl_screen_message, device->mux.width);
    lv_obj_set_height(ui_lbl_screen_message, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_screen_message, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_lbl_screen_message, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_scroll_dir(ui_lbl_screen_message, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lbl_screen_message, lv_color_hex(theme->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_screen_message, theme->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_screen_message, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lbl_screen_message, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_screen_message, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lbl_screen_message, lv_color_hex(0x100808), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lbl_screen_message, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lbl_screen_message, 200, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lbl_screen_message, LV_GRAD_DIR_HOR, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lbl_screen_message, lv_color_hex(0xA5B2B5), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lbl_screen_message, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lbl_screen_message, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_lbl_screen_message, LV_BORDER_SIDE_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_screen_message, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_screen_message, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_screen_message, theme->font.list_pad_top, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_screen_message, theme->font.list_pad_bottom, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_screen_message, lv_color_hex(0xF8E008), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(ui_lbl_screen_message, LV_OPA_COVER, MU_OBJ_MAIN_FOCUS);

    ui_pnl_message = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_message, device->mux.width - 25);
    lv_obj_set_height(ui_pnl_message, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_pnl_message, 0);
    lv_obj_set_y(ui_pnl_message, -theme->footer.height - 5);
    lv_obj_set_align(ui_pnl_message, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(ui_pnl_message, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnl_message, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_message, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_message, theme->message.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_message, lv_color_hex(theme->message.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_message, theme->message.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_pnl_message, lv_color_hex(theme->message.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnl_message, theme->message.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_message, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_message, LV_BORDER_SIDE_FULL, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_message = lv_label_create(ui_pnl_message);
    lv_obj_set_width(ui_lbl_message, device->mux.width - 50);
    lv_obj_set_height(ui_lbl_message, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_message, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbl_message, "");
    lv_label_set_recolor(ui_lbl_message, 1);
    lv_obj_set_style_text_color(ui_lbl_message, lv_color_hex(theme->message.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_message, theme->message.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_message, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_message, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_message, theme->font.message_pad_top, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_message, theme->font.message_pad_bottom, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_help = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_help, device->mux.width);
    lv_obj_set_height(ui_pnl_help, device->mux.height);
    lv_obj_set_align(ui_pnl_help, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_help, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_help, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_help, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help, 155, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_pnl_help, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnl_help, 155, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_help, 1, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_help_message = lv_obj_create(ui_pnl_help);
    lv_obj_set_width(ui_pnl_help_message, (int) round(device->mux.width * .9));
    lv_obj_set_height(ui_pnl_help_message, (int) round(device->mux.height * .9));
    lv_obj_set_align(ui_pnl_help_message, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_help_message, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_help_message, theme->help.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help_message, lv_color_hex(theme->help.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help_message, theme->help.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_pnl_help_message, lv_color_hex(theme->help.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnl_help_message, theme->help.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_help_message, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_help_message, LV_BORDER_SIDE_FULL, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_layout(ui_pnl_help_message, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui_pnl_help_message, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui_pnl_help_message, (int32_t) round(device->mux.height * .03), 0);

    ui_lbl_help_header = lv_label_create(ui_pnl_help_message);
    lv_obj_set_width(ui_lbl_help_header, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_lbl_help_header, LV_SIZE_CONTENT);
    lv_obj_align(ui_lbl_help_header, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_label_set_long_mode(ui_lbl_help_header, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lbl_help_header, "");
    lv_obj_set_style_text_color(ui_lbl_help_header, lv_color_hex(theme->help.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_help_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_help_content = lv_obj_create(ui_pnl_help_message);
    lv_obj_set_align(ui_pnl_help_content, LV_ALIGN_CENTER);
    lv_obj_set_width(ui_pnl_help_content, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_flex_grow(ui_pnl_help_content, 1);
    lv_obj_clear_flag(ui_pnl_help_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_pnl_help_content, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui_pnl_help_content, LV_SCROLL_SNAP_NONE);

    ui_lbl_help_content = lv_label_create(ui_pnl_help_content);
    lv_obj_set_width(ui_lbl_help_content, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_lbl_help_content, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_help_content, LV_ALIGN_TOP_LEFT);
    lv_label_set_long_mode(ui_lbl_help_content, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ui_lbl_help_content, "");
    lv_label_set_recolor(ui_lbl_help_content, 1);
    lv_obj_set_style_text_color(ui_lbl_help_content, lv_color_hex(theme->help.content), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_help_content, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lbl_help_content, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lbl_help_content, 5, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_help_extra = lv_obj_create(ui_pnl_help_message);
    lv_obj_set_width(ui_pnl_help_extra, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_pnl_help_extra, theme->footer.height);
    lv_obj_align(ui_pnl_help_extra, LV_ALIGN_BOTTOM_LEFT, 15, 0);
    lv_obj_set_flex_flow(ui_pnl_help_extra, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnl_help_extra, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(
        ui_pnl_help_extra, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE
                               | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE
                               | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN
    );
    lv_obj_set_style_radius(ui_pnl_help_extra, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help_extra, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help_extra, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_help_extra, LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_help_nav_ud_glyph = create_footer_glyph(ui_pnl_help_extra, theme, "ud", theme->nav.ud, 0);
    ui_lbl_help_nav_ud = create_footer_text(ui_pnl_help_extra, theme, theme->nav.ud.text, theme->nav.ud.text_alpha, 0);
    lv_label_set_text(ui_lbl_help_nav_ud, lang->generic.scroll);

    ui_lbl_preview_header_glyph =
        create_footer_glyph(ui_pnl_help_extra, theme, config.settings.remap.layout ? "b" : "a", theme->nav.a, 0);

    ui_lbl_preview_header = create_footer_text(ui_pnl_help_extra, theme, theme->nav.a.text, theme->nav.a.text_alpha, 0);
    lv_label_set_text(ui_lbl_preview_header, lang->generic.switch_image);

    ui_lbl_help_nav_b_glyph =
        create_footer_glyph(ui_pnl_help_extra, theme, config.settings.remap.layout ? "a" : "b", theme->nav.b, 0);

    ui_lbl_help_nav_b = create_footer_text(ui_pnl_help_extra, theme, theme->nav.b.text, theme->nav.b.text_alpha, 0);
    lv_label_set_text(ui_lbl_help_nav_b, lang->generic.close);

    ui_pnl_help_preview = lv_obj_create(ui_pnl_help);
    lv_obj_set_width(ui_pnl_help_preview, (int) round(device->mux.width * .9));
    lv_obj_set_height(ui_pnl_help_preview, (int) round(device->mux.height * .9));
    lv_obj_set_align(ui_pnl_help_preview, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_help_preview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_help_preview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_help_preview, theme->help.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help_preview, lv_color_hex(theme->help.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help_preview, theme->help.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_pnl_help_preview, lv_color_hex(theme->help.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnl_help_preview, theme->help.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_help_preview, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_help_preview, LV_BORDER_SIDE_FULL, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_help_preview_header = lv_label_create(ui_pnl_help_preview);
    lv_obj_set_width(ui_lbl_help_preview_header, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_lbl_help_preview_header, LV_SIZE_CONTENT);
    lv_obj_align(ui_lbl_help_preview_header, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_label_set_long_mode(ui_lbl_help_preview_header, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lbl_help_preview_header, "");
    lv_obj_set_style_text_color(ui_lbl_help_preview_header, lv_color_hex(theme->help.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_help_preview_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_help_preview_image = lv_obj_create(ui_pnl_help_preview);
    lv_obj_set_width(ui_pnl_help_preview_image, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_pnl_help_preview_image, (int) round(device->mux.height * .9) - 120);
    lv_obj_set_align(ui_pnl_help_preview_image, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_help_preview_image, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_help_preview_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help_preview_image, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help_preview_image, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_help_preview_image, 0, MU_OBJ_MAIN_DEFAULT);

    ui_img_help_preview_image = lv_img_create(ui_pnl_help_preview_image);
    lv_img_set_src(ui_img_help_preview_image, &ui_img_blank);
    lv_obj_set_width(ui_img_help_preview_image, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_img_help_preview_image, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_img_help_preview_image, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_img_help_preview_image, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_help_preview_image, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_img_help_preview_image, theme->image_preview.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_img_help_preview_image, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_img_help_preview_image, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_clip_corner(ui_img_help_preview_image, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_opa(ui_img_help_preview_image, theme->image_preview.alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(
        ui_img_help_preview_image, lv_color_hex(theme->image_preview.recolour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_img_recolor_opa(
        ui_img_help_preview_image, theme->image_preview.recolour_alpha, MU_OBJ_MAIN_DEFAULT
    );

    ui_pnl_help_preview_info = lv_obj_create(ui_pnl_help_preview);
    lv_obj_set_width(ui_pnl_help_preview_info, (int) round(device->mux.width * .9) - 60);
    lv_obj_set_height(ui_pnl_help_preview_info, theme->footer.height);
    lv_obj_align(ui_pnl_help_preview_info, LV_ALIGN_BOTTOM_LEFT, 15, 0);
    lv_obj_set_flex_flow(ui_pnl_help_preview_info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnl_help_preview_info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_help_preview_info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_help_preview_info, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_help_preview_info, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help_preview_info, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_help_preview_info, LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_help_preview_info_glyph =
        create_footer_glyph(ui_pnl_help_preview_info, theme, config.settings.remap.layout ? "b" : "a", theme->nav.a, 0);

    ui_lbl_help_preview_info_message =
        create_footer_text(ui_pnl_help_preview_info, theme, theme->nav.a.text, theme->nav.a.text_alpha, 0);
    lv_label_set_text(ui_lbl_help_preview_info_message, lang->generic.switch_info);

    ui_lbl_help_preview_nav_b_glyph =
        create_footer_glyph(ui_pnl_help_preview_info, theme, config.settings.remap.layout ? "a" : "b", theme->nav.b, 0);

    ui_lbl_help_preview_nav_b =
        create_footer_text(ui_pnl_help_preview_info, theme, theme->nav.b.text, theme->nav.b.text_alpha, 0);
    lv_label_set_text(ui_lbl_help_preview_nav_b, lang->generic.close);

    ui_pnl_progress_brightness = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_progress_brightness, theme->bar.panel_width);
    lv_obj_set_height(ui_pnl_progress_brightness, theme->bar.panel_height);
    lv_obj_set_x(ui_pnl_progress_brightness, 0);
    lv_obj_set_y(ui_pnl_progress_brightness, theme->bar.y_pos);
    lv_obj_set_align(ui_pnl_progress_brightness, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnl_progress_brightness, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        ui_pnl_progress_brightness, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER
    );
    lv_obj_add_flag(ui_pnl_progress_brightness, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_progress_brightness, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_progress_brightness, theme->bar.panel_border_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_pnl_progress_brightness, lv_color_hex(theme->bar.panel_background), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_opa(ui_pnl_progress_brightness, theme->bar.panel_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(
        ui_pnl_progress_brightness, lv_color_hex(theme->bar.panel_border), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_border_opa(ui_pnl_progress_brightness, theme->bar.panel_border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_progress_brightness, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_progress_brightness, LV_BORDER_SIDE_FULL, MU_OBJ_MAIN_DEFAULT);

    ui_ico_progress_brightness = lv_img_create(ui_pnl_progress_brightness);
    lv_obj_set_align(ui_ico_progress_brightness, LV_ALIGN_CENTER);
    lv_obj_set_style_img_recolor(ui_ico_progress_brightness, lv_color_hex(theme->bar.icon), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_ico_progress_brightness, theme->bar.icon_alpha, MU_OBJ_MAIN_DEFAULT);
    update_glyph(ui_ico_progress_brightness, "bar", "bright_0");

    current_brightness = config.settings.general.brightness;
    ui_bar_progress_brightness = lv_bar_create(ui_pnl_progress_brightness);
    lv_bar_set_value(
        ui_bar_progress_brightness, brightness_to_percent(config.settings.general.brightness), LV_ANIM_OFF
    );
    lv_bar_set_start_value(ui_bar_progress_brightness, 0, LV_ANIM_OFF);
    lv_bar_set_range(ui_bar_progress_brightness, 0, 100);
    lv_obj_set_width(ui_bar_progress_brightness, theme->bar.progress_width);
    lv_obj_set_height(ui_bar_progress_brightness, theme->bar.progress_height);
    lv_obj_set_align(ui_bar_progress_brightness, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_bar_progress_brightness, theme->bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_bar_progress_brightness, lv_color_hex(theme->bar.progress_main_background), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_opa(ui_bar_progress_brightness, theme->bar.progress_main_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_progress_brightness, theme->bar.progress_radius, MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_bar_progress_brightness, lv_color_hex(theme->bar.progress_active_background), MU_OBJ_INDI_DEFAULT
    );
    lv_obj_set_style_bg_opa(
        ui_bar_progress_brightness, theme->bar.progress_active_background_alpha, MU_OBJ_INDI_DEFAULT
    );

    ui_pnl_progress_volume = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_progress_volume, theme->bar.panel_width);
    lv_obj_set_height(ui_pnl_progress_volume, theme->bar.panel_height);
    lv_obj_set_x(ui_pnl_progress_volume, 0);
    lv_obj_set_y(ui_pnl_progress_volume, theme->bar.y_pos);
    lv_obj_set_align(ui_pnl_progress_volume, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnl_progress_volume, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        ui_pnl_progress_volume, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER
    );
    lv_obj_add_flag(ui_pnl_progress_volume, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_progress_volume, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_progress_volume, theme->bar.panel_border_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_progress_volume, lv_color_hex(theme->bar.panel_background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_progress_volume, theme->bar.panel_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_pnl_progress_volume, lv_color_hex(theme->bar.panel_border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnl_progress_volume, theme->bar.panel_border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_progress_volume, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_progress_volume, LV_BORDER_SIDE_FULL, MU_OBJ_MAIN_DEFAULT);

    ui_ico_progress_volume = lv_img_create(ui_pnl_progress_volume);
    lv_obj_set_align(ui_ico_progress_volume, LV_ALIGN_CENTER);
    lv_obj_set_style_img_recolor(ui_ico_progress_volume, lv_color_hex(theme->bar.icon), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_ico_progress_volume, theme->bar.icon_alpha, MU_OBJ_MAIN_DEFAULT);
    update_glyph(ui_ico_progress_volume, "bar", "volume_0");

    current_volume = config.settings.general.volume;
    ui_bar_progress_volume = lv_bar_create(ui_pnl_progress_volume);
    lv_bar_set_value(ui_bar_progress_volume, volume_to_percent(config.settings.general.volume), LV_ANIM_OFF);
    lv_bar_set_start_value(ui_bar_progress_volume, 0, LV_ANIM_OFF);
    lv_bar_set_range(ui_bar_progress_volume, 0, 100);
    lv_obj_set_width(ui_bar_progress_volume, theme->bar.progress_width);
    lv_obj_set_height(ui_bar_progress_volume, theme->bar.progress_height);
    lv_obj_set_align(ui_bar_progress_volume, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_bar_progress_volume, theme->bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_bar_progress_volume, lv_color_hex(theme->bar.progress_main_background), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_opa(ui_bar_progress_volume, theme->bar.progress_main_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_progress_volume, theme->bar.progress_radius, MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_bar_progress_volume, lv_color_hex(theme->bar.progress_active_background), MU_OBJ_INDI_DEFAULT
    );
    lv_obj_set_style_bg_opa(ui_bar_progress_volume, theme->bar.progress_active_background_alpha, MU_OBJ_INDI_DEFAULT);

    ui_pnl_progress = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_progress, device->mux.width);
    lv_obj_set_height(ui_pnl_progress, device->mux.height);
    lv_obj_set_flex_flow(ui_pnl_progress, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_pnl_progress, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui_pnl_progress, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_pnl_progress, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_progress, 230, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN);

    ui_bar_progress = lv_bar_create(ui_pnl_progress);
    lv_obj_set_size(ui_bar_progress, 400, 40);
    lv_obj_set_style_radius(ui_bar_progress, theme->bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_bar_progress, lv_color_hex(theme->bar.progress_main_background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_progress, theme->bar.progress_main_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_progress, 0, MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_color(
        ui_bar_progress, lv_color_hex(theme->bar.progress_active_background), MU_OBJ_INDI_DEFAULT
    );
    lv_obj_set_style_bg_opa(ui_bar_progress, theme->bar.progress_active_background_alpha, MU_OBJ_INDI_DEFAULT);
    lv_bar_set_value(ui_bar_progress, 0, LV_ANIM_OFF);

    ui_lbl_progress = lv_label_create(ui_pnl_progress);
    lv_label_set_text(ui_lbl_progress, "");
    lv_obj_set_width(ui_lbl_progress, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_progress, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(ui_lbl_progress, lv_color_hex(theme->message.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_progress, theme->message.text_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_disp_load_scr(ui_screen_container);

    if (config.visual.blackfade && !fade_in_done) gen_black(LV_OPA_COVER);
}

int ui_common_check(const int mode) {
    if ((config.boot.device_mode && mode == 0) || config.boot.factory_reset) return 0;

    progress_onscreen = 1;
    return 1;
}

void init_ui_item_counter(const struct theme_config *theme) {
    ui_lbl_counter_explore = lv_label_create(ui_screen);
    lv_label_set_text(ui_lbl_counter_explore, "");

    lv_obj_set_width(ui_lbl_counter_explore, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_counter_explore, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_counter_explore, LV_ALIGN_CENTER);

    lv_label_set_text(ui_lbl_counter_explore, "");
    lv_obj_add_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_counter_explore, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_text_color(ui_lbl_counter_explore, lv_color_hex(theme->counter.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_counter_explore, theme->counter.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_counter_explore, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(ui_lbl_counter_explore, lv_color_hex(theme->counter.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_counter_explore, theme->counter.background_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_border_color(
        ui_lbl_counter_explore, lv_color_hex(theme->counter.border_colour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_border_opa(ui_lbl_counter_explore, theme->counter.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lbl_counter_explore, theme->counter.border_width, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_lbl_counter_explore, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_counter_explore, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_counter_explore, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_counter_explore, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_radius(ui_lbl_counter_explore, theme->counter.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_y(ui_lbl_counter_explore, theme->counter.padding_top);

    switch (theme->counter.alignment) {
        case 1:
            lv_obj_set_align(ui_lbl_counter_explore, LV_ALIGN_TOP_MID);
            break;
        case 2:
            lv_obj_set_align(ui_lbl_counter_explore, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_x(ui_lbl_counter_explore, -theme->counter.padding_side);
            break;
        default:
            lv_obj_set_align(ui_lbl_counter_explore, LV_ALIGN_TOP_LEFT);
            lv_obj_set_x(ui_lbl_counter_explore, theme->counter.padding_side);
            break;
    }
}

static int blank_check(void) {
    if (file_exist(MUX_BLANK)) {
        is_blank = 1;

        lv_obj_set_style_bg_opa(ui_blank, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_foreground(ui_blank);
    } else {
        is_blank = 0;

        lv_obj_set_style_bg_opa(ui_blank, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_background(ui_blank);
    }

    return is_blank;
}

#define PROGRESS_FADE_TIME 200

static void progress_fade_exec_cb(void *var, const int32_t value) {
    lv_obj_set_style_opa(var, (lv_opa_t) value, MU_OBJ_MAIN_DEFAULT);
}

static void progress_fade_ready_cb(const lv_anim_t *a) {
    lv_obj_t *obj = a->var;
    lv_obj_add_flag(obj, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
}

void ui_common_progress_fade_out(lv_obj_t *obj) {
    if (!obj || !lv_obj_is_valid(obj)) return;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_anim_get(obj, progress_fade_exec_cb)) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, PROGRESS_FADE_TIME);
    lv_anim_set_exec_cb(&a, progress_fade_exec_cb);
    lv_anim_set_ready_cb(&a, progress_fade_ready_cb);
    lv_anim_start(&a);
}

static void progress_show(lv_obj_t *show, lv_obj_t *hide) {
    lv_obj_add_flag(hide, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(show, progress_fade_exec_cb);
    lv_obj_set_style_opa(show, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(show, LV_OBJ_FLAG_HIDDEN);
}

static void adjust_brightness(const int direction) {
    if (!ui_common_check(0) || !progress_onscreen) return;

    const int inc_bright = config.settings.advanced.inc_bright;

    if (direction > 0) {
        current_brightness += current_brightness <= inc_bright - 1 ? 1 : inc_bright;
        if (current_brightness > device.screen.bright) current_brightness = device.screen.bright;
    } else {
        current_brightness -= current_brightness <= inc_bright ? 1 : inc_bright;
        if (current_brightness < 0) current_brightness = 0;
    }

    const int percent = brightness_to_percent(current_brightness);

    const char *glyph = "bright_0";
    if (percent > 70) {
        glyph = "bright_3";
    } else if (percent > 35) {
        glyph = "bright_2";
    } else if (percent > 0) {
        glyph = "bright_1";
    }

    update_glyph(ui_ico_progress_brightness, "bar", glyph);

    progress_show(ui_pnl_progress_brightness, ui_pnl_progress_volume);

    brightness_changed = 1;
}

static void adjust_volume(const int direction) {
    if (!ui_common_check(0) || !progress_onscreen) return;

    const int inc_volume = config.settings.advanced.inc_volume;

    if (direction > 0) {
        current_volume += inc_volume;
        if (current_volume > device.audio.max) current_volume = device.audio.max;
    } else {
        current_volume -= inc_volume;
        if (current_volume < 0) current_volume = 0;
    }

    const int percent = volume_to_percent(current_volume);

    const char *glyph = "volume_0";
    if (percent > 70) {
        glyph = "volume_3";
    } else if (percent > 35) {
        glyph = "volume_2";
    } else if (percent > 0) {
        glyph = "volume_1";
    }

    update_glyph(ui_ico_progress_volume, "bar", glyph);

    progress_show(ui_pnl_progress_volume, ui_pnl_progress_brightness);

    volume_changed = 1;
}

void ui_common_handle_bright_up(void) {
    adjust_brightness(+1);
}

void ui_common_handle_bright_down(void) {
    adjust_brightness(-1);
}

void ui_common_handle_volume_up(void) {
    adjust_volume(+1);
}

void ui_common_handle_volume_down(void) {
    adjust_volume(-1);
}

void ui_common_handle_idle(void) {
    if (strcmp(mux_module, "mucredits") == 0 || strcmp(mux_module, "muxcharge") == 0) {
        lv_task_handler();
        return;
    }

    inotify_check(ino_proc);
    int need_update = 0;

    if (brightness_changed || last_brightness != current_brightness) {
        lv_bar_set_value(ui_bar_progress_brightness, brightness_to_percent(current_brightness), LV_ANIM_OFF);

        last_brightness = current_brightness;
        brightness_changed = 0;

        char buffer[MAX_BUFFER_SIZE];
        CFG_INT_FIELD(config.settings.general.brightness, CONF_CONFIG_PATH "settings/general/brightness", 90);

        blank_check();
        need_update = 1;
    }

    if (volume_changed || last_volume != current_volume) {
        lv_bar_set_value(ui_bar_progress_volume, volume_to_percent(current_volume), LV_ANIM_OFF);

        last_volume = current_volume;
        volume_changed = 0;

        char buffer[MAX_BUFFER_SIZE];
        CFG_INT_FIELD(config.settings.general.volume, CONF_CONFIG_PATH "settings/general/volume", 75);

        need_update = 1;
    }

    if (hdmi_refresh_exists) {
        remove(HDMI_REFRESH);
        hdmi_refresh_exists = 0;

        lv_obj_invalidate(ui_pnl_header);
        lv_obj_invalidate(ui_pnl_content);
        lv_obj_invalidate(ui_pnl_footer);
        lv_obj_invalidate(ui_screen);

        refresh_screen(ui_screen, 3);
        need_update = 1;
    }

    if (!blank_exists && lv_obj_get_style_bg_opa(ui_blank, MU_OBJ_MAIN_DEFAULT) > LV_OPA_TRANSP) blank_check();

    anim_process();

    if (need_update) {
        lv_task_handler();
    } else {
        lv_timer_handler();
    }
}

lv_obj_t *create_header_glyph(lv_obj_t *parent, const struct theme_config *theme) {
    lv_obj_t *ui_glyph = lv_img_create(parent);

    lv_obj_set_width(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_glyph, LV_SIZE_CONTENT);

    lv_obj_set_align(ui_glyph, LV_ALIGN_CENTER);

    lv_obj_set_style_pad_left(ui_glyph, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_glyph, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_glyph, theme->font.header_icon_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_glyph, theme->font.header_icon_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    return ui_glyph;
}

lv_obj_t *create_footer_glyph(
    lv_obj_t *parent, const struct theme_config *theme, const char *glyph_name,
    const struct footer_glyph nav_footer_glyph, const int16_t add_hide_flag
) {
    lv_obj_t *ui_glyph = lv_img_create(parent);
    char footer_image_path[MAX_BUFFER_SIZE] = "";
    char footer_image_embed[MAX_BUFFER_SIZE] = "";

    if (generate_image_embed(
            mux_dim, "footer", glyph_name, footer_image_path, sizeof(footer_image_path), footer_image_embed,
            sizeof(footer_image_embed)
        )
        && nav_footer_glyph.glyph_alpha > 0) {
        const int footer_target = resolve_glyph_size(
            config.settings.themeopt.glyph_size_footer, theme->glyph.footer, theme->mux.item.height * 3 / 4
        );
        const int footer_px = glyph_explicit_px(config.settings.themeopt.glyph_size_footer, theme->glyph.footer);
        append_glyph_size_hint(footer_image_embed, sizeof(footer_image_embed), footer_target);
        set_list_glyph_image(ui_glyph, footer_image_embed);
        apply_glyph_scale(ui_glyph, footer_image_embed, footer_px, footer_px);
    }

    lv_obj_set_width(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_glyph, LV_ALIGN_CENTER);

    lv_obj_set_style_img_opa(ui_glyph, nav_footer_glyph.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(ui_glyph, lv_color_hex(nav_footer_glyph.glyph), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_glyph, nav_footer_glyph.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_glyph, theme->nav.spacing, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_glyph, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_glyph, theme->font.footer_icon_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_glyph, theme->font.footer_icon_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    if (nav_footer_glyph.glyph_alpha == 0) lv_obj_set_width(ui_glyph, 0);
    if (add_hide_flag) lv_obj_add_flag(ui_glyph, LV_OBJ_FLAG_HIDDEN);

    return ui_glyph;
}

lv_obj_t *create_footer_text(
    lv_obj_t *parent, const struct theme_config *theme, const uint32_t text_color, const int16_t text_alpha,
    const int16_t add_hide_flag
) {
    lv_obj_t *ui_lbl_nav_text = lv_label_create(parent);

    lv_obj_set_width(ui_lbl_nav_text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_nav_text, LV_SIZE_CONTENT);

    lv_obj_set_align(ui_lbl_nav_text, LV_ALIGN_CENTER);

    lv_label_set_text(ui_lbl_nav_text, "");
    lv_label_set_recolor(ui_lbl_nav_text, 1);

    lv_obj_set_style_text_color(ui_lbl_nav_text, lv_color_hex(text_color), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_nav_text, text_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_left(ui_lbl_nav_text, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_nav_text, theme->nav.spacing, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_nav_text, theme->font.footer_pad_top * 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_nav_text, theme->font.footer_pad_bottom * 2, MU_OBJ_MAIN_DEFAULT);

    if (text_alpha == 0) lv_obj_set_width(ui_lbl_nav_text, 0);
    if (add_hide_flag) lv_obj_add_flag(ui_lbl_nav_text, LV_OBJ_FLAG_HIDDEN);

    return ui_lbl_nav_text;
}

int load_glyph_icon(
    const char *mux_dim, const char *glyph_folder, const char *glyph_name, char *image_path, size_t image_size
) {
    if (!glyph_folder || !glyph_name || !image_path || image_size == 0) return 0;

#define TRY_GLYPH(fmt, ...)                                                                                            \
    do {                                                                                                               \
        snprintf(image_path, image_size, fmt, ##__VA_ARGS__);                                                          \
        LOG_DEBUG(mux_module, "Glyph path check: %s", image_path);                                                     \
        if (file_exist(image_path)) {                                                                                  \
            LOG_DEBUG(mux_module, "Glyph found at: %s", image_path);                                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

    TRY_GLYPH("%s/%sglyph/%s/%s.svg", theme_base, mux_dim, glyph_folder, glyph_name);
    TRY_GLYPH("%s/glyph/%s/%s.svg", theme_base, glyph_folder, glyph_name);
    TRY_GLYPH("%s/%sglyph/%s/%s.png", theme_base, mux_dim, glyph_folder, glyph_name);
    TRY_GLYPH("%s/glyph/%s/%s.png", theme_base, glyph_folder, glyph_name);

    TRY_GLYPH("%s/%sglyph/%s/%s.svg", INTERNAL_THEME, mux_dim, glyph_folder, glyph_name);
    TRY_GLYPH("%s/glyph/%s/%s.svg", INTERNAL_THEME, glyph_folder, glyph_name);
    TRY_GLYPH("%s/%sglyph/%s/%s.png", INTERNAL_THEME, mux_dim, glyph_folder, glyph_name);
    TRY_GLYPH("%s/glyph/%s/%s.png", INTERNAL_THEME, glyph_folder, glyph_name);

#undef TRY_GLYPH

    LOG_DEBUG(mux_module, "Glyph not found: %s/%s", glyph_folder, glyph_name);
    image_path[0] = '\0';
    return 0;
}

int generate_image_embed(
    const char *dimension, const char *glyph_folder, const char *glyph_name, char *image_path, const size_t path_size,
    char *image_embed, const size_t embed_size
) {
    if (!load_glyph_icon(dimension, glyph_folder, glyph_name, image_path, path_size)) return 0;

    const int written = snprintf(image_embed, embed_size, "M:%s", image_path);
    if (written < 0 || (size_t) written >= embed_size) {
        image_embed[0] = '\0';
        return 0;
    }

    return 1;
}

void update_glyph(lv_obj_t *ui_img, const char *glyph_folder, const char *glyph_name) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];

    if (generate_image_embed(
            mux_dim, glyph_folder, glyph_name, image_path, sizeof(image_path), image_embed, sizeof(image_embed)
        )) {
        lv_img_set_src(ui_img, image_embed);
    }
}

static void update_status_glyph(
    lv_obj_t *ui_img, const struct theme_config *theme, const char *glyph_folder, const char *glyph_name
) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];

    if (generate_image_embed(
            mux_dim, glyph_folder, glyph_name, image_path, sizeof(image_path), image_embed, sizeof(image_embed)
        )) {
        const int header_target =
            resolve_glyph_size(config.settings.themeopt.glyph_size_header, theme->glyph.header, theme->header.height);
        const int header_px = glyph_explicit_px(config.settings.themeopt.glyph_size_header, theme->glyph.header);

        append_glyph_size_hint(image_embed, sizeof(image_embed), header_target);
        lv_img_set_src(ui_img, image_embed);
        apply_glyph_scale(ui_img, image_embed, header_px, header_px);
    }
}

void update_battery_capacity(lv_obj_t *ui_sta_capacity, const struct theme_config *theme) {
    const char *battery_glyph_name = battery_get_capacity_glyph();

    if (str_startswith(battery_glyph_name, "capacity_charging_")) {
        lv_obj_set_style_img_recolor(ui_sta_capacity, lv_color_hex(theme->status.battery.active), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_sta_capacity, theme->status.battery.active_alpha, MU_OBJ_MAIN_DEFAULT);
    } else if (battery_is_low()) {
        lv_obj_set_style_img_recolor(ui_sta_capacity, lv_color_hex(theme->status.battery.low), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_sta_capacity, theme->status.battery.low_alpha, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_img_recolor(ui_sta_capacity, lv_color_hex(theme->status.battery.normal), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_sta_capacity, theme->status.battery.normal_alpha, MU_OBJ_MAIN_DEFAULT);
    }

    update_status_glyph(ui_sta_capacity, theme, "header", battery_glyph_name);
}

void update_battery_percent_label(lv_obj_t *ui_label, const struct theme_config *theme) {
    if (!ui_label || !lv_obj_is_valid(ui_label)) return;

    const int percent = battery_get_capacity();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(ui_label, buf);

    if (battery_is_charging()) {
        lv_obj_set_style_text_color(ui_label, lv_color_hex(theme->status.battery.active), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(ui_label, theme->status.battery.active_alpha, MU_OBJ_MAIN_DEFAULT);
    } else if (battery_is_low()) {
        lv_obj_set_style_text_color(ui_label, lv_color_hex(theme->status.battery.low), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(ui_label, theme->status.battery.low_alpha, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_label, lv_color_hex(theme->status.battery.normal), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(ui_label, theme->status.battery.normal_alpha, MU_OBJ_MAIN_DEFAULT);
    }
}

void update_bluetooth_status(lv_obj_t *ui_sta_bluetooth, const struct theme_config *theme) {
    struct {
        lv_color_t color;
        lv_opa_t alpha;
        const char *status;
    } status_style;

    if (device.board.has_bluetooth && is_bluetooth_connected()) {
        status_style.color = lv_color_hex(theme->status.bluetooth.active);
        status_style.alpha = theme->status.bluetooth.active_alpha;
        status_style.status = "active";
    } else {
        status_style.color = lv_color_hex(theme->status.bluetooth.normal);
        status_style.alpha = theme->status.bluetooth.normal_alpha;
        status_style.status = "normal";
    }

    lv_obj_set_style_img_recolor(ui_sta_bluetooth, status_style.color, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_sta_bluetooth, status_style.alpha, MU_OBJ_MAIN_DEFAULT);

    const char *bluetooth_status_filename = status_style.status[0] == 'a' ? "bluetooth_active" : "bluetooth_normal";

    update_status_glyph(ui_sta_bluetooth, theme, "header", bluetooth_status_filename);
}

void update_network_status(lv_obj_t *ui_sta_network, const struct theme_config *theme, const int force_glyph) {
    struct {
        lv_color_t color;
        lv_opa_t alpha;
        const char *status;
    } status_style;

    if (force_glyph == 1 || (force_glyph == 0 && device.board.has_network && is_network_connected())) {
        status_style.color = lv_color_hex(theme->status.network.active);
        status_style.alpha = theme->status.network.active_alpha;
        status_style.status = "active";
    } else {
        status_style.color = lv_color_hex(theme->status.network.normal);
        status_style.alpha = theme->status.network.normal_alpha;
        status_style.status = "normal";
    }

    lv_obj_set_style_img_recolor(ui_sta_network, status_style.color, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_sta_network, status_style.alpha, MU_OBJ_MAIN_DEFAULT);

    const char *network_status_filename = status_style.status[0] == 'a' ? "network_active" : "network_normal";

    update_status_glyph(ui_sta_network, theme, "header", network_status_filename);
}

static void hide_message(const lv_timer_t *msg_timer) {
    lv_obj_t *target_obj = msg_timer->user_data;
    lv_obj_set_style_opa(target_obj, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    if (target_obj == ui_pnl_message && toast_timer == msg_timer) {
        toast_timer = NULL;
    } else if (counter_timer == msg_timer) {
        counter_timer = NULL;
    }
}

static void
show_message(lv_obj_t *panel, lv_obj_t *label, const char *msg, const uint32_t delay, lv_timer_t **msg_timer) {
    lv_label_set_text(label, msg);
    lv_label_set_recolor(label, 1);

    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    if (*msg_timer != NULL) {
        lv_timer_del(*msg_timer);
        *msg_timer = NULL;
    }

    if (delay > 0) {
        *msg_timer = lv_timer_create(hide_message, delay, panel);
        lv_timer_set_repeat_count(*msg_timer, 1);
    }
}

void toast_message(const char *msg, const uint32_t delay) {
    show_message(ui_pnl_message, ui_lbl_message, msg, delay, &toast_timer);

    if (delay == tst_wait_f) {
        lv_obj_move_foreground(ui_pnl_message);
        refresh_screen(ui_screen, 3);
        usleep(256);
    }
}

void counter_message(lv_obj_t *ui_lbl_counter, const char *msg, const uint32_t delay) {
    show_message(ui_lbl_counter, ui_lbl_counter, msg, delay, &counter_timer);
}

void adjust_panel_priority(lv_obj_t *panels[]) {
    for (size_t i = 0; panels[i] != NULL; i++) {
        lv_obj_move_foreground(panels[i]);
    }
}

int adjust_wallpaper_element(lv_group_t *ui_group, const int starter_image, const int wall_type) {
    if (config.boot.factory_reset) {
        int video_played = 0;
        if (config.visual.video_wallpaper) {
            const char *program = lv_obj_get_user_data(ui_screen);
            char mp4_path[MAX_BUFFER_SIZE];
            const char *ad_dims[] = {mux_dim, ""};
            for (size_t i = 0; i < 2 && !video_played; i++) {
                const int w =
                    snprintf(mp4_path, sizeof(mp4_path), "%s/%simage/wall/%s.mp4", theme_base, ad_dims[i], program);
                if (w > 0 && (size_t) w < sizeof(mp4_path) && file_exist(mp4_path)) {
                    video_wallpaper_play(mp4_path);
                    lv_img_set_src(ui_img_wall, &ui_img_blank);
                    lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                    set_gradient_visible(0);
                    video_played = 1;
                }
            }
        }
        if (!video_played) {
            char init_wall[MAX_BUFFER_SIZE];
            snprintf(init_wall, sizeof(init_wall), "M:%s/%simage/wall/default.png", theme_base, mux_dim);
            lv_img_header_t wall_hdr;
            if (lv_img_decoder_get_info(init_wall, &wall_hdr) == LV_RES_OK) {
                lv_img_set_src(ui_img_wall, init_wall);
            } else {
                lv_img_set_src(ui_img_wall, &ui_img_blank);
            }
        }
    } else {
        load_wallpaper(ui_screen, ui_group, ui_img_wall, wall_type);
    }

    static char static_image[MAX_BUFFER_SIZE];
    snprintf(static_image, sizeof(static_image), "%s", load_static_image(ui_screen, ui_group, wall_type));

    if (*static_image) {
        LOG_INFO(mux_module, "Loading Static Image: %s", static_image);

        switch (theme.misc.static_alignment) {
            case 0: // Bottom + Front
                lv_obj_set_x(ui_pnl_box, 0);
                lv_obj_set_y(ui_pnl_box, 0);
                lv_obj_set_width(ui_pnl_box, device.mux.width);
                lv_obj_set_height(ui_pnl_box, device.mux.height);
                lv_obj_set_align(ui_img_box, LV_ALIGN_BOTTOM_RIGHT);
                lv_obj_move_foreground(ui_pnl_box);
                break;
            case 1: // Middle + Front
                lv_obj_set_x(ui_pnl_box, 0);
                lv_obj_set_y(ui_pnl_box, 0);
                lv_obj_set_width(ui_pnl_box, device.mux.width);
                lv_obj_set_height(ui_pnl_box, device.mux.height);
                lv_obj_set_align(ui_img_box, LV_ALIGN_RIGHT_MID);
                lv_obj_move_foreground(ui_pnl_box);
                break;
            case 2: // Top + Front
                lv_obj_set_x(ui_pnl_box, 0);
                lv_obj_set_y(ui_pnl_box, 0);
                lv_obj_set_width(ui_pnl_box, device.mux.width);
                lv_obj_set_height(ui_pnl_box, device.mux.height);
                lv_obj_set_align(ui_img_box, LV_ALIGN_TOP_RIGHT);
                lv_obj_move_foreground(ui_pnl_box);
                break;
            case 3: // Fullscreen + Behind
                lv_obj_set_x(ui_pnl_box, 0);
                lv_obj_set_y(ui_pnl_box, 0);
                lv_obj_set_width(ui_pnl_box, device.mux.width);
                lv_obj_set_height(ui_pnl_box, device.mux.height);
                lv_obj_set_align(ui_img_box, LV_ALIGN_CENTER);
                lv_obj_move_background(ui_pnl_box);
                lv_obj_move_background(ui_pnl_wall);
                break;
            case 4: // Fullscreen + Front
                lv_obj_set_x(ui_pnl_box, 0);
                lv_obj_set_y(ui_pnl_box, 0);
                lv_obj_set_width(ui_pnl_box, device.mux.width);
                lv_obj_set_height(ui_pnl_box, device.mux.height);
                lv_obj_set_align(ui_img_box, LV_ALIGN_CENTER);
                lv_obj_move_foreground(ui_pnl_box);
                break;
            default:
                break;
        }

        const size_t slen = strlen(static_image);
        if (slen > 4 && strcmp(static_image + slen - 4, ".svg") == 0) {
            char svg_path[MAX_BUFFER_SIZE];
            snprintf(
                svg_path, sizeof(svg_path), "%s?%dx%d", static_image, lv_obj_get_width(ui_img_box),
                lv_obj_get_height(ui_img_box)
            );

            lv_img_set_src(ui_img_box, svg_path);
        } else {
            lv_img_set_src(ui_img_box, static_image);
        }
        return 1;
    }

    if (!starter_image) {
        lv_img_set_src(ui_img_box, &ui_img_blank);
        return 0;
    }

    return 1;
}

static int calc_grid_row_count(const struct theme_config *theme, const int item_count) {
    if (theme->grid.column_count <= 0) return 1;

    if (is_carousel_grid_mode()) return theme->grid.row_count > 0 ? theme->grid.row_count : 1;
    return (item_count + theme->grid.column_count - 1) / theme->grid.column_count;
}

void create_grid_panel(const struct theme_config *theme, const int item_count) {
    const int column_count = theme->grid.column_count > 0 ? theme->grid.column_count : 1;
    const int row_count = calc_grid_row_count(theme, item_count);

    if (grid_col_alloc < column_count + 1) {
        lv_coord_t *grid_alloc = lv_mem_realloc(grid_col_dsc, (column_count + 1) * sizeof(lv_coord_t));
        if (!grid_alloc) {
            LOG_ERROR(mux_module, "Grid column allocation failed");
            return;
        }

        grid_col_dsc = grid_alloc;
        grid_col_alloc = column_count + 1;
    }

    if (grid_row_alloc < row_count + 1) {
        lv_coord_t *grid_alloc = lv_mem_realloc(grid_row_dsc, (row_count + 1) * sizeof(lv_coord_t));
        if (!grid_alloc) {
            LOG_ERROR(mux_module, "Grid row allocation failed");
            return;
        }

        grid_row_dsc = grid_alloc;
        grid_row_alloc = row_count + 1;
    }

    for (int i = 0; i < column_count; i++)
        grid_col_dsc[i] = theme->grid.column_width == 0 ? LV_GRID_CONTENT : theme->grid.column_width;
    grid_col_dsc[column_count] = LV_GRID_TEMPLATE_LAST;

    for (int i = 0; i < row_count; i++)
        grid_row_dsc[i] = theme->grid.row_height == 0 ? LV_GRID_CONTENT : theme->grid.row_height;
    grid_row_dsc[row_count] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_style_grid_column_dsc_array(ui_pnl_grid, grid_col_dsc, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(ui_pnl_grid, grid_row_dsc, MU_OBJ_MAIN_DEFAULT);

    if (theme->grid.column_width == 0 && theme->grid.row_height == 0) {
        LOG_INFO(mux_module, "Setting Grid Size to: content");

        lv_obj_set_width(ui_pnl_grid, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_pnl_grid, LV_SIZE_CONTENT);
    } else {
        const int visible_rows = theme->grid.row_count > 0 ? theme->grid.row_count : row_count;
        int extra_rows = row_count - visible_rows;
        if (extra_rows < 0) extra_rows = 0;

        LOG_INFO(
            mux_module, "Setting Grid Size to: height (%d) width (%d)", theme->grid.row_height, theme->grid.column_width
        );

        lv_obj_set_size(ui_pnl_grid, column_count * theme->grid.column_width, visible_rows * theme->grid.row_height);
        lv_obj_set_style_pad_bottom(ui_pnl_grid, (extra_rows + 1) * theme->grid.row_height, MU_OBJ_MAIN_DEFAULT);
    }

    if (theme->grid.alignment == 0) {
        lv_obj_set_x(ui_pnl_grid, theme->grid.location_x);
        lv_obj_set_y(ui_pnl_grid, theme->grid.location_y);
    } else {
        lv_obj_align(
            ui_pnl_grid, theme->grid.alignment, theme->grid.alignment_x_offset, theme->grid.alignment_y_offset
        );
    }

    lv_obj_set_layout(ui_pnl_grid, LV_LAYOUT_GRID);
    lv_obj_set_style_bg_color(ui_pnl_grid, lv_color_hex(theme->grid.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_grid, theme->grid.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_scroll_dir(ui_pnl_grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_pnl_grid, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_snap_y(ui_pnl_grid, LV_SCROLL_SNAP_NONE);
    lv_obj_set_style_pad_row(ui_pnl_grid, theme->grid.row_padding, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_grid, theme->grid.column_padding, MU_OBJ_MAIN_DEFAULT);

    lv_obj_clear_flag(ui_pnl_grid_current_item, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(
        ui_pnl_grid_current_item, theme->grid.current_item_label.alignment, theme->grid.current_item_label.offset_x,
        theme->grid.current_item_label.offset_y
    );

    lv_obj_set_width(
        ui_pnl_grid_current_item,
        theme->grid.current_item_label.width == 0 ? LV_SIZE_CONTENT : theme->grid.current_item_label.width
    );
    lv_obj_set_height(
        ui_pnl_grid_current_item,
        theme->grid.current_item_label.height == 0 ? LV_SIZE_CONTENT : theme->grid.current_item_label.height
    );

    lv_obj_set_style_radius(ui_pnl_grid_current_item, theme->grid.current_item_label.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(
        ui_pnl_grid_current_item, theme->grid.current_item_label.border_width, MU_OBJ_MAIN_DEFAULT
    );

    lv_obj_set_style_bg_color(
        ui_pnl_grid_current_item, lv_color_hex(theme->grid.current_item_label.background), MU_OBJ_MAIN_DEFAULT
    );
    if (theme->grid.current_item_label.background_alpha < LV_OPA_COVER) {
        lv_obj_set_style_opa_layered(
            ui_pnl_grid_current_item, (lv_opa_t) theme->grid.current_item_label.background_alpha, MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_bg_opa(ui_pnl_grid_current_item, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(
            ui_pnl_grid_current_item, (lv_opa_t) theme->grid.current_item_label.background_alpha, MU_OBJ_MAIN_DEFAULT
        );
    }
    lv_obj_set_style_bg_grad_color(
        ui_pnl_grid_current_item, lv_color_hex(theme->grid.current_item_label.background_gradient_color),
        MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_main_stop(
        ui_pnl_grid_current_item, theme->grid.current_item_label.background_gradient_start, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_grad_stop(
        ui_pnl_grid_current_item, theme->grid.current_item_label.background_gradient_stop, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_grad_dir(
        ui_pnl_grid_current_item, theme->grid.current_item_label.background_gradient_direction, MU_OBJ_MAIN_DEFAULT
    );

    lv_obj_set_style_shadow_width(
        ui_pnl_grid_current_item, theme->grid.current_item_label.shadow_width, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_shadow_color(
        ui_pnl_grid_current_item, lv_color_hex(theme->grid.current_item_label.shadow), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_shadow_ofs_x(
        ui_pnl_grid_current_item, theme->grid.current_item_label.shadow_x_offset, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_shadow_ofs_y(
        ui_pnl_grid_current_item, theme->grid.current_item_label.shadow_y_offset, MU_OBJ_MAIN_DEFAULT
    );

    lv_obj_set_style_border_color(
        ui_pnl_grid_current_item, lv_color_hex(theme->grid.current_item_label.border), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_border_opa(
        ui_pnl_grid_current_item, theme->grid.current_item_label.border_alpha, MU_OBJ_MAIN_DEFAULT
    );

    lv_obj_set_style_pad_left(
        ui_pnl_grid_current_item, theme->grid.current_item_label.text_padding_left, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_pad_right(
        ui_pnl_grid_current_item, theme->grid.current_item_label.text_padding_right, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_pad_top(
        ui_pnl_grid_current_item, theme->grid.current_item_label.text_padding_top, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_pad_bottom(
        ui_pnl_grid_current_item, theme->grid.current_item_label.text_padding_bottom, MU_OBJ_MAIN_DEFAULT
    );

    lv_obj_set_width(ui_lbl_grid_current_item, LV_PCT(100));
    lv_obj_set_height(ui_lbl_grid_current_item, LV_SIZE_CONTENT);
    lv_obj_align(ui_lbl_grid_current_item, LV_ALIGN_CENTER, 0, -2);

    lv_label_set_long_mode(ui_lbl_grid_current_item, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(
        ui_lbl_grid_current_item, theme->grid.current_item_label.text_alignment, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_text_color(
        ui_lbl_grid_current_item, lv_color_hex(theme->grid.current_item_label.text), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_text_opa(ui_lbl_grid_current_item, theme->grid.current_item_label.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(
        ui_lbl_grid_current_item, theme->grid.current_item_label.text_line_spacing, MU_OBJ_MAIN_DEFAULT
    );
}

void grid_item_focus_event_cb(lv_event_t *e) {
    const lv_obj_t *cell_pnl = lv_event_get_target(e);
    const uint32_t child_cnt = lv_obj_get_child_cnt(cell_pnl);

    // Panel has no children (maybe being deleted)
    if (child_cnt == 0) return;

    const int32_t last_index = (int32_t) (child_cnt - 1);
    lv_obj_t *cell_image_focused = lv_obj_get_child(cell_pnl, last_index);
    if (!cell_image_focused) return;

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_set_style_img_opa(cell_image_focused, 255, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_width(cell_image_focused, LV_SIZE_CONTENT);
        lv_obj_set_height(cell_image_focused, LV_SIZE_CONTENT);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_img_opa(cell_image_focused, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_width(cell_image_focused, 0);
        lv_obj_set_height(cell_image_focused, 0);
    }
}

void create_grid_item(
    const struct theme_config *theme, lv_obj_t *cell_pnl, lv_obj_t *cell_label, lv_obj_t *cell_image, const int16_t col,
    const int16_t row, char *item_image_path, char *item_image_focused_path, const char *item_text
) {
    if (!grid_cell_shadow_style_ready) {
        lv_style_init(&grid_cell_shadow_style);
        grid_cell_shadow_style_ready = 1;
    } else {
        lv_style_reset(&grid_cell_shadow_style);
    }

    lv_style_set_shadow_width(&grid_cell_shadow_style, theme->grid.cell.shadow_width);
    lv_style_set_shadow_color(&grid_cell_shadow_style, lv_color_hex(theme->grid.cell.shadow));
    lv_style_set_shadow_ofs_x(&grid_cell_shadow_style, theme->grid.cell.shadow_x_offset);
    lv_style_set_shadow_ofs_y(&grid_cell_shadow_style, theme->grid.cell.shadow_y_offset);

    lv_obj_set_width(cell_pnl, theme->grid.cell.width == 0 ? LV_SIZE_CONTENT : theme->grid.cell.width);
    lv_obj_set_height(cell_pnl, theme->grid.cell.height == 0 ? LV_SIZE_CONTENT : theme->grid.cell.height);

    lv_obj_set_style_radius(cell_pnl, theme->grid.cell.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_clip_corner(cell_pnl, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(cell_pnl, theme->grid.cell.border_width, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_style(cell_pnl, &grid_cell_shadow_style, 0);

    lv_obj_set_style_bg_color(cell_pnl, lv_color_hex(theme->grid.cell_default.background), MU_OBJ_MAIN_DEFAULT);
    /* opa_layered renders bg+children into an off-screen layer so semi-transparent
     * cells composite correctly via premultiplied alpha over the video background. */
    if (theme->grid.cell_default.background_alpha < LV_OPA_COVER) {
        lv_obj_set_style_opa_layered(
            cell_pnl, (lv_opa_t) theme->grid.cell_default.background_alpha, MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_bg_opa(cell_pnl, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(cell_pnl, (lv_opa_t) theme->grid.cell_default.background_alpha, MU_OBJ_MAIN_DEFAULT);
    }
    lv_obj_set_style_bg_grad_color(
        cell_pnl, lv_color_hex(theme->grid.cell_default.background_gradient_color), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_bg_main_stop(cell_pnl, theme->grid.cell_default.background_gradient_start, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_stop(cell_pnl, theme->grid.cell_default.background_gradient_stop, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(cell_pnl, theme->grid.cell_default.background_gradient_direction, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(cell_pnl, lv_color_hex(theme->grid.cell_default.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(cell_pnl, theme->grid.cell_default.border_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_text_color(cell_label, lv_color_hex(theme->grid.cell_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(cell_label, theme->grid.cell_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(cell_label, theme->grid.cell.text_line_spacing, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_img_opa(cell_image, theme->grid.cell_default.image_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(
        cell_image, lv_color_hex(theme->grid.cell_default.image_recolour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_img_recolor_opa(cell_image, theme->grid.cell_default.image_recolour_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(cell_pnl, lv_color_hex(theme->grid.cell_focus.background), MU_OBJ_MAIN_FOCUS);
    if (theme->grid.cell_focus.background_alpha < LV_OPA_COVER) {
        lv_obj_set_style_opa_layered(cell_pnl, (lv_opa_t) theme->grid.cell_focus.background_alpha, MU_OBJ_MAIN_FOCUS);
        lv_obj_set_style_bg_opa(cell_pnl, LV_OPA_COVER, MU_OBJ_MAIN_FOCUS);
    } else {
        lv_obj_set_style_bg_opa(cell_pnl, (lv_opa_t) theme->grid.cell_focus.background_alpha, MU_OBJ_MAIN_FOCUS);
    }
    lv_obj_set_style_border_color(cell_pnl, lv_color_hex(theme->grid.cell_focus.border), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_grad_color(
        cell_pnl, lv_color_hex(theme->grid.cell_focus.background_gradient_color), MU_OBJ_MAIN_FOCUS
    );
    lv_obj_set_style_bg_main_stop(cell_pnl, theme->grid.cell_focus.background_gradient_start, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_grad_stop(cell_pnl, theme->grid.cell_focus.background_gradient_stop, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_grad_dir(cell_pnl, theme->grid.cell_focus.background_gradient_direction, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_border_opa(cell_pnl, theme->grid.cell_focus.border_alpha, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(cell_label, lv_color_hex(theme->grid.cell_focus.text), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_text_opa(cell_label, theme->grid.cell_focus.text_alpha, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_img_opa(cell_image, theme->grid.cell_focus.image_alpha, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_img_recolor(cell_image, lv_color_hex(theme->grid.cell_focus.image_recolour), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_img_recolor_opa(cell_image, theme->grid.cell_focus.image_recolour_alpha, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_grid_cell(cell_pnl, theme->grid.cell.column_align, col, 1, theme->grid.cell.row_align, row, 1);

    if (theme->grid.cell_default.text_alpha == 0 && theme->grid.cell_focus.text_alpha == 0) {
        lv_obj_align(cell_image, LV_ALIGN_CENTER, 0, theme->grid.cell.image_padding_top);
    } else {
        lv_obj_align(cell_image, LV_ALIGN_TOP_MID, 0, theme->grid.cell.image_padding_top);
    }

    // Grid glyph size: -1 native, 0 auto, > 0 fixed pixels
    int grid_hint_w;
    int grid_hint_h;

    int16_t grid_glyph = config.settings.themeopt.glyph_size_grid;
    if (grid_glyph == -2) grid_glyph = theme->glyph.grid;

    if (grid_glyph < 0) {
        grid_hint_w = grid_hint_h = -1;
    } else if (grid_glyph == 0) {
        grid_hint_w = theme->grid.cell.width * 3 / 4;
        grid_hint_h = theme->grid.cell.height * 3 / 4;
    } else {
        grid_hint_w = grid_hint_h = grid_glyph;
    }

    const int grid_px = grid_glyph > 0 ? grid_glyph : 0;

    if (item_image_path && *item_image_path && file_exist(item_image_path)) {
        char grid_image[MAX_BUFFER_SIZE];
        const size_t path_len = strlen(item_image_path);
        if (path_len > 4 && strcmp(item_image_path + path_len - 4, ".svg") == 0) {
            snprintf(grid_image, sizeof(grid_image), "M:%s?%dx%d", item_image_path, grid_hint_w, grid_hint_h);
        } else {
            snprintf(grid_image, sizeof(grid_image), "M:%s", item_image_path);
        }
        lv_img_set_src(cell_image, grid_image);
        apply_glyph_scale(cell_image, grid_image, grid_px, grid_px);
    } else {
        lv_img_set_src(cell_image, &ui_img_blank);
    }

    lv_obj_t *cell_image_focused = lv_img_create(cell_pnl);
    lv_obj_set_width(cell_image_focused, 0);
    lv_obj_set_height(cell_image_focused, 0);

    if (item_image_focused_path && *item_image_focused_path && file_exist(item_image_focused_path)) {
        char grid_image_focused[MAX_BUFFER_SIZE];
        const size_t path_len = strlen(item_image_focused_path);
        if (path_len > 4 && strcmp(item_image_focused_path + path_len - 4, ".svg") == 0) {
            snprintf(
                grid_image_focused, sizeof(grid_image_focused), "M:%s?%dx%d", item_image_focused_path, grid_hint_w,
                grid_hint_h
            );
        } else {
            snprintf(grid_image_focused, sizeof(grid_image_focused), "M:%s", item_image_focused_path);
        }
        lv_img_set_src(cell_image_focused, grid_image_focused);
        apply_glyph_scale(cell_image_focused, grid_image_focused, grid_px, grid_px);
    } else {
        lv_img_set_src(cell_image_focused, &ui_img_blank);
    }

    if (theme->grid.cell_default.text_alpha == 0 && theme->grid.cell_focus.text_alpha == 0) {
        lv_obj_align(cell_image_focused, LV_ALIGN_CENTER, 0, theme->grid.cell.image_padding_top);
    } else {
        lv_obj_align(cell_image_focused, LV_ALIGN_TOP_MID, 0, theme->grid.cell.image_padding_top);
    }

    lv_obj_set_style_img_opa(cell_image_focused, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(
        cell_image_focused, lv_color_hex(theme->grid.cell_focus.image_recolour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_img_recolor_opa(
        cell_image_focused, theme->grid.cell_focus.image_recolour_alpha, MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_add_event_cb(cell_pnl, grid_item_focus_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(cell_pnl, grid_item_focus_event_cb, LV_EVENT_DEFOCUSED, NULL);

    if (theme->grid.cell.width == 0
        || (theme->grid.cell_default.text_alpha == 0 && theme->grid.cell_focus.text_alpha == 0)) {
        lv_obj_set_width(cell_label, 0);
        lv_obj_set_height(cell_label, 0);
        lv_label_set_text(cell_label, "");
    } else {
        lv_obj_set_width(cell_label, theme->grid.cell.width - theme->grid.cell.text_padding_side * 2);
        lv_obj_set_height(cell_label, LV_SIZE_CONTENT);
        lv_label_set_text(cell_label, item_text ? item_text : "");
        lv_label_set_long_mode(cell_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(cell_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

        lv_obj_align(cell_label, LV_ALIGN_BOTTOM_MID, 0, -theme->grid.cell.text_padding_bottom);
    }
}

void scroll_help_content(const int direction, const int page_down) {
    if (lv_obj_get_content_height(ui_lbl_help_content) <= lv_obj_get_content_height(ui_pnl_help_content)) return;

    const int line_height = lv_font_get_line_height(lv_obj_get_style_text_font(ui_lbl_help_content, LV_PART_MAIN));
    const int line_space = lv_obj_get_style_text_line_space(ui_lbl_help_content, LV_PART_MAIN);
    int total_line_height = line_height + line_space;

    if (page_down) {
        const int lines_per_page = lv_obj_get_content_height(ui_pnl_help_content) / total_line_height;
        total_line_height = lines_per_page * total_line_height;
    }

    const int scroll_y = lv_obj_get_scroll_y(ui_pnl_help_content);

    if (scroll_y - total_line_height * direction < 0) {
        lv_obj_scroll_to_y(ui_pnl_help_content, 0, LV_ANIM_ON);
    } else if (scroll_y - total_line_height * direction
               >= lv_obj_get_content_height(ui_lbl_help_content) - lv_obj_get_content_height(ui_pnl_help_content)) {
        lv_obj_scroll_to_y(ui_pnl_help_content, lv_obj_get_content_height(ui_lbl_help_content), LV_ANIM_ON);
    } else {
        lv_obj_scroll_by(ui_pnl_help_content, 0, total_line_height * direction, LV_ANIM_ON);
    }
}

const char *help_lookup_message(const struct help_msg *map, const size_t count, const char *key) {
    if (!map || !count || !key || !*key) return lang.generic.no_help;

    for (size_t i = 0; i < count; i++) {
        if (map[i].key && strcmp(map[i].key, key) == 0) {
            const char *msg = map[i].message;
            if (msg && strlen(msg) > 1) return msg;
            break;
        }
    }

    return lang.generic.no_help;
}

void gen_help(
    const int current_index, const struct help_msg *help_messages, const size_t msg_count, const lv_group_t *group,
    const content_item *items
) {
    const char *title = lang.generic.unknown;
    const char *key = NULL;

    if (grid_mode_enabled) {
        if (items && current_index >= 0) {
            title = items[current_index].name ? items[current_index].name : lang.generic.unknown;
            key = items[current_index].glyph_icon;
        }
    } else {
        lv_obj_t *e_focused = group ? lv_group_get_focused(group) : NULL;

        if (e_focused) {
            title = TRS(lv_label_get_text(e_focused));
            key = lv_obj_get_user_data(e_focused);
        }
    }

    const char *msg = help_lookup_message(help_messages, msg_count, key);
    show_info_box(title, msg, 0);
}

void mu_img_no_shadow(lv_obj_t *img) {
    lv_obj_add_flag(img, LV_OBJ_FLAG_USER_1);
}
