#pragma once

void ui_common_screen_init(struct theme_config *theme, struct mux_device *device, const char *title);

void ui_common_handle_bright();

void ui_common_handle_vol();

void ui_common_handle_idle();

lv_obj_t *create_header_glyph(lv_obj_t *parent, struct theme_config *theme);

lv_obj_t *create_footer_glyph(lv_obj_t *parent, struct theme_config *theme, char *glyph_name,
                              struct footer_glyph nav_footer_glyph);

lv_obj_t *create_footer_text(lv_obj_t *parent, struct theme_config *theme, uint32_t text_color, int16_t text_alpha);

void update_battery_capacity(lv_obj_t *ui_staCapacity, struct theme_config *theme);

void update_bluetooth_status(lv_obj_t *ui_staBluetooth, struct theme_config *theme);

void update_network_status(lv_obj_t *ui_staNetwork, struct theme_config *theme);

void toast_message(const char * msg, uint32_t delay, uint32_t fade_duration);

extern lv_obj_t *ui_screen;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlContent;
extern lv_obj_t *ui_pnlBox;
extern lv_obj_t *ui_imgBox;
extern lv_obj_t *ui_pnlHeader;
extern lv_obj_t *ui_lblDatetime;
extern lv_obj_t *ui_lblTitle;
extern lv_obj_t *ui_conGlyphs;
extern lv_obj_t *ui_staBluetooth;
extern lv_obj_t *ui_staNetwork;
extern lv_obj_t *ui_staCapacity;
extern lv_obj_t *ui_pnlFooter;
extern lv_obj_t *ui_lblNavAGlyph;
extern lv_obj_t *ui_lblNavA;
extern lv_obj_t *ui_lblNavBGlyph;
extern lv_obj_t *ui_lblNavB;
extern lv_obj_t *ui_lblNavCGlyph;
extern lv_obj_t *ui_lblNavC;
extern lv_obj_t *ui_lblNavXGlyph;
extern lv_obj_t *ui_lblNavX;
extern lv_obj_t *ui_lblNavYGlyph;
extern lv_obj_t *ui_lblNavY;
extern lv_obj_t *ui_lblNavZGlyph;
extern lv_obj_t *ui_lblNavZ;
extern lv_obj_t *ui_lblNavMenuGlyph;
extern lv_obj_t *ui_lblNavMenu;
extern lv_obj_t *ui_lblScreenMessage;
extern lv_obj_t *ui_pnlMessage;
extern lv_obj_t *ui_lblMessage;
extern lv_obj_t *ui_pnlHelp;
extern lv_obj_t *ui_pnlHelpMessage;
extern lv_obj_t *ui_lblHelpHeader;
extern lv_obj_t *ui_lblHelpContent;
extern lv_obj_t *ui_pnlHelpExtra;
extern lv_obj_t *ui_lblPreviewHeaderGlyph;
extern lv_obj_t *ui_lblPreviewHeader;
extern lv_obj_t *ui_pnlHelpPreview;
extern lv_obj_t *ui_lblHelpPreviewHeader;
extern lv_obj_t *ui_pnlHelpPreviewImage;
extern lv_obj_t *ui_imgHelpPreviewImage;
extern lv_obj_t *ui_pnlHelpPreviewInfo;
extern lv_obj_t *ui_lblHelpPreviewInfoGlyph;
extern lv_obj_t *ui_lblHelpPreviewInfoMessage;
extern lv_obj_t *ui_pnlProgressBrightness;
extern lv_obj_t *ui_icoProgressBrightness;
extern lv_obj_t *ui_barProgressBrightness;
extern lv_obj_t *ui_pnlProgressVolume;
extern lv_obj_t *ui_icoProgressVolume;
extern lv_obj_t *ui_barProgressVolume;

LV_IMG_DECLARE(ui_img_nothing_png);
LV_IMG_DECLARE(ui_img_muoskofi_png);
