#include <stdlib.h>
#include "../font/notosans.h"
#include "img/nothing.h"
#include "common.h"
#include "language.h"
#include "config.h"
#include "input.h"
#include "theme.h"
#include "device.h"
#include "ui_common.h"
#include "log.h"

lv_obj_t *ui_screen_container;
lv_obj_t *ui_screen_temp;
lv_obj_t *ui_blank;
lv_obj_t *ui_screen;
lv_obj_t *ui_pnlWall;
lv_obj_t *ui_imgWall;
lv_obj_t *ui_pnlContent;
lv_obj_t *ui_pnlGrid;
lv_obj_t *ui_pnlBox;
lv_obj_t *ui_imgBox;
lv_obj_t *ui_pnlHeader;
lv_obj_t *ui_lblDatetime;
lv_obj_t *ui_lblTitle;
lv_obj_t *ui_conGlyphs;
lv_obj_t *ui_staBluetooth;
lv_obj_t *ui_staNetwork;
lv_obj_t *ui_staCapacity;
lv_obj_t *ui_pnlFooter;
lv_obj_t *ui_lblGridCurrentItem;
lv_obj_t *ui_lblNavAGlyph;
lv_obj_t *ui_lblNavA;
lv_obj_t *ui_lblNavBGlyph;
lv_obj_t *ui_lblNavB;
lv_obj_t *ui_lblNavCGlyph;
lv_obj_t *ui_lblNavC;
lv_obj_t *ui_lblNavXGlyph;
lv_obj_t *ui_lblNavX;
lv_obj_t *ui_lblNavYGlyph;
lv_obj_t *ui_lblNavY;
lv_obj_t *ui_lblNavZGlyph;
lv_obj_t *ui_lblNavZ;
lv_obj_t *ui_lblNavMenuGlyph;
lv_obj_t *ui_lblNavMenu;
lv_obj_t *ui_lblScreenMessage;
lv_obj_t *ui_pnlMessage;
lv_obj_t *ui_lblMessage;
lv_obj_t *ui_pnlHelp;
lv_obj_t *ui_pnlHelpMessage;
lv_obj_t *ui_lblHelpHeader;
lv_obj_t *ui_pnlHelpContent;
lv_obj_t *ui_lblHelpContent;
lv_obj_t *ui_pnlHelpExtra;
lv_obj_t *ui_lblPreviewHeaderGlyph;
lv_obj_t *ui_lblPreviewHeader;
lv_obj_t *ui_pnlHelpPreview;
lv_obj_t *ui_lblHelpPreviewHeader;
lv_obj_t *ui_pnlHelpPreviewImage;
lv_obj_t *ui_imgHelpPreviewImage;
lv_obj_t *ui_pnlHelpPreviewInfo;
lv_obj_t *ui_lblHelpPreviewInfoGlyph;
lv_obj_t *ui_lblHelpPreviewInfoMessage;
lv_obj_t *ui_pnlProgressBrightness;
lv_obj_t *ui_icoProgressBrightness;
lv_obj_t *ui_barProgressBrightness;
lv_obj_t *ui_pnlProgressVolume;
lv_obj_t *ui_icoProgressVolume;
lv_obj_t *ui_barProgressVolume;

// Global buffer for the canvas
static lv_color_t *cbuf;

// 4x4 Bayer Ordered Dithering Matrix (Normalized to 0-16)
const int bayerMatrix[4][4] = {
        {0,  8,  2,  10},
        {12, 4,  14, 6},
        {3,  11, 1,  9},
        {15, 7,  13, 5}
};

// Improved dithering function using Bayer matrix
static lv_color_t dither_color(lv_color_t color, int x, int y) {
    // Get matrix value (normalized to range -4 to +4)
    int bayerValue = bayerMatrix[y % 4][x % 4] - 7;

    int r = LV_COLOR_GET_R(color) + bayerValue;
    int g = LV_COLOR_GET_G(color) + bayerValue;
    int b = LV_COLOR_GET_B(color) + bayerValue;

    // Clamp values
    r = LV_CLAMP(0, r, 255);
    g = LV_CLAMP(0, g, 255);
    b = LV_CLAMP(0, b, 255);

    return lv_color_make(r, g, b);
}

void blur_gradient(lv_color_t *buf, int width, int height, int blur_strength) {
    for (int pass = 0; pass < blur_strength; pass++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                // Use a 3×3 box blur (increases smoothness)
                int r = (
                                LV_COLOR_GET_R(buf[(y - 1) * width + (x - 1)]) +
                                LV_COLOR_GET_R(buf[(y - 1) * width + x]) +
                                LV_COLOR_GET_R(buf[(y - 1) * width + (x + 1)]) +
                                LV_COLOR_GET_R(buf[y * width + (x - 1)]) +
                                LV_COLOR_GET_R(buf[y * width + x]) +
                                LV_COLOR_GET_R(buf[y * width + (x + 1)]) +
                                LV_COLOR_GET_R(buf[(y + 1) * width + (x - 1)]) +
                                LV_COLOR_GET_R(buf[(y + 1) * width + x]) +
                                LV_COLOR_GET_R(buf[(y + 1) * width + (x + 1)])
                        ) / 9;  // Average 9 surrounding pixels

                int g = (
                                LV_COLOR_GET_G(buf[(y - 1) * width + (x - 1)]) +
                                LV_COLOR_GET_G(buf[(y - 1) * width + x]) +
                                LV_COLOR_GET_G(buf[(y - 1) * width + (x + 1)]) +
                                LV_COLOR_GET_G(buf[y * width + (x - 1)]) +
                                LV_COLOR_GET_G(buf[y * width + x]) +
                                LV_COLOR_GET_G(buf[y * width + (x + 1)]) +
                                LV_COLOR_GET_G(buf[(y + 1) * width + (x - 1)]) +
                                LV_COLOR_GET_G(buf[(y + 1) * width + x]) +
                                LV_COLOR_GET_G(buf[(y + 1) * width + (x + 1)])
                        ) / 9;

                int b = (
                                LV_COLOR_GET_B(buf[(y - 1) * width + (x - 1)]) +
                                LV_COLOR_GET_B(buf[(y - 1) * width + x]) +
                                LV_COLOR_GET_B(buf[(y - 1) * width + (x + 1)]) +
                                LV_COLOR_GET_B(buf[y * width + (x - 1)]) +
                                LV_COLOR_GET_B(buf[y * width + x]) +
                                LV_COLOR_GET_B(buf[y * width + (x + 1)]) +
                                LV_COLOR_GET_B(buf[(y + 1) * width + (x - 1)]) +
                                LV_COLOR_GET_B(buf[(y + 1) * width + x]) +
                                LV_COLOR_GET_B(buf[(y + 1) * width + (x + 1)])
                        ) / 9;

                buf[y * width + x] = lv_color_make(r, g, b);
            }
        }
    }
}

void generate_gradient_with_bayer_dither(lv_color_t *buf, int width, int height,
                                         lv_color_t start_color, lv_color_t end_color,
                                         bool apply_dither, bool vertical,
                                         uint8_t main_stop, uint8_t grad_stop) {
    // Convert gradient stop values (0-255) to pixel positions
    int start_pos = (main_stop / 255.0) * (vertical ? height : width);
    int end_pos = (grad_stop / 255.0) * (vertical ? height : width);

    // Prevent invalid cases where start is beyond end
    if (start_pos >= end_pos) {
        start_pos = 0;
        end_pos = vertical ? height : width;
    }

    // Iterate over each pixel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Determine pixel position in gradient
            int pos = vertical ? y : x;

            // Normalize position within gradient stops
            float ratio = (float) (pos - start_pos) / (float) (end_pos - start_pos);
            ratio = LV_CLAMP(0.0f, ratio, 1.0f); // Ensure within valid range

            // Compute interpolated color values
            uint8_t r = (uint8_t) ((1.0f - ratio) * LV_COLOR_GET_R(start_color) + ratio * LV_COLOR_GET_R(end_color));
            uint8_t g = (uint8_t) ((1.0f - ratio) * LV_COLOR_GET_G(start_color) + ratio * LV_COLOR_GET_G(end_color));
            uint8_t b = (uint8_t) ((1.0f - ratio) * LV_COLOR_GET_B(start_color) + ratio * LV_COLOR_GET_B(end_color));

            // Ensure valid LVGL color format
            lv_color_t final_color = lv_color_make(r, g, b);

            // Apply Bayer dithering if enabled
            if (apply_dither) {
                final_color = dither_color(final_color, x, y);
            }

            // Assign the final color to the buffer
            buf[y * width + x] = final_color;
        }
    }
}

void apply_gradient_to_ui_screen(lv_obj_t *ui_screen, struct theme_config *theme, struct mux_device *device) {
    if (theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION == LV_GRAD_DIR_NONE) return;

    // Allocate memory for the canvas buffer
    cbuf = lv_mem_alloc(device->MUX.WIDTH * device->MUX.HEIGHT * sizeof(lv_color_t));

    if (!cbuf) {
        LV_LOG_ERROR("Failed to allocate memory for canvas buffer!");
        return;
    }

    // Create a canvas
    lv_obj_t *canvas = lv_canvas_create(ui_screen);
    lv_canvas_set_buffer(canvas, cbuf, device->MUX.WIDTH, device->MUX.HEIGHT, LV_IMG_CF_TRUE_COLOR);

    // Set size and position to cover the full screen
    lv_obj_set_size(canvas, device->MUX.WIDTH, device->MUX.HEIGHT);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);  // Center on the screen

    // Define gradient colors
    lv_color_t start_color = lv_color_hex(theme->SYSTEM.BACKGROUND); // Black
    lv_color_t end_color = lv_color_hex(theme->SYSTEM.BACKGROUND_GRADIENT_COLOR);   // Dark Gray

    // Generate the gradient with dithering
    generate_gradient_with_bayer_dither(cbuf, device->MUX.WIDTH, device->MUX.HEIGHT, start_color, end_color,
                                        theme->SYSTEM.BACKGROUND_GRADIENT_DITHER == 1,
                                        theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION == LV_GRAD_DIR_VER,
                                        theme->SYSTEM.BACKGROUND_GRADIENT_START,
                                        theme->SYSTEM.BACKGROUND_GRADIENT_STOP);
    blur_gradient(cbuf, device->MUX.WIDTH, device->MUX.HEIGHT, theme->SYSTEM.BACKGROUND_GRADIENT_BLUR);

    // Refresh the canvas
    lv_obj_invalidate(canvas);
}

void init_ui_common_screen(struct theme_config *theme, struct mux_device *device,
                           struct mux_lang *lang, const char *title) {
    ui_screen_container = lv_obj_create(NULL);
    if (ui_screen_temp == NULL) ui_screen_temp = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(ui_screen_container, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);

    apply_gradient_to_ui_screen(ui_screen_container, theme, device);

    ui_blank = lv_obj_create(ui_screen_container);
    lv_obj_set_width(ui_blank, device->MUX.WIDTH);
    lv_obj_set_height(ui_blank, device->MUX.HEIGHT);
    lv_obj_set_align(ui_blank, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_blank, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_blank, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_screen = lv_obj_create(ui_screen_container);
    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(theme->SYSTEM.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, theme->SYSTEM.BACKGROUND_GRADIENT_DIRECTION == LV_GRAD_DIR_NONE
                                       ? theme->SYSTEM.BACKGROUND_ALPHA : 0,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_font(ui_screen, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(ui_screen, device->MUX.WIDTH);
    lv_obj_set_height(ui_screen, device->MUX.HEIGHT);
    lv_obj_set_align(ui_screen, LV_ALIGN_CENTER);

    ui_pnlWall = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlWall, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlWall, device->MUX.HEIGHT);
    lv_obj_set_align(ui_pnlWall, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlWall, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                  LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnlWall, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnlWall, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_pnlWall, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnlWall, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlWall, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    ui_imgWall = lv_img_create(ui_pnlWall);
    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    lv_obj_set_width(ui_imgWall, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgWall, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgWall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgWall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgWall, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnlGrid = lv_obj_create(ui_screen);
    ui_lblGridCurrentItem = lv_label_create(ui_screen);
    lv_label_set_text(ui_lblGridCurrentItem, "");
    lv_obj_add_flag(ui_lblGridCurrentItem, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

    ui_pnlContent = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlContent, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlContent, theme->MISC.CONTENT.HEIGHT);
    lv_obj_set_x(ui_pnlContent, 0);
    lv_obj_set_y(ui_pnlContent, theme->HEADER.HEIGHT + 2 + theme->MISC.CONTENT.PADDING_TOP);
    lv_obj_set_flex_flow(ui_pnlContent, LV_FLEX_FLOW_COLUMN);
    if (theme->MISC.CONTENT.ALIGNMENT == 1) {
        lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(ui_pnlContent, theme->MISC.CONTENT.PADDING_LEFT * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (theme->MISC.CONTENT.ALIGNMENT == 2) {
        lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_right(ui_pnlContent, -theme->MISC.CONTENT.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_left(ui_pnlContent, theme->MISC.CONTENT.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_clear_flag(ui_pnlContent, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnlContent, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnlContent, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_pnlContent, lv_color_hex(0x0D0803), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlContent, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnlContent, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlContent, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlContent, lv_color_hex(0xFFFFFF), LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
    lv_obj_set_style_bg_opa(ui_pnlContent, 0, LV_PART_SCROLLBAR | LV_STATE_SCROLLED);

    ui_pnlBox = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlBox, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlBox, device->MUX.HEIGHT - theme->HEADER.HEIGHT - theme->FOOTER.HEIGHT - 4);
    lv_obj_set_x(ui_pnlBox, 0);
    lv_obj_set_y(ui_pnlBox, theme->HEADER.HEIGHT + 2);
    lv_obj_set_align(ui_pnlBox, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(ui_pnlBox, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                 LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_pnlBox, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_pnlBox, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_pnlBox, lv_color_hex(0x0D0803), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlBox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_pnlBox, lv_color_hex(0x000000), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlBox, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    ui_imgBox = lv_img_create(ui_pnlBox);
    lv_img_set_src(ui_imgBox, &ui_image_Nothing);
    lv_obj_set_width(ui_imgBox, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgBox, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_imgBox, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(ui_imgBox, true, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_imgBox, theme->IMAGE_LIST.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui_imgBox, theme->IMAGE_LIST.ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_imgBox, lv_color_hex(theme->IMAGE_LIST.RECOLOUR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgBox, theme->IMAGE_LIST.RECOLOUR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_imgBox, theme->IMAGE_LIST.PAD_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_imgBox, theme->IMAGE_LIST.PAD_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_imgBox, theme->IMAGE_LIST.PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_imgBox, theme->IMAGE_LIST.PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHeader = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlHeader, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlHeader, theme->HEADER.HEIGHT);
    lv_obj_set_align(ui_pnlHeader, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnlHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHeader, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHeader, lv_color_hex(theme->HEADER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHeader, theme->HEADER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblDatetime = lv_label_create(ui_pnlHeader);
    lv_obj_set_width(ui_lblDatetime, device->MUX.WIDTH);
    lv_obj_set_height(ui_lblDatetime, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui_lblDatetime, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblDatetime, "");
    lv_obj_clear_flag(ui_lblDatetime, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_align(ui_lblDatetime, theme->DATETIME.ALIGN, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblDatetime, lv_color_hex(theme->DATETIME.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblDatetime, theme->DATETIME.ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblDatetime, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblDatetime, theme->DATETIME.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblDatetime, theme->DATETIME.PADDING_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblDatetime, theme->FONT.HEADER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblDatetime, theme->FONT.HEADER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblTitle = lv_label_create(ui_pnlHeader);
    lv_obj_set_width(ui_lblTitle, device->MUX.WIDTH);
    lv_obj_set_height(ui_lblTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblTitle, LV_ALIGN_TOP_MID);
    lv_label_set_long_mode(ui_lblTitle, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblTitle, title);
    lv_obj_set_style_text_color(ui_lblTitle, lv_color_hex(theme->HEADER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblTitle, theme->HEADER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblTitle, theme->HEADER.TEXT_ALIGN, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblTitle, theme->HEADER.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblTitle, theme->HEADER.PADDING_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblTitle, theme->FONT.HEADER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblTitle, theme->FONT.HEADER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_conGlyphs = lv_obj_create(ui_pnlHeader);
    lv_obj_set_width(ui_conGlyphs, device->MUX.WIDTH);
    lv_obj_set_height(ui_conGlyphs, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_conGlyphs, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_conGlyphs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_conGlyphs, theme->STATUS.ALIGN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conGlyphs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conGlyphs, theme->STATUS.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conGlyphs, theme->STATUS.PADDING_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conGlyphs, theme->FONT.HEADER_ICON_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conGlyphs, theme->FONT.HEADER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_staBluetooth = create_header_glyph(ui_conGlyphs, theme);
    //update_bluetooth_status(ui_staBluetooth, theme);

    ui_staNetwork = create_header_glyph(ui_conGlyphs, theme);
    update_network_status(ui_staNetwork, theme, 0);
    if (!config.VISUAL.NETWORK) lv_obj_add_flag(ui_staNetwork, LV_OBJ_FLAG_HIDDEN);

    ui_staCapacity = create_header_glyph(ui_conGlyphs, theme);
    battery_capacity = read_battery_capacity();
    update_battery_capacity(ui_staCapacity, theme);

    ui_pnlFooter = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlFooter, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlFooter, theme->FOOTER.HEIGHT);
    lv_obj_set_align(ui_pnlFooter, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(ui_pnlFooter, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlFooter, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlFooter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlFooter, lv_color_hex(theme->FOOTER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlFooter, theme->FOOTER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_flex_align_t e_align;
    switch (theme->NAV.ALIGNMENT) {
        case 1:
            e_align = LV_FLEX_ALIGN_CENTER;
            break;
        case 2:
            lv_obj_set_style_pad_right(ui_pnlFooter, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
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
            lv_obj_set_style_pad_left(ui_pnlFooter, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
            e_align = LV_FLEX_ALIGN_START;
            break;
    }
    lv_obj_set_style_flex_main_place(ui_pnlFooter, e_align, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblNavAGlyph = create_footer_glyph(ui_pnlFooter, theme, (config.SETTINGS.ADVANCED.SWAP) ? "b" : "a",
                                          theme->NAV.A);
    ui_lblNavA = create_footer_text(ui_pnlFooter, theme, theme->NAV.A.TEXT, theme->NAV.A.TEXT_ALPHA);

    ui_lblNavBGlyph = create_footer_glyph(ui_pnlFooter, theme, (config.SETTINGS.ADVANCED.SWAP) ? "a" : "b",
                                          theme->NAV.B);
    ui_lblNavB = create_footer_text(ui_pnlFooter, theme, theme->NAV.B.TEXT, theme->NAV.B.TEXT_ALPHA);

    ui_lblNavCGlyph = create_footer_glyph(ui_pnlFooter, theme, "c", theme->NAV.C);
    ui_lblNavC = create_footer_text(ui_pnlFooter, theme, theme->NAV.C.TEXT, theme->NAV.C.TEXT_ALPHA);

    ui_lblNavXGlyph = create_footer_glyph(ui_pnlFooter, theme, (config.SETTINGS.ADVANCED.SWAP) ? "y" : "x",
                                          theme->NAV.X);
    ui_lblNavX = create_footer_text(ui_pnlFooter, theme, theme->NAV.X.TEXT, theme->NAV.X.TEXT_ALPHA);

    ui_lblNavYGlyph = create_footer_glyph(ui_pnlFooter, theme, (config.SETTINGS.ADVANCED.SWAP) ? "x" : "y",
                                          theme->NAV.Y);
    ui_lblNavY = create_footer_text(ui_pnlFooter, theme, theme->NAV.Y.TEXT, theme->NAV.Y.TEXT_ALPHA);

    ui_lblNavZGlyph = create_footer_glyph(ui_pnlFooter, theme, "z", theme->NAV.Z);
    ui_lblNavZ = create_footer_text(ui_pnlFooter, theme, theme->NAV.Z.TEXT, theme->NAV.Z.TEXT_ALPHA);

    ui_lblNavMenuGlyph = create_footer_glyph(ui_pnlFooter, theme, "menu", theme->NAV.MENU);
    ui_lblNavMenu = create_footer_text(ui_pnlFooter, theme, theme->NAV.MENU.TEXT, theme->NAV.MENU.TEXT_ALPHA);

    ui_lblScreenMessage = lv_label_create(ui_screen);
    lv_label_set_text(ui_lblScreenMessage, "");
    lv_obj_set_width(ui_lblScreenMessage, device->MUX.WIDTH);
    lv_obj_set_height(ui_lblScreenMessage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblScreenMessage, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_scroll_dir(ui_lblScreenMessage, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblScreenMessage, lv_color_hex(theme->LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblScreenMessage, theme->LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblScreenMessage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblScreenMessage, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblScreenMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblScreenMessage, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblScreenMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblScreenMessage, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblScreenMessage, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblScreenMessage, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblScreenMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblScreenMessage, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblScreenMessage, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblScreenMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblScreenMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblScreenMessage, theme->FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblScreenMessage, theme->FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblScreenMessage, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblScreenMessage, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_pnlMessage = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlMessage, device->MUX.WIDTH - 25);
    lv_obj_set_height(ui_pnlMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_pnlMessage, 0);
    lv_obj_set_y(ui_pnlMessage, -theme->FOOTER.HEIGHT - 5);
    lv_obj_set_align(ui_pnlMessage, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(ui_pnlMessage, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlMessage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlMessage, theme->MESSAGE.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlMessage, lv_color_hex(theme->MESSAGE.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlMessage, theme->MESSAGE.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlMessage, lv_color_hex(theme->MESSAGE.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlMessage, theme->MESSAGE.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlMessage, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlMessage, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblMessage = lv_label_create(ui_pnlMessage);
    lv_obj_set_width(ui_lblMessage, device->MUX.WIDTH - 50);
    lv_obj_set_height(ui_lblMessage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblMessage, "");
    lv_label_set_recolor(ui_lblMessage, "true");
    lv_obj_set_style_text_color(ui_lblMessage, lv_color_hex(theme->MESSAGE.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMessage, theme->MESSAGE.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblMessage, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblMessage, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblMessage, theme->FONT.MESSAGE_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblMessage, theme->FONT.MESSAGE_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelp = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlHelp, device->MUX.WIDTH);
    lv_obj_set_height(ui_pnlHelp, device->MUX.HEIGHT);
    lv_obj_set_align(ui_pnlHelp, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelp, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelp, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelp, 155, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlHelp, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlHelp, 155, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelp, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpMessage = lv_obj_create(ui_pnlHelp);
    lv_obj_set_width(ui_pnlHelpMessage, device->MUX.WIDTH * .9);
    lv_obj_set_height(ui_pnlHelpMessage, device->MUX.HEIGHT * .9);
    lv_obj_set_align(ui_pnlHelpMessage, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpMessage, theme->HELP.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpMessage, lv_color_hex(theme->HELP.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpMessage, theme->HELP.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlHelpMessage, lv_color_hex(theme->HELP.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlHelpMessage, theme->HELP.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpMessage, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpMessage, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpHeader = lv_label_create(ui_pnlHelpMessage);
    lv_obj_set_width(ui_lblHelpHeader, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_lblHelpHeader, LV_SIZE_CONTENT);
    lv_obj_align(ui_lblHelpHeader, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_label_set_long_mode(ui_lblHelpHeader, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblHelpHeader, "");
    lv_obj_set_style_text_color(ui_lblHelpHeader, lv_color_hex(theme->HELP.TITLE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpHeader, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpContent = lv_obj_create(ui_pnlHelp);
    lv_obj_set_align(ui_pnlHelpContent, LV_ALIGN_CENTER);
    lv_obj_set_width(ui_pnlHelpContent, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_pnlHelpContent, device->MUX.HEIGHT * .9 - 120);
    lv_obj_clear_flag(ui_pnlHelpContent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui_pnlHelpContent, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui_pnlHelpContent, LV_SCROLL_SNAP_NONE);

    ui_lblHelpContent = lv_label_create(ui_pnlHelpContent);
    lv_obj_set_width(ui_lblHelpContent, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_lblHelpContent, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHelpContent, LV_ALIGN_TOP_LEFT);
    lv_label_set_long_mode(ui_lblHelpContent, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ui_lblHelpContent, "");
    lv_obj_set_style_text_color(ui_lblHelpContent, lv_color_hex(theme->HELP.CONTENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpContent, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblHelpContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblHelpContent, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpExtra = lv_obj_create(ui_pnlHelpMessage);
    lv_obj_set_width(ui_pnlHelpExtra, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_pnlHelpExtra, theme->FOOTER.HEIGHT);
    lv_obj_align(ui_pnlHelpExtra, LV_ALIGN_BOTTOM_LEFT, 15, 0);
    lv_obj_set_flex_flow(ui_pnlHelpExtra, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlHelpExtra, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpExtra, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                       LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE |
                                       LV_OBJ_FLAG_SCROLL_ELASTIC |
                                       LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_radius(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpExtra, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpExtra, LV_BORDER_SIDE_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblPreviewHeaderGlyph = create_footer_glyph(ui_pnlHelpExtra, theme, (config.SETTINGS.ADVANCED.SWAP) ? "b" : "a",
                                                   theme->NAV.A);

    ui_lblPreviewHeader = create_footer_text(ui_pnlHelpExtra, theme, theme->NAV.A.TEXT, theme->NAV.A.TEXT_ALPHA);
    lv_label_set_text(ui_lblPreviewHeader, lang->GENERIC.SWITCH_IMAGE);

    ui_pnlHelpPreview = lv_obj_create(ui_pnlHelp);
    lv_obj_set_width(ui_pnlHelpPreview, device->MUX.WIDTH * .9);
    lv_obj_set_height(ui_pnlHelpPreview, device->MUX.HEIGHT * .9);
    lv_obj_set_align(ui_pnlHelpPreview, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreview, theme->HELP.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreview, lv_color_hex(theme->HELP.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreview, theme->HELP.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlHelpPreview, lv_color_hex(theme->HELP.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlHelpPreview, theme->HELP.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpPreview, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpPreview, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpPreviewHeader = lv_label_create(ui_pnlHelpPreview);
    lv_obj_set_width(ui_lblHelpPreviewHeader, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_lblHelpPreviewHeader, LV_SIZE_CONTENT);
    lv_obj_align(ui_lblHelpPreviewHeader, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_label_set_long_mode(ui_lblHelpPreviewHeader, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblHelpPreviewHeader, "");
    lv_obj_set_style_text_color(ui_lblHelpPreviewHeader, lv_color_hex(theme->HELP.TITLE),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpPreviewHeader, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpPreviewImage = lv_obj_create(ui_pnlHelpPreview);
    lv_obj_set_width(ui_pnlHelpPreviewImage, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_pnlHelpPreviewImage, device->MUX.HEIGHT * .9 - 120);
    lv_obj_set_align(ui_pnlHelpPreviewImage, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpPreviewImage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreviewImage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_imgHelpPreviewImage = lv_img_create(ui_pnlHelpPreviewImage);
    lv_img_set_src(ui_imgHelpPreviewImage, &ui_image_Nothing);
    lv_obj_set_width(ui_imgHelpPreviewImage, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgHelpPreviewImage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgHelpPreviewImage, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgHelpPreviewImage, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgHelpPreviewImage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_imgHelpPreviewImage, theme->IMAGE_PREVIEW.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_imgHelpPreviewImage, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_imgHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui_imgHelpPreviewImage, true, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui_imgHelpPreviewImage, theme->IMAGE_PREVIEW.ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_imgHelpPreviewImage, lv_color_hex(theme->IMAGE_PREVIEW.RECOLOUR),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_imgHelpPreviewImage, theme->IMAGE_PREVIEW.RECOLOUR_ALPHA,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpPreviewInfo = lv_obj_create(ui_pnlHelpPreview);
    lv_obj_set_width(ui_pnlHelpPreviewInfo, device->MUX.WIDTH * .9 - 60);
    lv_obj_set_height(ui_pnlHelpPreviewInfo, theme->FOOTER.HEIGHT);
    lv_obj_align(ui_pnlHelpPreviewInfo, LV_ALIGN_BOTTOM_LEFT, 15, 0);
    lv_obj_set_flex_flow(ui_pnlHelpPreviewInfo, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlHelpPreviewInfo, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpPreviewInfo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreviewInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpPreviewInfo, LV_BORDER_SIDE_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpPreviewInfoGlyph = create_footer_glyph(ui_pnlHelpPreviewInfo, theme,
                                                     (config.SETTINGS.ADVANCED.SWAP) ? "b" : "a",
                                                     theme->NAV.A);

    ui_lblHelpPreviewInfoMessage = create_footer_text(ui_pnlHelpPreviewInfo, theme, theme->NAV.A.TEXT,
                                                      theme->NAV.A.TEXT_ALPHA);
    lv_label_set_text(ui_lblHelpPreviewInfoMessage, lang->GENERIC.SWITCH_INFO);

    ui_pnlProgressBrightness = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlProgressBrightness, theme->BAR.PANEL_WIDTH);
    lv_obj_set_height(ui_pnlProgressBrightness, theme->BAR.PANEL_HEIGHT);
    lv_obj_set_x(ui_pnlProgressBrightness, 0);
    lv_obj_set_y(ui_pnlProgressBrightness, theme->BAR.Y_POS);
    lv_obj_set_align(ui_pnlProgressBrightness, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnlProgressBrightness, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlProgressBrightness, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlProgressBrightness, theme->BAR.PANEL_BORDER_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlProgressBrightness, lv_color_hex(theme->BAR.PANEL_BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlProgressBrightness, theme->BAR.PANEL_BACKGROUND_ALPHA,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlProgressBrightness, lv_color_hex(theme->BAR.PANEL_BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlProgressBrightness, theme->BAR.PANEL_BORDER_ALPHA,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlProgressBrightness, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlProgressBrightness, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_icoProgressBrightness = lv_img_create(ui_pnlProgressBrightness);
    lv_obj_set_align(ui_icoProgressBrightness, LV_ALIGN_CENTER);
    lv_obj_set_style_img_recolor(ui_icoProgressBrightness, lv_color_hex(theme->BAR.ICON),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_icoProgressBrightness, theme->BAR.ICON_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    update_glyph(ui_icoProgressBrightness, "bar", "brightness");

    ui_barProgressBrightness = lv_bar_create(ui_pnlProgressBrightness);
    lv_bar_set_value(ui_barProgressBrightness, 0, LV_ANIM_ON);
    lv_bar_set_start_value(ui_barProgressBrightness, 0, LV_ANIM_ON);
    lv_obj_set_width(ui_barProgressBrightness, theme->BAR.PROGRESS_WIDTH);
    lv_obj_set_height(ui_barProgressBrightness, theme->BAR.PROGRESS_HEIGHT);
    lv_obj_set_align(ui_barProgressBrightness, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_barProgressBrightness, theme->BAR.PROGRESS_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgressBrightness, lv_color_hex(theme->BAR.PROGRESS_MAIN_BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgressBrightness, theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_barProgressBrightness, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgressBrightness, lv_color_hex(theme->BAR.PROGRESS_ACTIVE_BACKGROUND),
                              LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgressBrightness, theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_pnlProgressVolume = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlProgressVolume, theme->BAR.PANEL_WIDTH);
    lv_obj_set_height(ui_pnlProgressVolume, theme->BAR.PANEL_HEIGHT);
    lv_obj_set_x(ui_pnlProgressVolume, 0);
    lv_obj_set_y(ui_pnlProgressVolume, theme->BAR.Y_POS);
    lv_obj_set_align(ui_pnlProgressVolume, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_pnlProgressVolume, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlProgressVolume, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlProgressVolume, theme->BAR.PANEL_BORDER_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlProgressVolume, lv_color_hex(theme->BAR.PANEL_BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlProgressVolume, theme->BAR.PANEL_BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlProgressVolume, lv_color_hex(theme->BAR.PANEL_BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlProgressVolume, theme->BAR.PANEL_BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlProgressVolume, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlProgressVolume, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_icoProgressVolume = lv_img_create(ui_pnlProgressVolume);
    lv_obj_set_align(ui_icoProgressVolume, LV_ALIGN_CENTER);
    lv_obj_set_style_img_recolor(ui_icoProgressVolume, lv_color_hex(theme->BAR.ICON),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_icoProgressVolume, theme->BAR.ICON_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    update_glyph(ui_icoProgressVolume, "bar", "volume_0");

    ui_barProgressVolume = lv_bar_create(ui_pnlProgressVolume);
    lv_bar_set_value(ui_barProgressVolume, 0, LV_ANIM_ON);
    lv_bar_set_start_value(ui_barProgressVolume, 0, LV_ANIM_ON);
    if (config.SETTINGS.ADVANCED.OVERDRIVE) {
        lv_bar_set_range(ui_barProgressVolume, 0, 200);
    } else {
        lv_bar_set_range(ui_barProgressVolume, 0, 100);
    }
    lv_obj_set_width(ui_barProgressVolume, theme->BAR.PROGRESS_WIDTH);
    lv_obj_set_height(ui_barProgressVolume, theme->BAR.PROGRESS_HEIGHT);
    lv_obj_set_align(ui_barProgressVolume, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_barProgressVolume, theme->BAR.PROGRESS_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgressVolume, lv_color_hex(theme->BAR.PROGRESS_MAIN_BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgressVolume, theme->BAR.PROGRESS_MAIN_BACKGROUND_ALPHA,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_barProgressVolume, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgressVolume, lv_color_hex(theme->BAR.PROGRESS_ACTIVE_BACKGROUND),
                              LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgressVolume, theme->BAR.PROGRESS_ACTIVE_BACKGROUND_ALPHA,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_disp_load_scr(ui_screen_container);
}

void ui_common_handle_bright() {
    if (read_int_from_file("/tmp/hdmi_in_use", 1) || config.BOOT.FACTORY_RESET) {
        return;
    }

    progress_onscreen = 1;
    lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);
}

void ui_common_handle_vol() {
    if (read_int_from_file("/tmp/hdmi_in_use", 1) || config.BOOT.FACTORY_RESET) {
        return;
    }

    progress_onscreen = 2;
    lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);
}

void ui_common_handle_idle() {
    if (file_exist("/tmp/hdmi_do_refresh")) {
        remove("/tmp/hdmi_do_refresh");

        lv_obj_invalidate(ui_pnlHeader);
        lv_obj_invalidate(ui_pnlContent);
        lv_obj_invalidate(ui_pnlFooter);

        lv_obj_invalidate(ui_screen);
        lv_refr_now(NULL);
    }

    if (file_exist("/tmp/mux_blank")) {
        lv_obj_set_style_bg_opa(ui_blank, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(ui_blank);
    } else {
        if (lv_obj_get_style_bg_opa(ui_blank, LV_PART_MAIN | LV_STATE_DEFAULT) > 0) {
            lv_obj_set_style_bg_opa(ui_blank, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_move_background(ui_blank);
        }
    }

    lv_task_handler();
}

lv_obj_t *create_header_glyph(lv_obj_t *parent, struct theme_config *theme) {
    lv_obj_t *ui_glyph;
    ui_glyph = lv_img_create(parent);
    lv_obj_set_width(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_glyph, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_left(ui_glyph, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_glyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_glyph, theme->FONT.HEADER_ICON_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_glyph, theme->FONT.HEADER_ICON_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    return ui_glyph;
}

lv_obj_t *create_footer_glyph(lv_obj_t *parent, struct theme_config *theme, char *glyph_name,
                              struct footer_glyph nav_footer_glyph) {
    lv_obj_t *ui_glyph;
    char footer_image_path[MAX_BUFFER_SIZE];
    char footer_image_embed[MAX_BUFFER_SIZE];

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    if ((snprintf(footer_image_path, sizeof(footer_image_path), "%s/%sglyph/footer/%s.png",
                  STORAGE_THEME, mux_dimension, glyph_name) >= 0 &&
         file_exist(footer_image_path)) ||
        (snprintf(footer_image_path, sizeof(footer_image_path), "%s/glyph/footer/%s.png",
                  STORAGE_THEME, glyph_name) >= 0 &&
         file_exist(footer_image_path)) ||
        (snprintf(footer_image_path, sizeof(footer_image_path), "%s/%sglyph/footer/%s.png",
                  INTERNAL_THEME, mux_dimension, glyph_name) >= 0 &&
         file_exist(footer_image_path)) ||
        (snprintf(footer_image_path, sizeof(footer_image_path), "%s/glyph/footer/%s.png",
                  INTERNAL_THEME, glyph_name) >= 0 &&
         file_exist(footer_image_path))) {

        int written = snprintf(footer_image_embed, sizeof(footer_image_embed), "M:%s", footer_image_path);
        if (written < 0 || (size_t) written >= sizeof(footer_image_embed)) return NULL;
    }

    ui_glyph = lv_img_create(parent);
    lv_obj_set_width(ui_glyph, LV_SIZE_CONTENT);
    if (file_exist(footer_image_path) && nav_footer_glyph.GLYPH_ALPHA > 0) lv_img_set_src(ui_glyph, footer_image_embed);
    lv_obj_set_style_img_opa(ui_glyph, nav_footer_glyph.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_glyph, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_height(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_glyph, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_left(ui_glyph, theme->NAV.SPACING, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_glyph, theme->FONT.FOOTER_ICON_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_glyph, theme->FONT.FOOTER_ICON_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(ui_glyph, lv_color_hex(nav_footer_glyph.GLYPH), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_glyph, nav_footer_glyph.GLYPH_RECOLOUR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (nav_footer_glyph.GLYPH_ALPHA == 0) lv_obj_set_width(ui_glyph, 0);
    lv_obj_add_flag(ui_glyph, LV_OBJ_FLAG_HIDDEN);
    return ui_glyph;
}

lv_obj_t *create_footer_text(lv_obj_t *parent, struct theme_config *theme, uint32_t text_color, int16_t text_alpha) {
    lv_obj_t *ui_lblNavText = lv_label_create(parent);
    lv_obj_set_width(ui_lblNavText, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblNavText, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblNavText, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblNavText, "");
    lv_label_set_recolor(ui_lblNavText, "true");
    lv_obj_set_style_text_color(ui_lblNavText, lv_color_hex(text_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblNavText, text_alpha, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblNavText, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblNavText, theme->NAV.SPACING, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblNavText, theme->FONT.FOOTER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblNavText, theme->FONT.FOOTER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (text_alpha == 0) lv_obj_set_width(ui_lblNavText, 0);
    lv_obj_add_flag(ui_lblNavText, LV_OBJ_FLAG_HIDDEN);
    return ui_lblNavText;
}

int load_glyph(const char *theme_base, const char *mux_dimension, const char *glyph_folder,
               const char *glyph_name, char *image_path, size_t image_size) {
    return (snprintf(image_path, image_size, "%s/%sglyph/%s/%s.png", theme_base,
                     mux_dimension, glyph_folder, glyph_name) >= 0 &&
            file_exist(image_path)) ||
           (snprintf(image_path, image_size, "%s/glyph/%s/%s.png", theme_base,
                     glyph_folder, glyph_name) >= 0 &&
            file_exist(image_path));
}

int generate_image_embed(const char *base_path, const char *dimension, const char *glyph_folder, const char *glyph_name,
                         char *image_path, size_t path_size, char *image_embed, size_t embed_size) {
    if (load_glyph(base_path, dimension, glyph_folder, glyph_name, image_path, path_size)) {
        int written = snprintf(image_embed, embed_size, "M:%s", image_path);
        if (written < 0 || (size_t) written >= embed_size) return 0;
        return 1;
    }
    return 0;
}

void update_glyph(lv_obj_t *ui_img, const char *glyph_folder, const char *glyph_name) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));
    if (generate_image_embed(STORAGE_THEME, mux_dimension, glyph_folder, glyph_name, image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed)) ||
        generate_image_embed(INTERNAL_THEME, mux_dimension, glyph_folder, glyph_name, image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed))) {
        if (file_exist(image_path)) {
            lv_img_set_src(ui_img, image_embed);
        }
    }
}

void update_battery_capacity(lv_obj_t *ui_staCapacity, struct theme_config *theme) {
    char *battery_glyph_name = get_capacity();
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    char mux_dimension[15];

    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    if (str_startswith(battery_glyph_name, "capacity_charging_")) {
        lv_obj_set_style_img_recolor(ui_staCapacity, lv_color_hex(theme->STATUS.BATTERY.ACTIVE),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_staCapacity, theme->STATUS.BATTERY.ACTIVE_ALPHA,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (battery_capacity <= 15) {
        lv_obj_set_style_img_recolor(ui_staCapacity, lv_color_hex(theme->STATUS.BATTERY.LOW),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_staCapacity, theme->STATUS.BATTERY.LOW_ALPHA,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_img_recolor(ui_staCapacity, lv_color_hex(theme->STATUS.BATTERY.NORMAL),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_staCapacity, theme->STATUS.BATTERY.NORMAL_ALPHA,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (generate_image_embed(STORAGE_THEME, mux_dimension, "header", battery_glyph_name, image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed)) ||
        generate_image_embed(INTERNAL_THEME, mux_dimension, "header", battery_glyph_name, image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed))) {
        if (file_exist(image_path)) {
            lv_img_set_src(ui_staCapacity, image_embed);
        }
    }
}

void update_bluetooth_status(lv_obj_t *ui_staBluetooth, struct theme_config *theme) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    char mux_dimension[15];

    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    if (generate_image_embed(STORAGE_THEME, mux_dimension, "header", "bluetooth", image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed)) ||
        generate_image_embed(INTERNAL_THEME, mux_dimension, "header", "bluetooth", image_path,
                             sizeof(image_path), image_embed, sizeof(image_embed))) {
        if (file_exist(image_path)) {
            lv_img_set_src(ui_staBluetooth, image_embed);
        }
    }
}

void update_network_status(lv_obj_t *ui_staNetwork, struct theme_config *theme, int force_glyph) {
    struct {
        lv_color_t color;
        lv_opa_t alpha;
        const char *status;
    } status_style;

    if (force_glyph == 1 || (force_glyph == 0 && device.DEVICE.HAS_NETWORK && is_network_connected())) {
        status_style.color = lv_color_hex(theme->STATUS.NETWORK.ACTIVE);
        status_style.alpha = theme->STATUS.NETWORK.ACTIVE_ALPHA;
        status_style.status = "active";
    } else {
        status_style.color = lv_color_hex(theme->STATUS.NETWORK.NORMAL);
        status_style.alpha = theme->STATUS.NETWORK.NORMAL_ALPHA;
        status_style.status = "normal";
    }

    lv_obj_set_style_img_recolor(ui_staNetwork, status_style.color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_staNetwork, status_style.alpha, LV_PART_MAIN | LV_STATE_DEFAULT);

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    char network_status_filename[20];
    snprintf(network_status_filename, sizeof(network_status_filename), "network_%s", status_style.status);

    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    if ((generate_image_embed(STORAGE_THEME, mux_dimension, "header", network_status_filename, image_path,
                              sizeof(image_path), image_embed, sizeof(image_embed)) ||
         generate_image_embed(INTERNAL_THEME, mux_dimension, "header", network_status_filename, image_path,
                              sizeof(image_path), image_embed, sizeof(image_embed))) &&
        file_exist(image_path)) {
        lv_img_set_src(ui_staNetwork, image_embed);
    }
}

void toast_message(const char *msg, uint32_t delay, uint32_t fade_duration) {
    lv_label_set_text(ui_lblMessage, msg);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui_pnlMessage, LV_OPA_COVER, 0);

    if (delay <= 0 || fade_duration <= 0) return;

    lv_anim_del(ui_pnlMessage, NULL);
    lv_obj_fade_out(ui_pnlMessage, fade_duration, delay);
}

void fade_label(lv_obj_t *ui_lbl, const char *msg, uint32_t delay, uint32_t fade_duration) {
    lv_label_set_text(ui_lbl, msg);
    lv_obj_clear_flag(ui_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui_lbl, LV_OPA_COVER, 0);

    if (delay <= 0 || fade_duration <= 0) return;

    lv_anim_del(ui_lbl, NULL);
    lv_obj_fade_out(ui_lbl, fade_duration, delay);
}

void adjust_panel_priority(lv_obj_t *panels[], size_t num_panels) {
    for (size_t i = 0; i < num_panels; i++) {
        lv_obj_move_foreground(panels[i]);
    }
}

int adjust_wallpaper_element(lv_group_t *ui_group, int starter_image, int wall_type) {
    if (config.BOOT.FACTORY_RESET) {
        char mux_dimension[15];
        char init_wall[MAX_BUFFER_SIZE];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));
        snprintf(init_wall, sizeof(init_wall), "M:%s/%simage/wall/default.png", INTERNAL_THEME, mux_dimension);
        lv_img_set_src(ui_imgWall, init_wall);
    } else {
        load_wallpaper(ui_screen, ui_group, ui_pnlWall, ui_imgWall, wall_type);
    }

    static char static_image[MAX_BUFFER_SIZE];
    snprintf(static_image, sizeof(static_image), "%s",
             load_static_image(ui_screen, ui_group, wall_type));

    if (strlen(static_image) > 0) {
        LOG_INFO(mux_module, "Loading Static Image: %s", static_image);

        switch (theme.MISC.STATIC_ALIGNMENT) {
            case 0: // Bottom + Front
                lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                lv_obj_move_foreground(ui_pnlBox);
                break;
            case 1: // Middle + Front
                lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
                lv_obj_move_foreground(ui_pnlBox);
                break;
            case 2: // Top + Front
                lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
                lv_obj_move_foreground(ui_pnlBox);
                break;
            case 3: // Fullscreen + Behind
                lv_obj_set_y(ui_pnlBox, 0);
                lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                lv_obj_set_align(ui_imgBox, LV_ALIGN_CENTER);
                lv_obj_move_background(ui_pnlBox);
                lv_obj_move_background(ui_pnlWall);
                break;
            case 4: // Fullscreen + Front
                lv_obj_set_y(ui_pnlBox, 0);
                lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                lv_obj_set_align(ui_imgBox, LV_ALIGN_CENTER);
                lv_obj_move_foreground(ui_pnlBox);
                break;
        }

        lv_img_set_src(ui_imgBox, static_image);
    } else {
        if (!starter_image) {
            lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            return 0; // Reset back for static image loading
        }
    }

    return 1;
}

void fade_to_black(lv_obj_t *ui_screen) {
    lv_obj_t *black = lv_obj_create(ui_screen);

    lv_obj_set_width(black, device.MUX.WIDTH);
    lv_obj_set_height(black, device.MUX.HEIGHT);

    lv_obj_set_style_bg_color(black, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(black, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_center(black);
    lv_obj_move_foreground(black);

    unload_image_animation();
    for (unsigned int i = 0; i <= 255; i += 25) {
        lv_obj_set_style_bg_opa(black, i, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_task_handler();
        usleep(128);
    }
}

void fade_from_black(lv_obj_t *ui_black) {
    for (unsigned int i = 255; i >= 1; i -= 25) {
        lv_obj_set_style_bg_opa(ui_black, i, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_task_handler();
        usleep(128);
    }
    lv_obj_del_async(ui_black);
}

void create_grid_panel(struct theme_config *theme, int item_count) {
    int row_count = item_count / theme->GRID.COLUMN_COUNT + 1;
    lv_coord_t *col_dsc = malloc((theme->GRID.COLUMN_COUNT + 1) * sizeof(lv_coord_t));
    lv_coord_t *row_dsc = malloc((row_count + 1) * sizeof(lv_coord_t));

    for (int i = 0; i < theme->GRID.COLUMN_COUNT; i++) {
        col_dsc[i] = theme->GRID.COLUMN_WIDTH;
    }
    col_dsc[theme->GRID.COLUMN_COUNT] = LV_GRID_TEMPLATE_LAST;

    for (int i = 0; i < row_count; i++) {
        row_dsc[i] = theme->GRID.ROW_HEIGHT;
    }
    row_dsc[row_count] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_style_grid_column_dsc_array(ui_pnlGrid, col_dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(ui_pnlGrid, row_dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ui_pnlGrid, theme->GRID.COLUMN_COUNT * theme->GRID.COLUMN_WIDTH,
                    theme->GRID.ROW_COUNT * theme->GRID.ROW_HEIGHT);
    //add padding to the bottom to make sure grid panel scrolls correctly
    lv_obj_set_style_pad_bottom(ui_pnlGrid, (row_count - theme->GRID.ROW_COUNT + 1) * theme->GRID.ROW_HEIGHT,
                                LV_PART_MAIN);
    lv_obj_set_x(ui_pnlGrid, theme->GRID.LOCATION_X);
    lv_obj_set_y(ui_pnlGrid, theme->GRID.LOCATION_Y);
    lv_obj_set_layout(ui_pnlGrid, LV_LAYOUT_GRID);
    lv_obj_set_style_bg_color(ui_pnlGrid, lv_color_hex(theme->GRID.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlGrid, theme->GRID.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scroll_dir(ui_pnlGrid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_pnlGrid, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_snap_y(ui_pnlGrid, LV_SCROLL_SNAP_NONE);

    lv_obj_clear_flag(ui_lblGridCurrentItem, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.ALIGNMENT,
                 theme->GRID.CURRENT_ITEM_LABEL.OFFSET_X, theme->GRID.CURRENT_ITEM_LABEL.OFFSET_Y);
    lv_label_set_long_mode(ui_lblGridCurrentItem, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALIGNMENT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_width(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.WIDTH == 0 ? LV_SIZE_CONTENT
                                                                                      : theme->GRID.CURRENT_ITEM_LABEL.WIDTH);
    lv_obj_set_height(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.HEIGHT == 0 ? LV_SIZE_CONTENT
                                                                                        : theme->GRID.CURRENT_ITEM_LABEL.HEIGHT);
    lv_obj_set_style_radius(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.RADIUS,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.BORDER_WIDTH,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_lblGridCurrentItem, lv_color_hex(theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.BACKGROUND_ALPHA,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblGridCurrentItem, lv_color_hex(theme->GRID.CURRENT_ITEM_LABEL.BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.BORDER_ALPHA,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_lblGridCurrentItem, lv_color_hex(theme->GRID.CURRENT_ITEM_LABEL.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_ALPHA,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_LINE_SPACING,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_LEFT,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_RIGHT,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_TOP,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblGridCurrentItem, theme->GRID.CURRENT_ITEM_LABEL.TEXT_PADDING_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
}

void grid_item_focus_event_cb(lv_event_t *e) {
    lv_obj_t *cell_pnl = lv_event_get_target(e);
    uint32_t child_cnt = lv_obj_get_child_cnt(cell_pnl);
    if (child_cnt == 0) {
        // Panel has no children (maybe being deleted)
        return;
    }

    lv_obj_t *cell_image_focused = lv_obj_get_child(cell_pnl, child_cnt - 1);
    if (!cell_image_focused) return;    

    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_obj_set_style_img_opa(cell_image_focused, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
        lv_obj_set_style_img_opa(cell_image_focused, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void create_grid_item(struct theme_config *theme, lv_obj_t *cell_pnl, lv_obj_t *cell_label, lv_obj_t *cell_image,
                      int16_t col, int16_t row,
                      char *item_image_path, char *item_image_focused_path, char *item_text) {

    lv_obj_set_width(cell_pnl, theme->GRID.CELL.WIDTH);
    lv_obj_set_height(cell_pnl, theme->GRID.CELL.HEIGHT);
    lv_obj_set_style_radius(cell_pnl, theme->GRID.CELL.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cell_pnl, theme->GRID.CELL.BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_shadow_width(&style, theme->GRID.CELL.SHADOW_WIDTH);
    lv_style_set_shadow_color(&style, lv_color_hex(theme->GRID.CELL.SHADOW));
    lv_style_set_shadow_ofs_x(&style, theme->GRID.CELL.SHADOW_X_OFFSET);
    lv_style_set_shadow_ofs_y(&style, theme->GRID.CELL.SHADOW_Y_OFFSET);
    lv_obj_add_style(cell_pnl, &style, 0);

    lv_obj_set_style_bg_color(cell_pnl, lv_color_hex(theme->GRID.CELL_DEFAULT.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cell_pnl, theme->GRID.CELL_DEFAULT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(cell_pnl, lv_color_hex(theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_COLOR),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(cell_pnl, theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_START,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(cell_pnl, theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_STOP,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(cell_pnl, theme->GRID.CELL_DEFAULT.BACKGROUND_GRADIENT_DIRECTION,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cell_pnl, lv_color_hex(theme->GRID.CELL_DEFAULT.BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cell_pnl, theme->GRID.CELL_DEFAULT.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(cell_label, lv_color_hex(theme->GRID.CELL_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(cell_label, theme->GRID.CELL_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(cell_label, theme->GRID.CELL.TEXT_LINE_SPACING, LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_set_style_img_opa(cell_image, theme->GRID.CELL_DEFAULT.IMAGE_ALPHA,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(cell_image, lv_color_hex(theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(cell_image, theme->GRID.CELL_DEFAULT.IMAGE_RECOLOUR_ALPHA,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(cell_pnl, lv_color_hex(theme->GRID.CELL_FOCUS.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(cell_pnl, theme->GRID.CELL_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(cell_pnl, lv_color_hex(theme->GRID.CELL_FOCUS.BORDER),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_grad_color(cell_pnl, lv_color_hex(theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_COLOR),
                                   LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_main_stop(cell_pnl, theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_START,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_grad_stop(cell_pnl, theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_STOP,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_grad_dir(cell_pnl, theme->GRID.CELL_FOCUS.BACKGROUND_GRADIENT_DIRECTION,
                                 LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(cell_pnl, theme->GRID.CELL_FOCUS.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(cell_label, lv_color_hex(theme->GRID.CELL_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(cell_label, theme->GRID.CELL_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);


    lv_obj_set_style_img_opa(cell_image, theme->GRID.CELL_FOCUS.IMAGE_ALPHA,
                             LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_img_recolor(cell_image, lv_color_hex(theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR),
                                 LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_img_recolor_opa(cell_image, theme->GRID.CELL_FOCUS.IMAGE_RECOLOUR_ALPHA,
                                     LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_grid_cell(cell_pnl, theme->GRID.CELL.COLUMN_ALIGN, col, 1, theme->GRID.CELL.ROW_ALIGN, row, 1);

    if (file_exist(item_image_path)) {
        char grid_image[MAX_BUFFER_SIZE];
        snprintf(grid_image, sizeof(grid_image), "M:%s", item_image_path);
        lv_img_set_src(cell_image, grid_image);
        if (theme->GRID.CELL_DEFAULT.TEXT_ALPHA == 0 && theme->GRID.CELL_FOCUS.TEXT_ALPHA == 0) {
            lv_obj_align(cell_image, LV_ALIGN_CENTER, 0, theme->GRID.CELL.IMAGE_PADDING_TOP);
        } else {
            lv_obj_align(cell_image, LV_ALIGN_TOP_MID, 0, theme->GRID.CELL.IMAGE_PADDING_TOP);
        }
    }

    if (file_exist(item_image_focused_path)) {
        char grid_image_focused[MAX_BUFFER_SIZE];
        snprintf(grid_image_focused, sizeof(grid_image_focused), "M:%s", item_image_focused_path);
        lv_obj_t *cell_image_focused = lv_img_create(cell_pnl);
        lv_img_set_src(cell_image_focused, grid_image_focused);
        if (theme->GRID.CELL_DEFAULT.TEXT_ALPHA == 0 && theme->GRID.CELL_FOCUS.TEXT_ALPHA == 0) {
            lv_obj_align(cell_image_focused, LV_ALIGN_CENTER, 0, theme->GRID.CELL.IMAGE_PADDING_TOP);
        } else {
            lv_obj_align(cell_image_focused, LV_ALIGN_TOP_MID, 0, theme->GRID.CELL.IMAGE_PADDING_TOP);
        }
        lv_obj_set_style_img_opa(cell_image_focused, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(cell_pnl, grid_item_focus_event_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(cell_pnl, grid_item_focus_event_cb, LV_EVENT_DEFOCUSED, NULL);
    }

    lv_obj_set_width(cell_label, theme->GRID.CELL.WIDTH - (theme->GRID.CELL.TEXT_PADDING_SIDE * 2));
    lv_obj_set_height(cell_label, LV_SIZE_CONTENT);
    lv_label_set_text(cell_label, item_text);
    lv_label_set_long_mode(cell_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(cell_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_align(cell_label, LV_ALIGN_BOTTOM_MID, 0, -theme->GRID.CELL.TEXT_PADDING_BOTTOM);
}

void scroll_help_content(int direction, bool page_down) {
    if (lv_obj_get_content_height(ui_lblHelpContent) <= lv_obj_get_content_height(ui_pnlHelpContent)) return;

    int line_height = lv_font_get_line_height(lv_obj_get_style_text_font(ui_lblHelpContent, LV_PART_MAIN));
    int line_space = lv_obj_get_style_text_line_space(ui_lblHelpContent, LV_PART_MAIN);
    int total_line_height = line_height + line_space;
    if (page_down) {
        int lines_per_page = lv_obj_get_content_height(ui_pnlHelpContent) / total_line_height;
        total_line_height = lines_per_page * total_line_height;
    }

    int scroll_y = lv_obj_get_scroll_y(ui_pnlHelpContent);

    if (scroll_y - (total_line_height * direction) < 0) {
        lv_obj_scroll_to_y(ui_pnlHelpContent, 0, LV_ANIM_ON);
    } else if (scroll_y - (total_line_height * direction) >=
               lv_obj_get_content_height(ui_lblHelpContent) - lv_obj_get_content_height(ui_pnlHelpContent)) {
        lv_obj_scroll_to_y(ui_pnlHelpContent, lv_obj_get_content_height(ui_lblHelpContent), LV_ANIM_ON);
    } else {
        lv_obj_scroll_by(ui_pnlHelpContent, 0, total_line_height * direction, LV_ANIM_ON);
    }
}
