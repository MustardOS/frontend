#include "common.h"
#include "config.h"
#include "theme.h"
#include "device.h"
#include "ui_common.h"

lv_obj_t *ui_screen;
lv_obj_t *ui_pnlWall;
lv_obj_t *ui_imgWall;
lv_obj_t *ui_pnlContent;
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
lv_obj_t *ui____initial_actions0;

void ui_common_screen_init(struct theme_config *theme, struct mux_device *device, const char *title) {
    ui_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen, lv_color_hex(theme->SYSTEM.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen, theme->SYSTEM.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_screen, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
    lv_obj_set_width(ui_imgWall, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgWall, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgWall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgWall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgWall, LV_OBJ_FLAG_SCROLLABLE);

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
    lv_obj_set_height(ui_pnlBox, 400);
    lv_obj_set_align(ui_pnlBox, LV_ALIGN_CENTER);
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
    lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
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
    lv_obj_set_style_pad_left(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblDatetime = lv_label_create(ui_pnlHeader);
    lv_obj_set_height(ui_lblDatetime, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblDatetime, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui_lblDatetime, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblDatetime, "");
    lv_obj_set_style_text_color(ui_lblDatetime, lv_color_hex(theme->DATETIME.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblDatetime, theme->DATETIME.ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblDatetime, theme->DATETIME.PADDING_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblDatetime, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblDatetime, theme->FONT.HEADER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblDatetime, theme->FONT.HEADER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblTitle = lv_label_create(ui_pnlHeader);
    lv_obj_set_width(ui_lblTitle, 630);
    lv_obj_set_height(ui_lblTitle, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblTitle, LV_ALIGN_TOP_MID);
    lv_label_set_long_mode(ui_lblTitle, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblTitle, title);
    lv_obj_set_style_text_color(ui_lblTitle, lv_color_hex(theme->HEADER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblTitle, theme->HEADER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblTitle, theme->FONT.HEADER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblTitle, theme->FONT.HEADER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_conGlyphs = lv_obj_create(ui_pnlHeader);
    lv_obj_remove_style_all(ui_conGlyphs);
    lv_obj_set_width(ui_conGlyphs, 110);
    lv_obj_set_height(ui_conGlyphs, theme->HEADER.HEIGHT);
    lv_obj_set_align(ui_conGlyphs, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_flex_flow(ui_conGlyphs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_conGlyphs, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conGlyphs, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(ui_conGlyphs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conGlyphs, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conGlyphs, theme->STATUS.PADDING_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conGlyphs, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conGlyphs, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_staBluetooth = create_header_glyph(ui_conGlyphs, theme);
    //update_bluetooth_status(ui_staBluetooth, theme);

    ui_staNetwork = create_header_glyph(ui_conGlyphs, theme);
    update_network_status(ui_staNetwork, theme);

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
    lv_obj_set_style_pad_left(ui_pnlFooter, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
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
            e_align = LV_FLEX_ALIGN_START;
            break;
    }
    lv_obj_set_style_flex_main_place(ui_pnlFooter, e_align, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (config.SETTINGS.ADVANCED.SWAP) {
        ui_lblNavAGlyph = create_footer_glyph(ui_pnlFooter, theme, "b", theme->NAV.A);
        ui_lblNavA = create_footer_text(ui_pnlFooter, theme, theme->NAV.A.TEXT, theme->NAV.A.TEXT_ALPHA);
        ui_lblNavBGlyph = create_footer_glyph(ui_pnlFooter, theme, "a", theme->NAV.B);
    } else {
        ui_lblNavAGlyph = create_footer_glyph(ui_pnlFooter, theme, "a", theme->NAV.A);
        ui_lblNavA = create_footer_text(ui_pnlFooter, theme, theme->NAV.A.TEXT, theme->NAV.A.TEXT_ALPHA);
        ui_lblNavBGlyph = create_footer_glyph(ui_pnlFooter, theme, "b", theme->NAV.B);
    }

    ui_lblNavB = create_footer_text(ui_pnlFooter, theme, theme->NAV.B.TEXT, theme->NAV.B.TEXT_ALPHA);

    ui_lblNavCGlyph = create_footer_glyph(ui_pnlFooter, theme, "c", theme->NAV.C);
    ui_lblNavC = create_footer_text(ui_pnlFooter, theme, theme->NAV.C.TEXT, theme->NAV.C.TEXT_ALPHA);

    ui_lblNavXGlyph = create_footer_glyph(ui_pnlFooter, theme, "x", theme->NAV.X);
    ui_lblNavX = create_footer_text(ui_pnlFooter, theme, theme->NAV.X.TEXT, theme->NAV.X.TEXT_ALPHA);

    ui_lblNavYGlyph = create_footer_glyph(ui_pnlFooter, theme, "y", theme->NAV.Y);
    ui_lblNavY = create_footer_text(ui_pnlFooter, theme, theme->NAV.Y.TEXT, theme->NAV.Y.TEXT_ALPHA);

    ui_lblNavZGlyph = create_footer_glyph(ui_pnlFooter, theme, "z", theme->NAV.Z);
    ui_lblNavZ = create_footer_text(ui_pnlFooter, theme, theme->NAV.Z.TEXT, theme->NAV.Z.TEXT_ALPHA);

    ui_lblNavMenuGlyph = create_footer_glyph(ui_pnlFooter, theme, "menu", theme->NAV.MENU);
    ui_lblNavMenu = create_footer_text(ui_pnlFooter, theme, theme->NAV.MENU.TEXT, theme->NAV.MENU.TEXT_ALPHA);

    ui_lblScreenMessage = lv_label_create(ui_screen);
    lv_obj_set_width(ui_lblScreenMessage, device->MUX.WIDTH);
    lv_obj_set_height(ui_lblScreenMessage, 28);
    lv_obj_set_align(ui_lblScreenMessage, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblScreenMessage, "No Content Found...");
    lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
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
    lv_obj_set_width(ui_pnlMessage, 615);
    lv_obj_set_height(ui_pnlMessage, 42);
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
    lv_obj_set_width(ui_lblMessage, 600);
    lv_obj_set_height(ui_lblMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblMessage, -220);
    lv_obj_set_y(ui_lblMessage, -205);
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
    lv_obj_set_width(ui_pnlHelpMessage, 550);
    lv_obj_set_height(ui_pnlHelpMessage, 385);
    lv_obj_set_align(ui_pnlHelpMessage, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlHelpMessage, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlHelpMessage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpMessage, theme->HELP.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpMessage, lv_color_hex(theme->HELP.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpMessage, theme->HELP.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlHelpMessage, lv_color_hex(theme->HELP.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlHelpMessage, theme->HELP.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpMessage, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpMessage, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlHelpMessage, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlHelpMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpHeader = lv_label_create(ui_pnlHelpMessage);
    lv_obj_set_width(ui_lblHelpHeader, 515);
    lv_obj_set_height(ui_lblHelpHeader, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHelpHeader, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblHelpHeader, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblHelpHeader, "");
    lv_obj_set_style_text_color(ui_lblHelpHeader, lv_color_hex(theme->HELP.TITLE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpHeader, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpContent = lv_label_create(ui_pnlHelpMessage);
    lv_obj_set_width(ui_lblHelpContent, 515);
    lv_obj_set_height(ui_lblHelpContent, 250);
    lv_obj_set_align(ui_lblHelpContent, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblHelpContent, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(ui_lblHelpContent, "");
    lv_obj_set_style_text_color(ui_lblHelpContent, lv_color_hex(theme->HELP.CONTENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpContent, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblHelpContent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblHelpContent, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpExtra = lv_obj_create(ui_pnlHelpMessage);
    lv_obj_set_width(ui_pnlHelpExtra, 515);
    lv_obj_set_height(ui_pnlHelpExtra, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_pnlHelpExtra, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlHelpExtra, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlHelpExtra, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(ui_pnlHelpExtra, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                       LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE |
                                       LV_OBJ_FLAG_SCROLL_ELASTIC |
                                       LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_radius(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpExtra, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpExtra, LV_BORDER_SIDE_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlHelpExtra, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlHelpExtra, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlHelpExtra, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlHelpExtra, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblPreviewHeaderGlyph = lv_label_create(ui_pnlHelpExtra);
    lv_obj_set_width(ui_lblPreviewHeaderGlyph, 28);
    lv_obj_set_height(ui_lblPreviewHeaderGlyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblPreviewHeaderGlyph, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblPreviewHeaderGlyph, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "⇓");
    lv_obj_set_style_text_color(ui_lblPreviewHeaderGlyph, lv_color_hex(theme->NAV.A.GLYPH),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblPreviewHeaderGlyph, theme->NAV.A.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblPreviewHeaderGlyph, &ui_font_GamepadNav, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblPreviewHeaderGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblPreviewHeaderGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblPreviewHeaderGlyph, theme->FONT.FOOTER_ICON_PAD_TOP,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblPreviewHeaderGlyph, theme->FONT.FOOTER_ICON_PAD_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblPreviewHeader = lv_label_create(ui_pnlHelpExtra);
    lv_obj_set_width(ui_lblPreviewHeader, 480);
    lv_obj_set_height(ui_lblPreviewHeader, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblPreviewHeader, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblPreviewHeader, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_lblPreviewHeader, "Switch to Preview Image");
    lv_obj_set_style_text_color(ui_lblPreviewHeader, lv_color_hex(theme->NAV.A.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblPreviewHeader, theme->NAV.A.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblPreviewHeader, theme->FONT.FOOTER_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblPreviewHeader, theme->FONT.FOOTER_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpPreview = lv_obj_create(ui_pnlHelp);
    lv_obj_set_width(ui_pnlHelpPreview, 550);
    lv_obj_set_height(ui_pnlHelpPreview, 385);
    lv_obj_set_align(ui_pnlHelpPreview, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlHelpPreview, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlHelpPreview, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreview, theme->HELP.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreview, lv_color_hex(theme->HELP.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreview, theme->HELP.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_pnlHelpPreview, lv_color_hex(theme->HELP.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_pnlHelpPreview, theme->HELP.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpPreview, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpPreview, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlHelpPreview, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlHelpPreview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpPreviewHeader = lv_label_create(ui_pnlHelpPreview);
    lv_obj_set_width(ui_lblHelpPreviewHeader, 515);
    lv_obj_set_height(ui_lblHelpPreviewHeader, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHelpPreviewHeader, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblHelpPreviewHeader, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_lblHelpPreviewHeader, "");
    lv_obj_set_style_text_color(ui_lblHelpPreviewHeader, lv_color_hex(theme->HELP.TITLE),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpPreviewHeader, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlHelpPreviewImage = lv_obj_create(ui_pnlHelpPreview);
    lv_obj_set_width(ui_pnlHelpPreviewImage, 515);
    lv_obj_set_height(ui_pnlHelpPreviewImage, 250);
    lv_obj_set_align(ui_pnlHelpPreviewImage, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlHelpPreviewImage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreviewImage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlHelpPreviewImage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_imgHelpPreviewImage = lv_img_create(ui_pnlHelpPreviewImage);
    lv_img_set_src(ui_imgHelpPreviewImage, &ui_img_nothing_png);
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
    lv_obj_set_width(ui_pnlHelpPreviewInfo, 515);
    lv_obj_set_height(ui_pnlHelpPreviewInfo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_pnlHelpPreviewInfo, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlHelpPreviewInfo, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_pnlHelpPreviewInfo, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(ui_pnlHelpPreviewInfo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlHelpPreviewInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlHelpPreviewInfo, LV_BORDER_SIDE_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlHelpPreviewInfo, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlHelpPreviewInfo, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlHelpPreviewInfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlHelpPreviewInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpPreviewInfoGlyph = lv_label_create(ui_pnlHelpPreviewInfo);
    lv_obj_set_width(ui_lblHelpPreviewInfoGlyph, 28);
    lv_obj_set_height(ui_lblHelpPreviewInfoGlyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHelpPreviewInfoGlyph, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblHelpPreviewInfoGlyph, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_lblHelpPreviewInfoGlyph, "⇓");
    lv_obj_set_style_text_color(ui_lblHelpPreviewInfoGlyph, lv_color_hex(theme->NAV.A.GLYPH),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpPreviewInfoGlyph, theme->NAV.A.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHelpPreviewInfoGlyph, &ui_font_GamepadNav, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHelpPreviewInfoGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHelpPreviewInfoGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHelpPreviewInfoGlyph, theme->FONT.FOOTER_ICON_PAD_TOP,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHelpPreviewInfoGlyph, theme->FONT.FOOTER_ICON_PAD_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblHelpPreviewInfoMessage = lv_label_create(ui_pnlHelpPreviewInfo);
    lv_obj_set_width(ui_lblHelpPreviewInfoMessage, 480);
    lv_obj_set_height(ui_lblHelpPreviewInfoMessage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHelpPreviewInfoMessage, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblHelpPreviewInfoMessage, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_lblHelpPreviewInfoMessage, "Switch to Information");
    lv_obj_set_style_text_color(ui_lblHelpPreviewInfoMessage, lv_color_hex(theme->NAV.A.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHelpPreviewInfoMessage, theme->NAV.A.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHelpPreviewInfoMessage, theme->FONT.FOOTER_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHelpPreviewInfoMessage, theme->FONT.FOOTER_PAD_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlProgressBrightness = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlProgressBrightness, 615);
    lv_obj_set_height(ui_pnlProgressBrightness, 42);
    lv_obj_set_x(ui_pnlProgressBrightness, 0);
    lv_obj_set_y(ui_pnlProgressBrightness, -47);
    lv_obj_set_align(ui_pnlProgressBrightness, LV_ALIGN_BOTTOM_MID);
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

    ui_icoProgressBrightness = lv_label_create(ui_pnlProgressBrightness);
    lv_obj_set_width(ui_icoProgressBrightness, 18);
    lv_obj_set_height(ui_icoProgressBrightness, 28);
    lv_obj_set_x(ui_icoProgressBrightness, -220);
    lv_obj_set_y(ui_icoProgressBrightness, -205);
    lv_obj_set_align(ui_icoProgressBrightness, LV_ALIGN_CENTER);
    lv_label_set_text(ui_icoProgressBrightness, "");
    lv_label_set_recolor(ui_icoProgressBrightness, "true");
    lv_obj_set_style_text_color(ui_icoProgressBrightness, lv_color_hex(theme->BAR.ICON),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_icoProgressBrightness, theme->BAR.ICON_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_icoProgressBrightness, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_icoProgressBrightness, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_icoProgressBrightness, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_icoProgressBrightness, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_icoProgressBrightness, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_barProgressBrightness = lv_bar_create(ui_pnlProgressBrightness);
    lv_bar_set_value(ui_barProgressBrightness, 50, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_barProgressBrightness, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_barProgressBrightness, 550);
    lv_obj_set_height(ui_barProgressBrightness, 16);
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
    lv_obj_set_width(ui_pnlProgressVolume, 615);
    lv_obj_set_height(ui_pnlProgressVolume, 42);
    lv_obj_set_x(ui_pnlProgressVolume, 0);
    lv_obj_set_y(ui_pnlProgressVolume, -47);
    lv_obj_set_align(ui_pnlProgressVolume, LV_ALIGN_BOTTOM_MID);
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

    ui_icoProgressVolume = lv_label_create(ui_pnlProgressVolume);
    lv_obj_set_width(ui_icoProgressVolume, 18);
    lv_obj_set_height(ui_icoProgressVolume, 28);
    lv_obj_set_x(ui_icoProgressVolume, -220);
    lv_obj_set_y(ui_icoProgressVolume, -205);
    lv_obj_set_align(ui_icoProgressVolume, LV_ALIGN_CENTER);
    lv_label_set_text(ui_icoProgressVolume, "");
    lv_label_set_recolor(ui_icoProgressVolume, "true");
    lv_obj_set_style_text_color(ui_icoProgressVolume, lv_color_hex(theme->BAR.ICON), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_icoProgressVolume, theme->BAR.ICON_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_icoProgressVolume, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_icoProgressVolume, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_icoProgressVolume, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_icoProgressVolume, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_icoProgressVolume, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_barProgressVolume = lv_bar_create(ui_pnlProgressVolume);
    lv_bar_set_value(ui_barProgressVolume, 50, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_barProgressVolume, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_barProgressVolume, 550);
    lv_obj_set_height(ui_barProgressVolume, 16);
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

    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_screen);
}

lv_obj_t *create_header_glyph(lv_obj_t *parent, struct theme_config *theme) {
    lv_obj_t * ui_glyph;
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
    lv_obj_t * ui_glyph;

    char footer_image_path[MAX_BUFFER_SIZE];
    char footer_image_embed[MAX_BUFFER_SIZE];
    if (snprintf(footer_image_path, sizeof(footer_image_path), "%s/theme/active/glyph/footer/%s.png",
                 STORAGE_PATH, glyph_name) >= 0 && file_exist(footer_image_path)) {
        snprintf(footer_image_embed, sizeof(footer_image_embed), "M:%s/theme/active/glyph/footer/%s.png",
                 STORAGE_PATH, glyph_name);
    } else if (snprintf(footer_image_path, sizeof(footer_image_path), "%s/theme/glyph/footer/%s.png",
                        INTERNAL_PATH, glyph_name) >= 0 &&
               file_exist(footer_image_path)) {
        snprintf(footer_image_embed, sizeof(footer_image_embed), "M:%s/theme/glyph/footer/%s.png",
                 INTERNAL_PATH, glyph_name);
    }

    ui_glyph = lv_img_create(parent);
    lv_obj_set_width(ui_glyph, LV_SIZE_CONTENT);
    if (file_exist(footer_image_path) && nav_footer_glyph.GLYPH_ALPHA > 0) lv_img_set_src(ui_glyph, footer_image_embed);
    lv_obj_set_style_img_opa(ui_glyph, nav_footer_glyph.GLYPH_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_glyph, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_height(ui_glyph, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_glyph, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_left(ui_glyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_glyph, theme->FONT.FOOTER_ICON_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_glyph, theme->FONT.FOOTER_ICON_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor(ui_glyph, lv_color_hex(nav_footer_glyph.GLYPH), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_glyph, nav_footer_glyph.GLYPH_RECOLOUR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (nav_footer_glyph.GLYPH_ALPHA == 0) lv_obj_set_width(ui_glyph, 0);
    return ui_glyph;
}

lv_obj_t *create_footer_text(lv_obj_t *parent, struct theme_config *theme, uint32_t text_color, int16_t text_alpha) {
    lv_obj_t * ui_lblNavText = lv_label_create(parent);
    lv_obj_set_width(ui_lblNavText, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblNavText, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblNavText, -220);
    lv_obj_set_y(ui_lblNavText, -205);
    lv_obj_set_align(ui_lblNavText, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblNavText, "");
    lv_label_set_recolor(ui_lblNavText, "true");
    lv_obj_set_style_text_color(ui_lblNavText, lv_color_hex(text_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblNavText, text_alpha, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblNavText, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblNavText, 9, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblNavText, theme->FONT.FOOTER_PAD_TOP * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblNavText, theme->FONT.FOOTER_PAD_BOTTOM * 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (text_alpha == 0) lv_obj_set_width(ui_lblNavText, 0);
    return ui_lblNavText;
}

void update_battery_capacity(lv_obj_t *ui_staCapacity, struct theme_config *theme) {
    char *battery_glyph_name = get_capacity();

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

    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    if (snprintf(image_path, sizeof(image_path), "%s/theme/active/glyph/header/%s.png",
                 STORAGE_PATH, battery_glyph_name) >= 0 && file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/active/glyph/header/%s.png",
                 STORAGE_PATH, battery_glyph_name);
    } else if (snprintf(image_path, sizeof(image_path), "%s/theme/glyph/header/%s.png",
                        INTERNAL_PATH, battery_glyph_name) >= 0 &&
               file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/glyph/header/%s.png",
                 INTERNAL_PATH, battery_glyph_name);
    }

    if (file_exist(image_path)) lv_img_set_src(ui_staCapacity, image_embed);
}

void update_bluetooth_status(lv_obj_t *ui_staBluetooth, struct theme_config *theme) {
    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    if (snprintf(image_path, sizeof(image_path), "%s/theme/active/glyph/header/bluetooth.png",
                 STORAGE_PATH) >= 0 && file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/active/glyph/header/bluetooth.png",
                 STORAGE_PATH);
    } else if (snprintf(image_path, sizeof(image_path), "%s/theme/glyph/header/bluetooth.png",
                        INTERNAL_PATH) >= 0 &&
               file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/glyph/header/bluetooth.png",
                 INTERNAL_PATH);
    }

    if (file_exist(image_path)) lv_img_set_src(ui_staBluetooth, image_embed);
}

void update_network_status(lv_obj_t *ui_staNetwork, struct theme_config *theme) {
    char *network_status;
    if (device.DEVICE.HAS_NETWORK && is_network_connected()) {
        network_status = "active";
        lv_obj_set_style_img_recolor(ui_staNetwork, lv_color_hex(theme->STATUS.NETWORK.ACTIVE),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_staNetwork, theme->STATUS.NETWORK.ACTIVE_ALPHA,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        network_status = "normal";
        lv_obj_set_style_img_recolor(ui_staNetwork, lv_color_hex(theme->STATUS.NETWORK.NORMAL),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_staNetwork, theme->STATUS.NETWORK.NORMAL_ALPHA,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];
    if (snprintf(image_path, sizeof(image_path), "%s/theme/active/glyph/header/network_%s.png",
                 STORAGE_PATH, network_status) >= 0 && file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/active/glyph/header/network_%s.png",
                 STORAGE_PATH, network_status);
    } else if (snprintf(image_path, sizeof(image_path), "%s/theme/glyph/header/network_%s.png",
                        INTERNAL_PATH, network_status) >= 0 &&
               file_exist(image_path)) {
        snprintf(image_embed, sizeof(image_embed), "M:%s/theme/glyph/header/network_%s.png",
                 INTERNAL_PATH, network_status);
    }

    if (file_exist(image_path)) lv_img_set_src(ui_staNetwork, image_embed);
}
