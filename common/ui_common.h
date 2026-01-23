#pragma once

extern lv_timer_t *toast_timer;
extern lv_timer_t *counter_timer;

struct theme_config;
struct mux_device;
struct mux_lang;
struct footer_glyph;

struct help_msg {
    lv_obj_t *element;
    char *message;
};

struct nav_bar {
    lv_obj_t *item;
    char *text;
    int ui_check;
};

void apply_gradient_to_ui_screen(lv_obj_t *ui_screen, struct theme_config *theme, struct mux_device *device);

void init_ui_common_screen(struct theme_config *theme, struct mux_device *device,
                           struct mux_lang *lang, const char *title);

void init_ui_item_counter(struct theme_config *theme);

void ui_common_handle_bright_up();

void ui_common_handle_bright_down();

void ui_common_handle_volume_up();

void ui_common_handle_volume_down();

void ui_common_handle_idle();

lv_obj_t *create_header_glyph(lv_obj_t *parent, struct theme_config *theme);

lv_obj_t *create_footer_glyph(lv_obj_t *parent, struct theme_config *theme, char *glyph_name,
                              struct footer_glyph nav_footer_glyph, int16_t add_hide_flag);

lv_obj_t *create_footer_text(lv_obj_t *parent, struct theme_config *theme, uint32_t text_color, int16_t text_alpha,
                             int16_t add_hide_flag);

int generate_image_embed(const char *dimension, const char *glyph_folder, const char *glyph_name,
                         char *image_path, size_t path_size, char *image_embed, size_t embed_size);

void update_glyph(lv_obj_t *ui_img, const char *glyph_folder, const char *glyph_name);

void update_battery_capacity(lv_obj_t *ui_staCapacity, struct theme_config *theme);

void update_bluetooth_status(lv_obj_t *ui_staBluetooth, struct theme_config *theme);

void update_network_status(lv_obj_t *ui_staNetwork, struct theme_config *theme, int force_glyph);

void toast_message(const char *msg, uint32_t delay);

void counter_message(lv_obj_t *ui_lblCounter, const char *msg, uint32_t delay);

void adjust_panel_priority(lv_obj_t *panels[]);

int adjust_wallpaper_element(lv_group_t *ui_group, int starter_image, int wall_type);

void fade_to_black(lv_obj_t *ui_screen);

void fade_from_black(lv_obj_t *ui_black);

void create_grid_panel(struct theme_config *theme, int item_count);

void create_grid_item(struct theme_config *theme, lv_obj_t *cell_pnl, lv_obj_t *cell_label, lv_obj_t *cell_image,
                      int16_t col, int16_t row, char *item_image_path, char *item_image_focused_path, char *item_text);

void scroll_help_content(int direction, bool page_down);

void gen_help(lv_obj_t *element_focused, struct help_msg *help_messages, int num_messages);

extern lv_obj_t *ui_screen_container;
extern lv_obj_t *ui_screen_temp;
extern lv_obj_t *ui_screen;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlContent;
extern lv_obj_t *ui_pnlGrid;
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
extern lv_obj_t *ui_lblGridCurrentItem;
extern lv_obj_t *ui_lblNavLRGlyph;
extern lv_obj_t *ui_lblNavLR;
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
extern lv_obj_t *ui_pnlHelpContent;
extern lv_obj_t *ui_lblHelpContent;
extern lv_obj_t *ui_pnlHelpExtra;
extern lv_obj_t *ui_lblHelpNavUDGlyph;
extern lv_obj_t *ui_lblHelpNavUD;
extern lv_obj_t *ui_lblHelpNavBGlyph;
extern lv_obj_t *ui_lblHelpNavB;
extern lv_obj_t *ui_lblPreviewHeaderGlyph;
extern lv_obj_t *ui_lblPreviewHeader;
extern lv_obj_t *ui_pnlHelpPreview;
extern lv_obj_t *ui_lblHelpPreviewHeader;
extern lv_obj_t *ui_pnlHelpPreviewImage;
extern lv_obj_t *ui_imgHelpPreviewImage;
extern lv_obj_t *ui_pnlHelpPreviewInfo;
extern lv_obj_t *ui_lblHelpPreviewInfoGlyph;
extern lv_obj_t *ui_lblHelpPreviewInfoMessage;
extern lv_obj_t *ui_lblHelpPreviewNavBGlyph;
extern lv_obj_t *ui_lblHelpPreviewNavB;
extern lv_obj_t *ui_pnlProgressBrightness;
extern lv_obj_t *ui_icoProgressBrightness;
extern lv_obj_t *ui_barProgressBrightness;
extern lv_obj_t *ui_pnlProgressVolume;
extern lv_obj_t *ui_icoProgressVolume;
extern lv_obj_t *ui_barProgressVolume;
extern lv_obj_t *ui_pnlProgress;
extern lv_obj_t *ui_barProgress;
extern lv_obj_t *ui_lblProgress;
extern lv_obj_t *ui_lblCounter_explore;

LV_IMG_DECLARE(ui_img_nothing_png)
LV_IMG_DECLARE(ui_img_muoskofi_png)
