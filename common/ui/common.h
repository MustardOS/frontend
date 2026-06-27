#pragma once

#include "../../lvgl/lvgl.h"
#include "../collection/common.h"

extern const lv_img_dsc_t ui_img_blank;

extern lv_timer_t *toast_timer;
extern lv_timer_t *counter_timer;

struct theme_config;
struct mux_device;
struct mux_lang;
struct footer_glyph;

struct help_msg {
    char *key;
    char *message;
};

struct nav_bar {
    lv_obj_t *item;
    char *text;
    int ui_check;
};

void apply_gradient_to_ui_screen(
    lv_obj_t *ui_screen, const struct theme_config *theme, const struct mux_device *device
);

void set_gradient_visible(int visible);

void ui_common_get_gradient_buffer(void **buf, int *w, int *h);

void fade_reset(void);

void fade_in_screen(void);

void fade_out_screen(void);

void init_ui_common_screen(
    const struct theme_config *theme, const struct mux_device *device, const struct mux_lang *lang, const char *title
);

void init_ui_item_counter(const struct theme_config *theme);

void ui_common_handle_bright_up();

void ui_common_handle_bright_down();

void ui_common_handle_volume_up();

void ui_common_handle_volume_down();

void ui_common_handle_idle();

lv_obj_t *create_header_glyph(lv_obj_t *parent, const struct theme_config *theme);

lv_obj_t *create_footer_glyph(
    lv_obj_t *parent, const struct theme_config *theme, const char *glyph_name, struct footer_glyph nav_footer_glyph,
    int16_t add_hide_flag
);

lv_obj_t *create_footer_text(
    lv_obj_t *parent, const struct theme_config *theme, uint32_t text_color, int16_t text_alpha, int16_t add_hide_flag
);

int generate_image_embed(
    const char *dimension, const char *glyph_folder, const char *glyph_name, char *image_path, size_t path_size,
    char *image_embed, size_t embed_size
);

void update_glyph(lv_obj_t *ui_img, const char *glyph_folder, const char *glyph_name);

void update_battery_capacity(lv_obj_t *ui_sta_capacity, const struct theme_config *theme);

void update_battery_percent_label(lv_obj_t *ui_label, const struct theme_config *theme);

void update_bluetooth_status(lv_obj_t *ui_sta_bluetooth, const struct theme_config *theme);

void update_network_status(lv_obj_t *ui_sta_network, const struct theme_config *theme, int force_glyph);

void toast_message(const char *msg, uint32_t delay);

void counter_message(lv_obj_t *ui_lbl_counter, const char *msg, uint32_t delay);

void adjust_panel_priority(lv_obj_t *panels[]);

int adjust_wallpaper_element(lv_group_t *ui_group, int starter_image, int wall_type);

void create_grid_panel(const struct theme_config *theme, int item_count);

void create_grid_item(
    const struct theme_config *theme, lv_obj_t *cell_pnl, lv_obj_t *cell_label, lv_obj_t *cell_image, int16_t col,
    int16_t row, char *item_image_path, char *item_image_focused_path, const char *item_text
);

void scroll_help_content(int direction, int page_down);

void gen_help(
    int current_index, const struct help_msg *help_messages, size_t msg_count, const lv_group_t *group,
    const content_item *items
);

void mu_img_no_shadow(lv_obj_t *img);

extern lv_obj_t *ui_screen_container;
extern lv_obj_t *ui_screen_temp;
extern lv_obj_t *ui_screen;
extern lv_obj_t *ui_pnl_wall;
extern lv_obj_t *ui_img_wall;
extern lv_obj_t *ui_pnl_content;
extern lv_obj_t *ui_pnl_grid;
extern lv_obj_t *ui_pnl_box;
extern lv_obj_t *ui_img_box;
extern lv_obj_t *ui_pnl_header;
extern lv_obj_t *ui_lbl_datetime;
extern lv_obj_t *ui_lbl_title;
extern lv_obj_t *ui_con_glyphs;
extern lv_obj_t *ui_sta_bluetooth;
extern lv_obj_t *ui_sta_network;
extern lv_obj_t *ui_lbl_battery_percent;
extern lv_obj_t *ui_pnl_footer;
extern lv_obj_t *ui_pnl_grid_current_item;
extern lv_obj_t *ui_lbl_grid_current_item;
extern lv_obj_t *ui_lbl_nav_lr_glyph;
extern lv_obj_t *ui_lbl_nav_lr;
extern lv_obj_t *ui_lbl_nav_a_glyph;
extern lv_obj_t *ui_lbl_nav_a;
extern lv_obj_t *ui_lbl_nav_b_glyph;
extern lv_obj_t *ui_lbl_nav_b;
extern lv_obj_t *ui_lbl_nav_c_glyph;
extern lv_obj_t *ui_lbl_nav_c;
extern lv_obj_t *ui_lbl_nav_x_glyph;
extern lv_obj_t *ui_lbl_nav_x;
extern lv_obj_t *ui_lbl_nav_y_glyph;
extern lv_obj_t *ui_lbl_nav_y;
extern lv_obj_t *ui_lbl_nav_z_glyph;
extern lv_obj_t *ui_lbl_nav_z;
extern lv_obj_t *ui_lbl_nav_menu_glyph;
extern lv_obj_t *ui_lbl_nav_menu;
extern lv_obj_t *ui_lbl_screen_message;
extern lv_obj_t *ui_pnl_help;
extern lv_obj_t *ui_pnl_help_message;
extern lv_obj_t *ui_lbl_help_header;
extern lv_obj_t *ui_pnl_help_content;
extern lv_obj_t *ui_lbl_help_content;
extern lv_obj_t *ui_pnl_help_extra;
extern lv_obj_t *ui_lbl_help_nav_ud_glyph;
extern lv_obj_t *ui_lbl_help_nav_ud;
extern lv_obj_t *ui_lbl_help_nav_b_glyph;
extern lv_obj_t *ui_lbl_help_nav_b;
extern lv_obj_t *ui_lbl_preview_header_glyph;
extern lv_obj_t *ui_lbl_preview_header;
extern lv_obj_t *ui_pnl_help_preview;
extern lv_obj_t *ui_lbl_help_preview_header;
extern lv_obj_t *ui_pnl_help_preview_image;
extern lv_obj_t *ui_img_help_preview_image;
extern lv_obj_t *ui_pnl_help_preview_info;
extern lv_obj_t *ui_lbl_help_preview_info_glyph;
extern lv_obj_t *ui_lbl_help_preview_info_message;
extern lv_obj_t *ui_lbl_help_preview_nav_b_glyph;
extern lv_obj_t *ui_lbl_help_preview_nav_b;
extern lv_obj_t *ui_pnl_progress_brightness;
extern lv_obj_t *ui_ico_progress_brightness;
extern lv_obj_t *ui_bar_progress_brightness;
extern lv_obj_t *ui_pnl_progress_volume;
extern lv_obj_t *ui_ico_progress_volume;
extern lv_obj_t *ui_bar_progress_volume;
extern lv_obj_t *ui_pnl_progress;
extern lv_obj_t *ui_bar_progress;
extern lv_obj_t *ui_lbl_progress;
extern lv_obj_t *ui_lbl_counter_explore;
