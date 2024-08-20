#pragma once

void ui_common_screen_init(struct theme_config *theme, struct mux_device *device, const char *title);
extern lv_obj_t * ui_screen;
extern lv_obj_t * ui_pnlWall;
extern lv_obj_t * ui_imgWall;
extern lv_obj_t * ui_pnlContent;
extern lv_obj_t * ui_pnlBox;
extern lv_obj_t * ui_imgBox;
extern lv_obj_t * ui_pnlHeader;
extern lv_obj_t * ui_lblDatetime;
extern lv_obj_t * ui_lblTitle;
extern lv_obj_t * ui_conGlyphs;
extern lv_obj_t * ui_staBluetooth;
extern lv_obj_t * ui_staNetwork;
extern lv_obj_t * ui_staCapacity;
extern lv_obj_t * ui_pnlFooter;
extern lv_obj_t * ui_lblNavAGlyph;
extern lv_obj_t * ui_lblNavA;
extern lv_obj_t * ui_lblNavBGlyph;
extern lv_obj_t * ui_lblNavB;
extern lv_obj_t * ui_lblNavCGlyph;
extern lv_obj_t * ui_lblNavC;
extern lv_obj_t * ui_lblNavXGlyph;
extern lv_obj_t * ui_lblNavX;
extern lv_obj_t * ui_lblNavYGlyph;
extern lv_obj_t * ui_lblNavY;
extern lv_obj_t * ui_lblNavZGlyph;
extern lv_obj_t * ui_lblNavZ;
extern lv_obj_t * ui_lblNavMenuGlyph;
extern lv_obj_t * ui_lblNavMenu;
extern lv_obj_t * ui_lblScreenMessage;
extern lv_obj_t * ui_pnlMessage;
extern lv_obj_t * ui_lblMessage;
extern lv_obj_t * ui_pnlHelp;
extern lv_obj_t * ui_pnlHelpMessage;
extern lv_obj_t * ui_lblHelpHeader;
extern lv_obj_t * ui_lblHelpContent;
extern lv_obj_t * ui_pnlHelpExtra;
extern lv_obj_t * ui_lblPreviewHeaderGlyph;
extern lv_obj_t * ui_lblPreviewHeader;
extern lv_obj_t * ui_pnlHelpPreview;
extern lv_obj_t * ui_lblHelpPreviewHeader;
extern lv_obj_t * ui_pnlHelpPreviewImage;
extern lv_obj_t * ui_imgHelpPreviewImage;
extern lv_obj_t * ui_pnlHelpPreviewInfo;
extern lv_obj_t * ui_lblHelpPreviewInfoGlyph;
extern lv_obj_t * ui_lblHelpPreviewInfoMessage;
extern lv_obj_t * ui_pnlProgressBrightness;
extern lv_obj_t * ui_icoProgressBrightness;
extern lv_obj_t * ui_barProgressBrightness;
extern lv_obj_t * ui_pnlProgressVolume;
extern lv_obj_t * ui_icoProgressVolume;
extern lv_obj_t * ui_barProgressVolume;
extern lv_obj_t * ui____initial_actions0;

LV_IMG_DECLARE(ui_img_nothing_png);    // assets/nothing.png
LV_IMG_DECLARE(ui_img_muoskofi_png);    // assets/muoskofi.png


LV_FONT_DECLARE(ui_font_Awesome);
LV_FONT_DECLARE(ui_font_AwesomeBig);
LV_FONT_DECLARE(ui_font_AwesomeBrand);
LV_FONT_DECLARE(ui_font_AwesomeBrandSmall);
LV_FONT_DECLARE(ui_font_AwesomeSmall);
LV_FONT_DECLARE(ui_font_Gamepad);
LV_FONT_DECLARE(ui_font_GamepadNav);
LV_FONT_DECLARE(ui_font_JGS_Ascii);
LV_FONT_DECLARE(ui_font_NotoSans);
LV_FONT_DECLARE(ui_font_NotoSansBig);
LV_FONT_DECLARE(ui_font_NotoSansSmall);
